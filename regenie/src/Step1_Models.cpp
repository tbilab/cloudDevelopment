/*

   This file is part of the regenie software package.

   Copyright (c) 2020 Joelle Mbatchou & Jonathan Marchini

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

*/


#include "Regenie.hpp"
#include "Geno.hpp"
#include "Joint_Tests.hpp"
#include "Step1_Models.hpp"
#include "Step2_Models.hpp"
#include "Files.hpp"
#include "Pheno.hpp"
#include "Masks.hpp"
#include "Data.hpp"

using namespace std;
using namespace Eigen;
using namespace boost;


void fit_null_logistic(const int chrom, struct param* params, struct phenodt* pheno_data, struct ests* m_ests, mstream& sout) {

  sout << "   -fitting null logistic regression on binary phenotypes..." << flush;

  auto t1 = std::chrono::high_resolution_clock::now();

  int niter_cur;
  double dev_old, dev_new=0;
  ArrayXd Y1, score;
  ArrayXd betaold, betanew, etavec, pivec, wvec_sqrt, wvec, zvec, loco_offset;
  MatrixXd X1, XtW, XtWX;

  for(int i = 0; i < params->n_pheno; ++i ){

    Y1 = pheno_data->phenotypes_raw.col(i).array() * pheno_data->masked_indivs.col(i).array().cast<double>();
    X1 = (pheno_data->new_cov.array().colwise() * pheno_data->masked_indivs.col(i).array().cast<double>()).matrix();
    if(params->test_mode) loco_offset = m_ests->blups.col(i).array() * pheno_data->masked_indivs.col(i).array().cast<double>();

    // starting values
    pivec = ( 0.5 + Y1 ) / 2;
    etavec = (pheno_data->masked_indivs.col(i).array()).select( log(pivec/ (1-pivec)), 0);
    betaold = ArrayXd::Zero(pheno_data->new_cov.cols());
    betaold(0) = etavec.mean() - (params->test_mode? loco_offset.mean() : 0);
    // bug fix: don't count in masked samples
    dev_old = get_logist_dev(Y1, pivec, pheno_data->masked_indivs.col(i).array());

    niter_cur = 0;
    while(niter_cur++ < params->niter_max){

      // p*(1-p)
      wvec = (pheno_data->masked_indivs.col(i).array()).select(pivec * (1 - pivec), 0);

      // check for zeroes
      if( pheno_data->masked_indivs.col(i).array().select(wvec,1).minCoeff() == 0){
        sout << "\nERROR: Zeros occured in Var(Y) during logistic regression.\n";
        exit(EXIT_FAILURE);
      }

      wvec_sqrt = wvec.sqrt();
      XtW = X1.transpose() * wvec_sqrt.matrix().asDiagonal();
      XtWX = XtW * XtW.transpose();

      // working vector z = X*beta + (Y-p)/(p*(1-p))
      zvec = (pheno_data->masked_indivs.col(i).array()).select(etavec + (Y1 - pivec) / wvec, 0);
      if(params->test_mode) zvec -= loco_offset;

      // parameter estimate
      betanew = ( XtWX ).colPivHouseholderQr().solve( XtW * (wvec_sqrt * zvec).matrix()).array();

      // start step-halving
      for( int niter_search = 1; niter_search <= params->niter_max_line_search; niter_search++ ){

        etavec = ( X1 * betanew.matrix()).array();
        if(params->test_mode) etavec += loco_offset;
        pivec = (pheno_data->masked_indivs.col(i).array()).select(1 - 1 / (etavec.exp() + 1), 0.5) ;

        // bug fix: don't count in masked samples
        dev_new = get_logist_dev(Y1, pivec, pheno_data->masked_indivs.col(i).array());

        if( (pivec.minCoeff() > 0) && (pivec.maxCoeff() < 1) && isfinite(dev_new) ) break;

        // adjust step size
        betanew = (betaold + betanew) / 2;

      }

      score = X1.transpose() * (Y1 - pivec).matrix();

      // stopping criterion
      if( (score.abs().maxCoeff() < params->tol) ||
          (abs(dev_new - dev_old)/(0.1 + abs(dev_new)) < params->tol) )
        break;

      betaold = betanew;
      dev_old = dev_new;
    }

    // If didn't converge
    if(niter_cur > params->niter_max){
      sout << "ERROR: Logistic regression did not converge. (Perhaps increase --niter?)\n";
      exit(EXIT_FAILURE);
    } else if(( pheno_data->masked_indivs.col(i).array() && (pivec < params->numtol_eps || pivec > 1 - params->numtol_eps) ).count() > 0)
      sout << "\n     WARNING: Fitted probabilities numerically 0/1 occured (for phenotype #" << i+1 <<").";
    // sout << "Converged in "<< niter_cur << " iterations." << endl;


    if(params->test_mode){
      m_ests->Y_hat_p.col(i) = pivec.matrix() ;
      m_ests->Gamma_sqrt.col(i) = (pivec * (1 - pivec) ).sqrt().matrix();
      XtW = ( X1.array().colwise() * m_ests->Gamma_sqrt.col(i).array()).matrix();
      m_ests->Xt_Gamma_X_inv[i] = (XtW.transpose() * XtW).colPivHouseholderQr().inverse();
    } else m_ests->offset_logreg.col(i) = etavec;

    /*
    Files fstar;
    fstar.openForWrite("offsets.txt", sout);
    fstar << etavec;
    fstar.closeFile();
    */

  }


  sout << "done";
  auto t2 = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
  sout << " (" << duration.count() << "ms) "<< endl;

}

double get_logist_dev(const ArrayXd& Y, const ArrayXd& pi, const ArrayXb& mask){

  double dev = 0;

  for( int i = 0; i < Y.size(); i++){
    if(mask(i)) dev+= (Y(i) == 1) ? log(1/pi(i)) : log(1/(1-pi(i)));
  }

  return 2 * dev;
}


/////////////////////////////////////////////////
/////////////////////////////////////////////////
////          level 0 models
/////////////////////////////////////////////////
/////////////////////////////////////////////////

void ridge_level_0(const int block, struct in_files* files, struct param* params, struct filter* filters, struct ests* m_ests, struct geno_block* Gblock, struct phenodt* pheno_data, vector<snp>& snpinfo, struct ridgel0* l0, struct ridgel1* l1, vector<MatrixXb>& masked_in_folds, mstream& sout) {

  sout << "   -calc level 0 ridge..." << flush;
  auto t2 = std::chrono::high_resolution_clock::now();

  int bs = l0->GGt.rows();
  int block_eff = params->write_l0_pred ? 0 : block;
  uint32_t low = 0, high = 0;
  string op_name, out_pheno;
  ofstream ofile;

  MatrixXd ww1, ww2, ww3, beta, pred, vmat, dvec, Xout;
  MatrixXd ident_l0 = MatrixXd::Identity(bs, bs);
  MatrixXd p_sum = MatrixXd::Zero(params->n_ridge_l0, params->n_pheno);
  MatrixXd p_sum2 = MatrixXd::Zero(params->n_ridge_l0, params->n_pheno);

  if(!params->within_sample_l0 && params->print_block_betas) {
    for(int ph = 0; ph < params->n_pheno; ++ph )
      params->beta_print_out[ph] = MatrixXd::Zero(params->n_ridge_l0, bs);
  }

  uint32_t i_total = 0, cum_size_folds = 0;
  for(int i = 0; i < params->cv_folds; ++i ) {
    // assign masking within folds
    for(int j = 0; j < params->cv_sizes[i]; ++j) {
      masked_in_folds[i].row(j) = pheno_data->masked_indivs.row(i_total);
      i_total++;
    }

    ww1 = l0->GGt - l0->G_folds[i];
    SelfAdjointEigenSolver<MatrixXd> eig(ww1);
    vmat = eig.eigenvectors();
    dvec = eig.eigenvalues();
    //if(i == 0)sout << ww1 << endl;
    if(i>0) low +=  params->cv_sizes[i-1];
    high += params->cv_sizes[i];
    for(int j = 0; j < params->n_ridge_l0; ++j ) {
      ww2 = vmat.transpose() * (l0->GTY - l0->GtY[i]);
      //if(i == 0)sout << ww2 << endl;
      //if(i == 0)sout << "lambda[j] =" << params->lambda[j] << endl;
      //if(i == 0)sout << ww1.array() + params->lambda[j]*ident_l0.array() << endl;
      ww3 = (dvec.array() + params->lambda[j]).inverse().matrix().asDiagonal() * ww2;
      //if(i == 0)sout << beta * (ww1+ params->lambda[j]*ident_l0)<< endl;
      beta = vmat * ww3;

      // save beta for each phenotype (only when using out-of-sample pred)
      if(!params->within_sample_l0 && params->print_block_betas)
        for(int ph = 0; ph < params->n_pheno; ++ph ) {
          params->beta_print_out[ph].row(j) += beta.col(ph).transpose();
        }

      pred = beta.transpose() * Gblock->Gmat;
      //if(i == 0)sout << pred.rows() << " " << pred.cols() << endl;
      //if(i == 0)sout << beta << endl;
      if(params->within_sample_l0) {
        // center and scale predictions
        VectorXd p_mean, p_sd;
        p_mean = pred.array().rowwise().mean();
        //if(i == 0)sout << i << " " << p_mean << endl;
        pred.colwise() -= p_mean;
        p_sd = pred.rowwise().norm() / sqrt(filters->ind_in_analysis.cast<float>().sum() -1);
        //if(i == 0)sout << i << " " << p_sd << endl;
        pred.array().colwise() /= p_sd.array();
      } else {
        p_sum.row(j) += (pred.block(0, cum_size_folds, params->n_pheno, params->cv_sizes[i]).array() * masked_in_folds[i].transpose().array().cast<double>()).matrix().rowwise().sum();
        p_sum2.row(j) += (pred.block(0, cum_size_folds, params->n_pheno, params->cv_sizes[i]).array() * masked_in_folds[i].transpose().array().cast<double>()).matrix().rowwise().squaredNorm();
      }

      // store predictions
      uint32_t kk = 0, jj = 0;
      for(size_t k = 0; k < params->n_samples; ++k ) {
        if( (k < low) | (k >= high) ) {
          if (params->within_sample_l0) {
            for(int ph = 0; ph < params->n_pheno; ++ph ) {
              l1->pred_mat[ph][i](kk, block*params->n_ridge_l0 + j) = pred(ph, k);
              l1->pred_pheno[ph][i](kk, 0) = pheno_data->phenotypes(k, ph);
              if (params->binary_mode && (block == 0) && (j == 0) ) {
                l1->pred_pheno_raw[ph][i](kk, 0) = pheno_data->phenotypes_raw(k, ph);
                l1->pred_offset[ph][i](kk, 0) = m_ests->offset_logreg(k, ph);
              }
            }
          }
          kk+=1;
        } else {
          for(int ph = 0; ph < params->n_pheno; ++ph ) {
            l1->test_mat[ph][i](jj, block_eff * params->n_ridge_l0 + j) = pred(ph, k);
            l1->test_pheno[ph][i](jj, 0) = pheno_data->phenotypes(k, ph);
            if (params->binary_mode && (block == 0) && (j == 0) ) {
              l1->test_pheno_raw[ph][i](jj, 0) = pheno_data->phenotypes_raw(k, ph);
              l1->test_offset[ph][i](jj, 0) = m_ests->offset_logreg(k, ph);
            }
          }
          jj+=1;
        }
      }
    }
    cum_size_folds += params->cv_sizes[i];
  }

  // when using only out-of-sample predictions for level 1 input features, center and scale using the whole sample
  if(!params->within_sample_l0){
    for(int ph = 0; ph < params->n_pheno; ++ph ) {
      RowVectorXd p_mean, p_invsd;
      p_mean = p_sum.col(ph).transpose() / pheno_data->Neff(ph);
      p_invsd = sqrt( (pheno_data->Neff(ph) - 1) / (p_sum2.col(ph).transpose().array() - pheno_data->Neff(ph) * p_mean.array().square()) );

      // scale printed estimates by the sd
      if(params->print_block_betas){
        params->beta_print_out[ph].array().colwise() *= p_invsd.transpose().array();
      }

      if(params->write_l0_pred) Xout = MatrixXd(params->n_samples, params->n_ridge_l0);

      cum_size_folds = 0;
      for(int i = 0; i < params->cv_folds; ++i ) {
        l1->test_mat[ph][i].block(0, block_eff * params->n_ridge_l0, params->cv_sizes[i], params->n_ridge_l0).rowwise() -= p_mean;
        // mask missing
        l1->test_mat[ph][i].block(0, block_eff * params->n_ridge_l0, params->cv_sizes[i], params->n_ridge_l0).array().colwise() *= masked_in_folds[i].col(ph).array().cast<double>();
        l1->test_mat[ph][i].block(0, block_eff * params->n_ridge_l0, params->cv_sizes[i], params->n_ridge_l0).array().rowwise() *= p_invsd.array();

        if(params->write_l0_pred) {
          Xout.block(cum_size_folds, 0, params->cv_sizes[i], params->n_ridge_l0) = l1->test_mat[ph][i].block(0, block_eff * params->n_ridge_l0, params->cv_sizes[i], params->n_ridge_l0);
          cum_size_folds += params->cv_sizes[i];
        }
      }

      // write predictions to file if specified
      if(params->write_l0_pred) {
        write_l0_file(files->write_preds_files[ph].get(), Xout, sout);
        //if(block ==0 && ph == 0 ) sout << endl << "Out " << endl <<  Xout.block(0, 0, 3, 3) << endl;
      }

    }
  }

  // if printing betas to file (average over folds) [assume snp IDs are unique]
  //   -> separate file for each block (params->n_ridge_l0 rows & (2+bs) columns)
  if(!params->within_sample_l0 && params->print_block_betas) {
    op_name = files->out_file + "_block" + to_string(block+1) + ".betas";
    ofile.open(op_name.c_str());

    // Header: [TRAIT PARAM snpID1 ... snpIDk]
    ofile << "TRAIT PARAM " ;
    for(int i = 0; i < bs; ++i )
      ofile << snpinfo[params->print_snpcount++].ID << " ";
    ofile << endl;

    // Each line: [pheno# ridge# beta1 ... betak]
    for(int ph = 0; ph < params->n_pheno; ++ph ){
      params->beta_print_out[ph] /= params->cv_folds;
      for(int j = 0; j < params->n_ridge_l0; ++j ) {
        ofile << ph + 1 << " " <<  j + 1 << " ";
        for(int i = 0; i < bs; ++i )
          ofile << params->beta_print_out[ph](j,i) << " ";
        ofile << endl;
      }
    }
    ofile.close();
  }

  sout << "done";
  auto t3 = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2);
  sout << " (" << duration.count() << "ms) "<< endl;

}


void ridge_level_0_loocv(const int block, struct in_files* files, struct param* params, struct filter* filters, struct ests* m_ests, struct geno_block* Gblock, struct phenodt* pheno_data, vector<snp>& snpinfo, struct ridgel0* l0, struct ridgel1* l1, mstream& sout) {

  sout << "   -calc level 0 ridge..." << flush;
  auto t2 = std::chrono::high_resolution_clock::now();
  int bs = l0->GGt.rows();
  int block_eff = params->write_l0_pred ? 0 : block; // if writing to file
  string out_pheno;
  ofstream ofile;
  VectorXd z1, gvec;
  MatrixXd VtG, z2, pred, Xout;
  RowVectorXd p_mean, p_sd;

  /*
     if(bs > params->n_samples){
     sout << "ERROR: Block size must be smaller than the number of samples to perform LOOCV!";
     exit(EXIT_FAILURE);
     }
     */


  // make matrix of (eigen-value + lambda)^(-1)
  Map<RowVectorXd> Lmap(params->lambda.data(), params->n_ridge_l0);
  MatrixXd dl = l0->GGt_eig_val.asDiagonal() * MatrixXd::Ones(bs, params->n_ridge_l0);
  dl.rowwise() += Lmap;
  MatrixXd DL_inv = dl.array().inverse().matrix();

  uint64 max_bytes = params->chunk_mb * 1e6;
  // amount of RAM used < max_mb [ creating (bs * target_size) matrix ]
  int nchunk = ceil( params->cv_folds * bs * sizeof(double) * 1.0 / max_bytes );
  if (params->verbose) sout << nchunk << " chunks..." << flush;
  int chunk, size_chunk, target_size = params->cv_folds / nchunk;
  int j_start;

  for(chunk = 0; chunk < nchunk; ++chunk ) {
    size_chunk = chunk == nchunk - 1? params->cv_folds - target_size * chunk : target_size;
    j_start = chunk * target_size;

    VtG = l0->GGt_eig_vec.transpose() * Gblock->Gmat.block(0, j_start, bs, size_chunk);
    for(int i = 0; i < size_chunk; ++i ) {
      z1 = VtG.col(i);
      z2 = DL_inv.array().colwise() * z1.array();
      gvec = z2.transpose() * z1;
      pred = z2.transpose() * l0->Wmat - gvec * pheno_data->phenotypes.row(j_start + i);
      pred.array().colwise() /= 1 - gvec.array();
      for(int ph = 0; ph < params->n_pheno; ++ph )
        l1->test_mat_conc[ph].block(j_start + i, block_eff * params->n_ridge_l0, 1, params->n_ridge_l0) = pred.col(ph).transpose();
    }
  }

  // center and scale within the block
  for(int ph = 0; ph < params->n_pheno; ++ph ) {
    // mask missing first
    l1->test_mat_conc[ph].block(0, block_eff * params->n_ridge_l0, params->n_samples, params->n_ridge_l0).array().colwise() *= pheno_data->masked_indivs.col(ph).array().cast<double>();
    p_mean = l1->test_mat_conc[ph].block(0, block_eff * params->n_ridge_l0, params->n_samples, params->n_ridge_l0).colwise().sum() / pheno_data->Neff(ph);
    //if(i == 0)sout << i << " " << p_mean << endl;
    l1->test_mat_conc[ph].block(0, block_eff * params->n_ridge_l0, params->n_samples, params->n_ridge_l0).rowwise() -= p_mean;
    // mask missing again
    l1->test_mat_conc[ph].block(0, block_eff * params->n_ridge_l0, params->n_samples, params->n_ridge_l0).array().colwise() *= pheno_data->masked_indivs.col(ph).array().cast<double>();
    p_sd = l1->test_mat_conc[ph].block(0, block_eff * params->n_ridge_l0, params->n_samples, params->n_ridge_l0).colwise().norm() / sqrt(pheno_data->Neff(ph) -1);
    //if(i == 0)sout << i << " " << p_sd << endl;
    l1->test_mat_conc[ph].block(0, block_eff * params->n_ridge_l0, params->n_samples, params->n_ridge_l0).array().rowwise() /= p_sd.array();


    if(params->write_l0_pred) {
      Xout = l1->test_mat_conc[ph].block(0, 0, params->n_samples, params->n_ridge_l0);
      write_l0_file(files->write_preds_files[ph].get(), Xout, sout);
      //if(block < 2 && ph == 0 ) sout << endl << "Out " << endl <<  Xout.block(0, 0, 5, Xout.cols()) << endl;
    }

  }

  sout << "done";
  auto t3 = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2);
  sout << " (" << duration.count() << "ms) "<< endl;

}

void write_l0_file(ofstream* ofs, MatrixXd& Xout, mstream& sout){

  ofs->write( reinterpret_cast<char *> (&Xout(0,0)), Xout.size() * sizeof(double) );
  if( ofs->fail() ){
    sout << "ERROR: Cannot successfully write temporary level 0 predictions to disk\n";
    exit(EXIT_FAILURE);
  }

}


/////////////////////////////////////////////////
/////////////////////////////////////////////////
////          level 1 models
/////////////////////////////////////////////////
/////////////////////////////////////////////////
void set_mem_l1(struct in_files* files, struct param* params, struct filter* filters, struct ests* m_ests, struct geno_block* Gblock, struct phenodt* pheno_data, struct ridgel1* l1, vector<MatrixXb>& masked_in_folds, mstream& sout){ // when l0 was run in parallel

  // store pheno info for l1
  if(!params->use_loocv) {

    uint32_t low = 0, high = 0;
    uint32_t i_total = 0, cum_size_folds = 0;
    for(int i = 0; i < params->cv_folds; ++i ) {
      // assign masking within folds
      for(int j = 0; j < params->cv_sizes[i]; ++j) {
        masked_in_folds[i].row(j) = pheno_data->masked_indivs.row(i_total);
        i_total++;
      }

      // set lower and upper index bounds for fold
      if(i>0) low +=  params->cv_sizes[i-1];
      high += params->cv_sizes[i];

      // store predictions
      uint32_t jj = 0;
      for(size_t k = 0; k < params->n_samples; ++k ) {
        if( (k >= low) && (k < high) ) {
          for(int ph = 0; ph < params->n_pheno; ++ph ) {
            l1->test_pheno[ph][i](jj, 0) = pheno_data->phenotypes(k, ph);
            if (params->binary_mode) {
              l1->test_pheno_raw[ph][i](jj, 0) = pheno_data->phenotypes_raw(k, ph);
              l1->test_offset[ph][i](jj, 0) = m_ests->offset_logreg(k, ph);
            }
          }
          jj++;
        }
      }
      cum_size_folds += params->cv_sizes[i];
    }

  }

}

void ridge_level_1(struct in_files* files, struct param* params, struct ridgel1* l1, mstream& sout) {

  sout << endl << " Level 1 ridge..." << endl << flush;

  int bs_l1 = params->total_n_block * params->n_ridge_l0;
  int ph_eff;
  string in_pheno;
  ifstream infile;

  MatrixXd X1, X2, beta_l1, p1, vmat, dvec, dl_inv;
  VectorXd VtX2;
  MatrixXd XtX_sum, XtY_sum;
  MatrixXd ident_l1 = MatrixXd::Identity(bs_l1,bs_l1);
  Map<RowVectorXd> Lmap(params->tau.data(), params->n_ridge_l1);

  // to compute Rsq and MSE of predictions
  for (int i = 0; i < 5; i++)
    l1->cumsum_values[i].setZero(params->n_pheno, params->n_ridge_l1);

  for(int ph = 0; ph < params->n_pheno; ++ph ) {
    sout << "   -on phenotype " << ph+1 <<" (" << files->pheno_names[ph] << ")..." << flush;
    auto ts1 = std::chrono::high_resolution_clock::now();
    ph_eff = params->write_l0_pred ? 0 : ph;

    // read in level 0 predictions from file
    if(params->write_l0_pred){

      // allocate memory
      if(ph == 0) {
        for( int i = 0; i < params->cv_folds; ++i )
          l1->test_mat[ph_eff][i] = MatrixXd::Zero(params->cv_sizes[i], bs_l1);
      }

      read_l0(ph, ph_eff, files, params, l1, sout);
      //if(ph == 0) sout << endl << "In:\n" << l1->test_mat[ph_eff][0].block(0,0,5,6) << endl;
    }

    // compute XtX and Xty for each fold and cum. sum using test_mat's
    if (!params->within_sample_l0){
      XtX_sum.setZero(bs_l1, bs_l1);
      XtY_sum.setZero(bs_l1, 1);
      for( int i = 0; i < params->cv_folds; ++i ) {
        l1->X_folds[i] = l1->test_mat[ph_eff][i].transpose() * l1->test_mat[ph_eff][i];
        l1->XtY[i]     = l1->test_mat[ph_eff][i].transpose() * l1->test_pheno[ph][i];
        XtX_sum += l1->X_folds[i];
        XtY_sum += l1->XtY[i];
      }
    }
    for(int i = 0; i < params->cv_folds; ++i ) {

      // use either in-sample or out-of-sample predictions
      if (params->within_sample_l0) {
        X1 = l1->pred_mat[ph][i].transpose() * l1->pred_mat[ph][i];
        X2 = l1->pred_mat[ph][i].transpose() * l1->pred_pheno[ph][i];
      } else{
        X1 = XtX_sum - l1->X_folds[i];
        X2 = XtY_sum - l1->XtY[i];
      }

      SelfAdjointEigenSolver<MatrixXd> eigX1(X1);
      vmat = eigX1.eigenvectors();
      dvec = eigX1.eigenvalues();
      VtX2 = vmat.transpose() * X2;
      // compute solutions for all ridge parameters at once
      // p1 is Nfold x nridge_l1 matrix
      dl_inv = ( (dvec.asDiagonal() *  MatrixXd::Ones(bs_l1, params->n_ridge_l1)).rowwise() + Lmap).array().inverse().matrix();
      dl_inv.array().colwise() *= VtX2.array();
      beta_l1 = vmat * dl_inv;
      if(!params->within_sample_l0) l1->beta_hat_level_1[ph][i] = beta_l1;
      p1 = l1->test_mat[ph_eff][i] * beta_l1;

      l1->cumsum_values[0].row(ph) += p1.colwise().sum();
      l1->cumsum_values[1].row(ph).array() += l1->test_pheno[ph][i].array().sum();
      l1->cumsum_values[2].row(ph) += p1.array().square().matrix().colwise().sum();
      l1->cumsum_values[3].row(ph).array() += l1->test_pheno[ph][i].array().square().sum();
      l1->cumsum_values[4].row(ph) += (p1.array().colwise() * l1->test_pheno[ph][i].col(0).array()).matrix().colwise().sum() ;
    }

    sout << "done";
    auto ts2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(ts2 - ts1);
    sout << " (" << duration.count() << "ms) "<< endl;
  }

  sout << endl;
}


void ridge_level_1_loocv(struct in_files* files, struct param* params, struct phenodt* pheno_data, struct ridgel1* l1, mstream& sout) {

  sout << endl << " Level 1 ridge..." << flush;

  int bs_l1 = params->total_n_block * params->n_ridge_l0;
  int ph_eff;
  string in_pheno;
  ifstream infile;
  MatrixXd Xmat_chunk, Yvec_chunk, Z1, Z2, dl, dl_inv, xtx;
  VectorXd wvec, zvec;
  RowVectorXd calFactor, pred;

  for (int i = 0; i < 5; i++)
    l1->cumsum_values[i].setZero(params->n_pheno, params->n_ridge_l1);

  // make matrix of (eigen-values + tau)^(-1)
  Map<RowVectorXd> Lmap(params->tau.data(), params->n_ridge_l1);

  uint64 max_bytes = params->chunk_mb * 1e6;
  // amount of RAM used < max_mb [ creating (target_size * bs_l1) matrix ]
  int nchunk = ceil( params->cv_folds * bs_l1 * sizeof(double) * 1.0 / max_bytes );
  if (params->verbose) sout << nchunk << " chunks...";
  sout << endl;
  int chunk, size_chunk, target_size = params->cv_folds / nchunk;
  int j_start;

  for(int ph = 0; ph < params->n_pheno; ++ph ) {
    sout << "   -on phenotype " << ph+1 <<" (" << files->pheno_names[ph] <<")..." << flush;
    auto ts1 = std::chrono::high_resolution_clock::now();
    ph_eff = params->write_l0_pred ? 0 : ph;

    // read in level 0 predictions from file
    if(params->write_l0_pred){

      // allocate memory (re-use same matrix for all traits)
      if(ph == 0) l1->test_mat_conc[ph_eff] = MatrixXd::Zero(params->n_samples, bs_l1);

      read_l0(ph, ph_eff, files, params, l1, sout);
    }

    xtx = l1->test_mat_conc[ph_eff].transpose() * l1->test_mat_conc[ph_eff];
    SelfAdjointEigenSolver<MatrixXd> eigX(xtx);
    dl = eigX.eigenvalues().asDiagonal() * MatrixXd::Ones(bs_l1, params->n_ridge_l1);
    dl.rowwise() += Lmap;
    dl_inv = dl.array().inverse().matrix();

    zvec = l1->test_mat_conc[ph_eff].transpose() * pheno_data->phenotypes.col(ph);
    wvec = eigX.eigenvectors().transpose() * zvec;

    for(chunk = 0; chunk < nchunk; ++chunk ) {
      size_chunk = chunk == nchunk - 1? params->cv_folds - target_size * chunk : target_size;
      j_start = chunk * target_size;
      Xmat_chunk = l1->test_mat_conc[ph_eff].block(j_start, 0, size_chunk, bs_l1);
      Yvec_chunk = pheno_data->phenotypes.block(j_start, ph, size_chunk, 1);
      Z1 = (Xmat_chunk * eigX.eigenvectors()).transpose();

      for(int i = 0; i < size_chunk; ++i ) {
        Z2 = (dl_inv.array().colwise() * Z1.col(i).array()).matrix();
        calFactor = Z1.col(i).transpose() * Z2;
        pred = wvec.transpose() * Z2;
        pred -=  Yvec_chunk(i, 0) * calFactor;
        pred.array()  /= 1 - calFactor.array();
        //if( ph == 0) sout << pred.head(5) << endl;

        // compute mse and rsq
        l1->cumsum_values[0].row(ph) += pred; // Sx
        // Y is centered so Sy = 0
        l1->cumsum_values[2].row(ph) += pred.array().square().matrix(); // Sx2
        // Y is scaled so Sy2 = params->n_samples - ncov
        l1->cumsum_values[4].row(ph).array() += pred.array() * Yvec_chunk(i,0); // Sxy
      }
    }
    sout << "done";
    auto ts2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(ts2 - ts1);
    sout << " (" << duration.count() << "ms) "<< endl;
  }
  l1->cumsum_values[3].array().colwise() += pheno_data->Neff - params->ncov; // Sy2

  sout << endl;
}


// Logistic models
void ridge_logistic_level_1(struct in_files* files, struct param* params, struct phenodt* pheno_data, struct ridgel1* l1, vector<MatrixXb>& masked_in_folds, mstream& sout) {

  sout << endl << " Level 1 ridge with logistic regression..." << endl << flush;

  int niter_cur;
  int bs_l1 = params->total_n_block * params->n_ridge_l0;
  int ph_eff;
  string in_pheno;
  ifstream infile;

  ArrayXd Y1, W1, p1, score;
  ArrayXd betaold, etavec, pivec, wvec, zvec, betanew, etatest;
  MatrixXd X1, XtW, XtWX, XtWZ;
  l1->pheno_l1_not_converged = ArrayXb::Constant(params->n_pheno, false);
  MatrixXd ident_l1 = MatrixXd::Identity(bs_l1,bs_l1);

  for (int i = 0; i < 6; i++)
    l1->cumsum_values[i].setZero(params->n_pheno, params->n_ridge_l1);

  for(int ph = 0; ph < params->n_pheno; ++ph ) {
    sout << "   -on phenotype " << ph+1 <<" (" << files->pheno_names[ph] <<")..." << flush;
    auto ts1 = std::chrono::high_resolution_clock::now();
    ph_eff = params->write_l0_pred ? 0 : ph;

    // read in level 0 predictions from file
    if(params->write_l0_pred){

      // allocate memory
      if(ph == 0) {
        for( int i = 0; i < params->cv_folds; ++i )
          l1->test_mat[ph_eff][i] = MatrixXd::Zero(params->cv_sizes[i], bs_l1);
      }

      read_l0(ph, ph_eff, files, params, l1, sout);
    }

    for(int i = 0; i < params->cv_folds; ++i ) {
      if( l1->pheno_l1_not_converged(ph) ) break;

      if( params->within_sample_l0 ){
        X1 = l1->pred_mat[ph][i];
        Y1 = l1->pred_pheno_raw[ph][i];
        W1 = l1->pred_offset[ph][i];
      }

      // starting values for each trait
      betaold = betanew = ArrayXd::Zero(bs_l1);

      for(int j = 0; j < params->n_ridge_l1; ++j ) {
        if( l1->pheno_l1_not_converged(ph) ) break;


        niter_cur = 0;
        // use warm starts (i.e. set final beta of previous ridge param 
        // as initial beta for current ridge param)
        betaold = betanew;

        while(niter_cur++ < params->niter_max_ridge){

          if(params->within_sample_l0) {
            etavec = W1 + (X1 * betaold.matrix()).array();
            pivec = 1 - 1/(etavec.exp() + 1);
            wvec = pivec * (1 - pivec);
            // check none of the values are 0
            if( ( wvec == 0 ).count() > 0 ){
              sout << "ERROR: Zeros occured in Var(Y) during ridge logistic regression! (Try with --loocv)" << endl;
              l1->pheno_l1_not_converged(ph) = true;
              break;
            }
            zvec = (etavec - W1) + (Y1 - pivec) / wvec;
            XtW = X1.transpose() * wvec.matrix().asDiagonal();
            betanew = (XtW * X1 + params->tau[j] * ident_l1).colPivHouseholderQr().solve(XtW * zvec.matrix()).array();
            // get the score
            etavec = W1 + (X1 * betanew.matrix()).array();
            pivec = 1 - 1/(etavec.exp() + 1);
            score = (X1.transpose() * (Y1 - pivec).matrix()).array() - params->tau[j] * betanew;

          } else {

            XtWX = MatrixXd::Zero(bs_l1, bs_l1);
            XtWZ = MatrixXd::Zero(bs_l1, 1);

            for(int k = 0; k < params->cv_folds; ++k ) {
              if( k != i) {

                // get w=p*(1-p) and check none of the values are 0
                if( get_wvec(ph, etavec, pivec, wvec, betaold, masked_in_folds[k], l1->test_offset[ph][k], l1->test_mat[ph_eff][k], params->l1_ridge_eps) ){
                  sout << "ERROR: Zeros occured in Var(Y) during ridge logistic regression! (Try with --loocv)" << endl;
                  l1->pheno_l1_not_converged(ph) = true;
                  break;
                }

                zvec = (masked_in_folds[k].col(ph).array()).select((etavec - l1->test_offset[ph][k].array()) + (l1->test_pheno_raw[ph][k].array() - pivec) / wvec, 0);

                XtW = l1->test_mat[ph_eff][k].transpose() * wvec.matrix().asDiagonal();
                XtWX += XtW * l1->test_mat[ph_eff][k];
                XtWZ += XtW * zvec.matrix();
              }
            }
            if( l1->pheno_l1_not_converged(ph) ) break;

            betanew = ((XtWX + params->tau[j] * ident_l1).llt().solve(XtWZ)).array();

            // start step-halving
            for( int niter_search = 1; niter_search <= params->niter_max_line_search_ridge; niter_search++ ){

              bool invalid_wvec = false;

              for(int k = 0; k < params->cv_folds; ++k ) {
                if( k != i) {
                  // get w=p*(1-p) and check none of the values are 0
                  invalid_wvec = get_wvec(ph, etavec, pivec, wvec, betanew, masked_in_folds[k], l1->test_offset[ph][k], l1->test_mat[ph_eff][k], params->l1_ridge_eps);
                  if( invalid_wvec ) break; // do another halving
                }
              }

              if( !invalid_wvec ) break;

              // halve step size
              betanew = (betaold + betanew) / 2;

            }

            // compute score
            score = ArrayXd::Zero(bs_l1);
            for(int k = 0; k < params->cv_folds; ++k ) {
              if( k != i) {
                // get w=p*(1-p) and check none of the values are 0
                if( get_wvec(ph, etavec, pivec, wvec, betanew, masked_in_folds[k], l1->test_offset[ph][k], l1->test_mat[ph_eff][k], params->l1_ridge_eps) ){
                  sout << "ERROR: Zeros occured in Var(Y) during ridge logistic regression! (Try with --loocv)" << endl;
                  l1->pheno_l1_not_converged(ph) = true;
                  break;
                }
                score += (l1->test_mat[ph_eff][k].transpose() * masked_in_folds[k].col(ph).array().select(l1->test_pheno_raw[ph][k].array() - pivec, 0).matrix()).array();
              }
            }
            score -= params->tau[j] * betanew;


          }

          // stopping criterion
          if( (score.abs().maxCoeff() < params->l1_ridge_tol) || l1->pheno_l1_not_converged(ph)) break;

          betaold = betanew;
        }

        //cerr << "\nFold=" << i << " tau = " << params->tau[j] << " beta=" << betanew.matrix().transpose().array() << endl;
        //if(i==1) exit(EXIT_FAILURE);

        if(niter_cur > params->niter_max){
          sout << "WARNING: Penalized logistic regression did not converge! (Increase --niter)\n";
          l1->pheno_l1_not_converged(ph) = true;
          break;
        } else if(l1->pheno_l1_not_converged(ph)) break;
        //sout << "Converged in "<< niter_cur << " iterations. Score max = " << score.abs().maxCoeff() << endl;


        etatest = l1->test_offset[ph][i].array() + (l1->test_mat[ph_eff][i] * betanew.matrix()).array();
        p1 = (1 - 1/(etatest.exp() + 1));

        if(!params->within_sample_l0) l1->beta_hat_level_1[ph][i].col(j) = betanew;


        // compute mse
        for(int l = 0; l < params->cv_sizes[i]; l++){
          if(!masked_in_folds[i](l,ph)) continue;

          // if p is within eps of 0/1, set to eps/1-eps
          if( p1(l) < params->l1_ridge_eps ) p1(l) = params->l1_ridge_eps;
          else if( p1(l) > (1-params->l1_ridge_eps) ) p1(l) = 1 - params->l1_ridge_eps;

          l1->cumsum_values[0](ph,j) += p1(l); // Sx
          l1->cumsum_values[1](ph,j) += l1->test_pheno_raw[ph][i](l,0); // Sy
          l1->cumsum_values[2](ph,j) += p1(l) * p1(l); // Sx2
          l1->cumsum_values[3](ph,j) += l1->test_pheno_raw[ph][i](l,0) * l1->test_pheno_raw[ph][i](l,0); // Sy2
          l1->cumsum_values[4](ph,j) += p1(l) * l1->test_pheno_raw[ph][i](l,0); // Sxy
          l1->cumsum_values[5](ph,j) += compute_log_lik(l1->test_pheno_raw[ph][i](l,0), p1(l)); // LL
        }

      }
    }

    sout << "done";
    auto ts2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(ts2 - ts1);
    sout << " (" << duration.count() << "ms) "<< endl;
  }

  sout << endl;

}


void ridge_logistic_level_1_loocv(struct in_files* files, struct param* params, struct phenodt* pheno_data, struct ests* m_ests, struct ridgel1* l1, mstream& sout) {

  sout << endl << " Level 1 ridge with logistic regression..." << flush;

  int niter_cur;
  int bs_l1 = params->total_n_block * params->n_ridge_l0;
  int ph_eff;
  double v2, pred, p1;
  string in_pheno;
  ifstream infile;

  ArrayXd betaold, etavec, pivec, wvec, zvec, betanew, score;
  MatrixXd XtWX, XtWZ, V1, b_loo;
  LLT<MatrixXd> Hinv;
  l1->pheno_l1_not_converged = ArrayXb::Constant(params->n_pheno, false);

  MatrixXd ident_l1 = MatrixXd::Identity(bs_l1,bs_l1);
  for (int i = 0; i < 6; i++)
    l1->cumsum_values[i].setZero(params->n_pheno, params->n_ridge_l1);

  uint64 max_bytes = params->chunk_mb * 1e6;
  // amount of RAM used < max_mb [ creating (bs_l1 * target_size) matrix ]
  int nchunk = ceil( params->cv_folds * bs_l1 * sizeof(double) * 1.0 / max_bytes );
  if (params->verbose)
    sout << nchunk << " chunks..." << endl;
  else
    sout << endl;
  int chunk, size_chunk, target_size = params->cv_folds / nchunk;
  int j_start;

  for(int ph = 0; ph < params->n_pheno; ++ph ) {
    sout << "   -on phenotype " << ph+1 <<" (" << files->pheno_names[ph] <<")..." << flush;
    auto ts1 = std::chrono::high_resolution_clock::now();
    ph_eff = params->write_l0_pred ? 0 : ph;

    // read in level 0 predictions from file
    if(params->write_l0_pred){

      // allocate memory (re-use same matrix for all traits)
      if(ph == 0) l1->test_mat_conc[ph_eff] = MatrixXd::Zero(params->n_samples, bs_l1);

      read_l0(ph, ph_eff, files, params, l1, sout);
    }


    // starting values for each trait
    betaold = betanew = ArrayXd::Zero(bs_l1);

    for(int j = 0; j < params->n_ridge_l1; ++j ) {

      niter_cur = 0;
      // use warm starts (i.e. set final beta of previous ridge param 
      //   as initial beta for current ridge param)
      betaold = betanew;

      while(niter_cur++ < params->niter_max_ridge){

        // get w=p*(1-p) and check none of the values are 0
        if( get_wvec(ph, etavec, pivec, wvec, betaold, pheno_data->masked_indivs, m_ests->offset_logreg.col(ph), l1->test_mat_conc[ph_eff], params->l1_ridge_eps) ){
          sout << "ERROR: Zeros occured in Var(Y) during ridge logistic regression." << endl;
          l1->pheno_l1_not_converged(ph) = true;
          break;
        }

        zvec = (pheno_data->masked_indivs.col(ph).array()).select( (etavec - m_ests->offset_logreg.col(ph).array()) + (pheno_data->phenotypes_raw.col(ph).array() - pivec) / wvec, 0);
        V1 = l1->test_mat_conc[ph_eff].transpose() * wvec.matrix().asDiagonal();
        XtWX = V1 * l1->test_mat_conc[ph_eff];
        XtWZ = V1 * zvec.matrix();
        Hinv.compute( XtWX + params->tau[j] * ident_l1 );
        betanew = (Hinv.solve(XtWZ)).array();

        // get w=p*(1-p) and check none of the values are 0
        if( get_wvec(ph, etavec, pivec, wvec, betanew, pheno_data->masked_indivs, m_ests->offset_logreg.col(ph), l1->test_mat_conc[ph_eff], params->l1_ridge_eps) ){
          sout << "ERROR: Zeros occured in Var(Y) during ridge logistic regression." << endl;
          l1->pheno_l1_not_converged(ph) = true;
          break;
        }

        // get the score
        score = ( l1->test_mat_conc[ph_eff].transpose() * (pheno_data->masked_indivs.col(ph).array()).select(pheno_data->phenotypes_raw.col(ph).array() - pivec, 0).matrix()).array() ;
        score -= params->tau[j] * betanew;

        if( score.abs().maxCoeff() < params->l1_ridge_tol ) break;

        betaold = betanew;

      }

      if(niter_cur > params->niter_max){
        sout << "WARNING: Ridge logistic regression did not converge! (Increase --niter)\n";
        l1->pheno_l1_not_converged(ph) = true;
      }
      if( l1->pheno_l1_not_converged(ph) ) break;

      //sout << "Converged in "<< niter_cur << " iterations. Score max = " << score.abs().maxCoeff() << endl;

      // compute Hinv
      //zvec = (pheno_data->masked_indivs.col(ph).array()).select( (etavec - m_ests->offset_logreg.col(ph).array()) + (pheno_data->phenotypes_raw.col(ph).array() - pivec) / wvec, 0);
      V1 = l1->test_mat_conc[ph_eff].transpose() * wvec.matrix().asDiagonal();
      XtWX = V1 * l1->test_mat_conc[ph_eff];
      Hinv.compute( XtWX + params->tau[j] * ident_l1 );


      // LOOCV estimates
      for(chunk = 0; chunk < nchunk; ++chunk ) {
        size_chunk = chunk == nchunk - 1? params->cv_folds - target_size * chunk : target_size;
        j_start = chunk * target_size;

        Ref<MatrixXd> Xmat_chunk = l1->test_mat_conc[ph_eff].block(j_start, 0, size_chunk, bs_l1); // n x k
        Ref<MatrixXd> Yvec_chunk = pheno_data->phenotypes_raw.block(j_start, ph, size_chunk, 1);
        Ref<MatrixXb> mask_chunk = pheno_data->masked_indivs.block(j_start, ph, size_chunk,1);

        V1 = Hinv.solve( Xmat_chunk.transpose() ); // k x n
        for(int i = 0; i < size_chunk; ++i ) {
          if(!mask_chunk(i,0)) continue;
          v2 = Xmat_chunk.row(i) * V1.col(i);
          v2 *= wvec(j_start + i);
          b_loo = (betanew - V1.col(i).array() * (Yvec_chunk(i,0) - pivec(j_start + i)) / (1 - v2)).matrix();
          pred = Xmat_chunk.row(i) * b_loo.col(0);
          pred += m_ests->offset_logreg(j_start + i, ph);
          p1 = 1 - 1/ ( exp(pred) + 1 );

          // if p is within eps of 0/1, set to eps/1-eps
          if( p1 < params->l1_ridge_eps ) p1 = params->l1_ridge_eps;
          else if( p1 > (1-params->l1_ridge_eps) ) p1 = 1 - params->l1_ridge_eps;

          // compute mse and rsq
          l1->cumsum_values[0](ph,j) += p1; // Sx
          l1->cumsum_values[1](ph,j) += Yvec_chunk(i,0); // Sy
          l1->cumsum_values[2](ph,j) += p1 * p1; // Sx2
          l1->cumsum_values[3](ph,j) += Yvec_chunk(i,0) * Yvec_chunk(i,0); // Sy2
          l1->cumsum_values[4](ph,j) += p1 * Yvec_chunk(i,0); // Sxy
          l1->cumsum_values[5](ph,j) += compute_log_lik(Yvec_chunk(i,0), p1); // Sxy
        }
      }

    }

    sout << "done";
    auto ts2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(ts2 - ts1);
    sout << " (" << duration.count() << "ms) "<< endl;
  }

  sout << endl;
}


bool get_wvec(int ph, ArrayXd& etavec, ArrayXd& pivec, ArrayXd& wvec, const ArrayXd& beta, const MatrixXb& masks, const MatrixXd& offset, const MatrixXd& test_mat, const double tol){

  etavec = masks.col(ph).array().select( (offset + test_mat * beta.matrix()).array() , 0);
  pivec = 1 - 1/(etavec.exp() + 1);

  wvec = ArrayXd::Ones( masks.rows() );// set all entries to 1
  // avoid 0 weights by setting w to eps when p is within eps of 0/1
  // (strategy used in glmnet)
  for (int i = 0; i < masks.rows(); i++){
    if( !masks(i,ph) ) continue;

    if( pivec(i) < tol) {
      pivec(i) = 0;
      wvec(i) = tol;
    } else if ( pivec(i) > (1-tol) ){
      pivec(i) = 1;
      wvec(i) = tol;
    } else wvec(i) = pivec(i) * (1-pivec(i));

  }
  //wvec = masks.col(ph).array().select(pivec * (1 - pivec), 1);

  return wvec.minCoeff() == 0;
}


double compute_log_lik(const double y, const double p){
  // negative log likelihood for bernoulli
  double ll;
  ll = - y * log(p) - (1 - y) * log(1-p);
  return(ll);
}

void read_l0(int ph, int ph_eff, struct in_files* files, struct param* params, struct ridgel1* l1, mstream& sout){

  int start, np;
  string fin;
 
  // all blocks in same file
  if(!params->run_l1_only){

    start = 0;
    np = params->total_n_block * params->n_ridge_l0;
    fin = files->loco_tmp_prefix;

    read_l0_chunk(ph, ph_eff, start, np, fin, params, l1, sout);

  } else { // blocks in separate file

    for(size_t i = 0; i < files->bstart.size(); i++){

      start = files->bstart[i] * params->n_ridge_l0;
      np = files->btot[i] * params->n_ridge_l0;
      fin = files->mprefix[i];

      read_l0_chunk(ph, ph_eff, start, np, fin, params, l1, sout);
    }


  }

}

// read in l0 predictors in columns [start,start+np)
void read_l0_chunk(int ph, int ph_eff, int start, int np, const string prefix, struct param* params, struct ridgel1* l1, mstream& sout){

  string in_pheno = prefix + "_l0_Y" + to_string(ph+1);
  ifstream infile;

  infile.open(in_pheno.c_str(), ios::in | ios::binary );
  if (!infile.is_open()) {
    sout << "ERROR : Cannot read temporary file " << in_pheno  << endl ;
    exit(EXIT_FAILURE);
  } else if( getSize(in_pheno) != (long) (sizeof(double) * params->n_samples * np )){
    sout << "ERROR : File " << in_pheno << " is not the right size.\n" << endl ;
    exit(EXIT_FAILURE);
  }
  //cerr << in_pheno << "  " << getSize(in_pheno) << endl;

  // store back values in test_mat
  if(params->use_loocv) {

    infile.read( reinterpret_cast<char *> (&l1->test_mat_conc[ph_eff](0, start)), params->n_samples * np * sizeof(double) );

    //if(ph == 0) sout << endl << "In:\n" << l1->test_mat_conc[ph_eff].block(0,0,5,6) << endl;
  } else {

    int nt = 0;

    for( int m = start; nt < np; nt++, m++ )
      for( int i = 0; i < params->cv_folds; ++i )
        for( int k = 0; k < params->cv_sizes[i]; ++k )
          infile.read( reinterpret_cast<char *> (&l1->test_mat[ph_eff][i](k,m)), sizeof(double) );

  //if(start==0) cerr << endl <<l1->test_mat[ph_eff][0].block(0,0,3,3) << endl;

  }

  infile.close();
  
}

long getSize(string fname){
  
  struct stat stat_buf;
  int rc = stat(fname.c_str(), &stat_buf);

  return ( rc == 0 ? stat_buf.st_size : -1);

}
