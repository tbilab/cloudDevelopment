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

#ifndef REGENIE_H
#define REGENIE_H


#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <random>
#include <map>
#include <fstream>
#include <math.h>       /* exp */
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>

// if using external LAPACK routines
#ifdef WITH_OPENBLAS
#include "lapacke.h"
#elif defined(WITH_MKL)
#include "mkl_lapacke.h"
#endif

#include <boost/math/distributions.hpp>

#include "Eigen/Dense"
#include "Eigen/StdVector"

#ifdef __linux__
#include <omp.h>
#endif

#define MAXFILELEN 2001

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long long uint64;
typedef Eigen::Array<bool,Eigen::Dynamic,1> ArrayXb;
typedef Eigen::Matrix<bool,Eigen::Dynamic,Eigen::Dynamic> MatrixXb;
typedef Eigen::Map<Eigen::ArrayXd > MapArXd;
typedef Eigen::Map<ArrayXb> MapArXb;
typedef Eigen::Array<uint16_t,Eigen::Dynamic,1> ArrayXt;

inline bool file_exists (const std::string& name) {
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
}

// for the log file
class mstream
{
  public:
    std::ofstream coss;

    template <class S>
      mstream& operator<< (const S& val)
      {
        coss << val;
        std::cout << val;
        return *this;
      }

    mstream& operator<< (std::ostream& (*pfun)(std::ostream&))
    {
      pfun(coss);
      pfun(std::cout);
      return *this;
    };

    mstream(void);
    ~mstream(void);
};


class MeasureTime {

  public:
    std::chrono::steady_clock::time_point begin;
    std::chrono::steady_clock::time_point end;
    time_t start_time_info, end_time_info;

    void init() {
      auto start = std::chrono::system_clock::now(); // wall clock
      start_time_info = std::chrono::system_clock::to_time_t( start ); 
      begin = std::chrono::steady_clock::now(); // to measure elapsed time
    }

    void stop(){
      auto endtime = std::chrono::system_clock::now(); 
      end_time_info = std::chrono::system_clock::to_time_t( endtime ); 
      end = std::chrono::steady_clock::now();
    }

    MeasureTime(void);
    ~MeasureTime(void);
};


struct param {

  std::string err_help = "For list of arguments, run with option --help\n"; // for checks
  std::string webinfo = "For more information, use option '--help' or visit the website: https://rgcgithub.github.io/regenie/"; 

  //////
  // global options
  int run_mode; // running in null model fitting (=1) or association testing (=2)
  bool test_mode = false; // step 1: false; step 2 = true
  bool binary_mode = false; // QT = false, BT = true
  bool strict_mode = false; // remove individuals with any NA
  bool bgenSample = false; // .sample file for bgen file
  bool gzOut = false; // to compress output files (.loco and .regenie files)
  bool getCorMat = false;

  // filters 
  bool rm_indivs = false; // user specify to remove genotyped samples from analysis
  bool keep_indivs = false; // user specify to keep only select genotyped samples in the analysis
  bool keep_snps = false; // user specify to keep select snps in analysis
  bool rm_snps = false; // user specify to remove snps from analysis
  bool select_phenos = false; // user specify which phenotype columns to use
  bool select_covs = false, cat_cov = false; // user specify which covariate columns to use and if categorical covars present
  int max_cat_levels = 10; // maximum number of categories of categorical covars
  bool select_chrs = false; // user specify which chromosomes to test

  // other global options
  const std::string missing_pheno_str = "NA";
  const double missing_value_double = -999;
  int nChrom = 23; // total number of chromosome numbers (sex chromosomes collapsed in chr23)
  bool CC_ZeroOne = true; // BT: 0/1 encoding?
  double numtol = 1e-6;
  double numtol_eps = 10 * std::numeric_limits<double>::epsilon();
  double tol = 1e-8; // for logistic regression
  double eigen_val_rel_tol = 1e-16;
  double nl_dbl_dmin = 10.0 * std::numeric_limits<double>::min();
  int threads = 0;
  bool verbose = false;
  bool early_exit = false;
  bool split_l0 = false, run_l0_only = false, run_l1_only = false; // run level 0 in parallel across different jobs
  int njobs, job_num, parallel_nGeno, parallel_nBlocks, parallel_nSnps;

  // for input data
  uint32_t n_samples = 0; // number of samples
  int n_pheno = 0; // number of phenotypes
  int n_cov = 0; // number of covariates
  int ncov; // number of linearly independent covariates
  uint32_t n_variants = 0, nvs_stored = 0; // number of variants in genotype file
  std::map <std::string, uint32_t> FID_IID_to_ind;
  std::vector< std::vector<std::string> > FIDvec; // store FID/IID separately (for write-samples option)
  bool with_bgi = false; // input bgi index file for BGEN format
  bool ref_first = false; // ordering of REF/ALT alleles in input genotype file
  std::vector<bool> sex; // 0 is female, 1 is male


  // step 1 
  int block_size = -1; // number of SNPs per block
  int cv_folds = 5; // number of CV folds
  int n_block = -1; // number of blocks to run
  int total_n_block = 0; // number of blocks to run across all chrs
  int n_ridge_l0 = 5; // number of ridge parameters at level 0
  int n_ridge_l1 = 5; // number of ridge parameters at level 1
  double alpha_prior = -1; // to set MAF dependent prior on the effect sizes
  int chunk_mb = 1000; // max amount of memory to use with LOOCV
  bool user_ridge_params_l0 = false; // if user specifies ridge parameters
  bool user_ridge_params_l1 = false; // if user specifies ridge parameters
  bool use_loocv = false; // specify whether to use LOOCV [note: this is not used if method=1]
  bool make_loco = true; // specify whether to compute & ouput LOCO predictions
  bool print_prs = false; // specify to print PRS (i.e. no LOCO used)
  bool write_blups = false; // write BLUP predictions for each chromosome
  bool write_l0_pred = false; // specify whether to write level 0 predictions to file to save on RAM
  bool rm_l0_pred = true; // specify whether to delete written level 0 predictions after level 1
  bool print_block_betas = false; // print betas from level 0 within each block (for debugging)
  bool force_run = false; // if using more than max nvariants in step 1
  int max_step1_variants = 1e6; // prevent users using too many step 1 variants
  int niter_max_ridge = 500; // max number of iterations for ridge logistic reg.
  int niter_max_line_search_ridge = 100; // max number of iterations for line search in ridge logistic reg.
  double l1_ridge_tol = 1e-4; // tolerance level for convergence criteria
  double l1_ridge_eps = 1e-5; // epsilon used to set weights for 0/1 probabilities
  uint32_t print_snpcount = 0; 
  std::vector<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> >  beta_print_out;
  std::vector<double> lambda; // ridge parameters at level 0
  std::vector<double> tau; // ridge parameters at level 1
  // TO REMOVE
  bool within_sample_l0 = false; // specify to use within-sample predictions as features at level 1 (default is to use out-of-sample predictions)
  std::vector<int> cv_sizes;


  // step 2
  bool rm_missing_qt = true; // remove missing individuals when performing test with QTs
  std::string file_type; // type of the genotype file format;
  bool streamBGEN = true; //  for BGEN v1.2 with 8-bit encoding
  bool fastMode = true; // use fast version of step 2 
  bool dosage_mode = false; // track if dosages are present for step 2
  bool split_by_pheno = true; // specify whether to write testing result in separate phenotype files
  bool skip_blups = false;
  bool use_prs = false; // adjust for whole genome PRS (no LOCO)
  double min_MAC = 5, min_MAC_mask; // minimum MAC of SNPs in testing mode
  bool setMinMAC = false;
  double min_INFO = 0; // minimum INFO score of SNPs (dosages) in testing mode
  bool setMinINFO = false;
  bool write_samples = false; // write sample IDs for each trait
  double alpha_pvalue = 0.05; // significance threshold above which to use firth correction
  int test_type = 0; // add=0/dom=1/rec=2 test
  // spa
  bool use_SPA = false; // saddlepoint approximation to estimate pvalue
  int niter_max_spa = 1000; 
  double tol_spa = pow( std::numeric_limits<double>::epsilon(), 0.25);
  // firth
  bool firth = false;// firth correction using LRT
  bool firth_approx = false; // approx. to Firth LRT
  int niter_max = 30; // max number of iterations for logistic reg.
  double numtol_firth = 1e-5; // tolerance level for firth
  int niter_max_firth = 250; // max number of iterations in Firth logistic reg.
  int niter_max_firth_null = 1000; // max number of iterations in Firth logistic reg. null model
  int niter_max_line_search = 25; // max number of iterations for line search in logistic reg.
  int maxstep = 5; // max step size in penalized logistic regression
  int maxstep_null = 25; // max step size in null penalized logistic regression
  int retry_maxstep_firth=5, retry_niter_firth=5000; // fallback settings for null approx. firth regression
  bool fix_maxstep_null = false; // if user specifies max step size
  bool back_correct_se = false; // for SE with Firth
  bool print_pheno_name = false; // add phenotype name when writing to file with sample IDs
  bool htp_out = false; 
  std::string cohort_name; // Name of cohort to add in HTP output
  bool set_range = false;
  int range_chr; 
  double range_min, range_max; // use genomic region to filter variants

  // snp sets (masks/joint tests)
  bool snp_set = false; 
  bool build_mask = false; 
  bool w_anno_lab = false;
  bool check_mask_files = false, strict_check_burden = false, fail_check = false;
  bool skip_test = false; // skip computing tests
  bool joint_test = false; // for step 2 joint testing
  std::string burden = ""; // type of burden test;
  uint max_set_size = 1000; // maximum number of predictors in joint test
  bool set_select_list = false; // comma separated list of sets given
  bool keep_sets = false; // user specify to keep select sets in analysis
  bool rm_sets = false; // user specify to remove sets from analysis
  bool w_regions = false; // categorize by set regions 
  int max_cat = 64; // maximum number of annotations (to fit in uint64)
  std::vector<std::string> mbins; // temporary object to store aaf bins
  bool mask_rule_max = true, mask_rule_comphet = false; // default use max to combine mask
  std::string mask_rule = "max";
  bool set_aaf = false;// for user-given AAFs for building masks
  bool singleton_carriers = false; // carrier count used to define singletons
  bool write_masks = false, write_setlist = false; //write masks to bed file
  bool mask_loo = false;
  bool p_joint_only = false;
  std::string mask_loo_name, mask_loo_set, mask_loo_region; // for LOO with masks
  double mask_loo_aaf;
  bool nnls_out_all = false;
  int nnls_napprox = 10;
  double acat_a1 = 1, acat_a2 = 1; // for ACAT test

};

// for input files
struct in_files {

  std::string bed_prefix;
  std::string pgen_prefix;
  std::string bgen_file, sample_file;
  std::string file_ind_include, file_ind_exclude;
  std::string file_snps_include, file_snps_exclude;
  std::string file_sets_include, file_sets_exclude;
  std::string cov_file, pheno_file;
  std::string loco_tmp_prefix = "";
  std::string split_file;
  std::string out_file;
  std::string blup_file;
  std::vector<std::shared_ptr<std::ofstream>> write_preds_files;
  std::vector<std::string> blup_files;
  std::vector<std::string> pheno_names;
  std::vector<int> pheno_index;
  std::vector<int> chr_counts, chr_read;
  uint64 bed_block_size; // prevent overflow
  std::ifstream bed_ifstream;
  std::vector<uchar> inbed;
  std::string set_file, new_sets;
  std::string anno_file, anno_labs_file, mask_file, aaf_file;
  std::vector<int> bstart, btot; // for parallel l0
  std::vector<std::string> mprefix; // for parallel l0

};

struct filter {

  // to filter phenotype/covariates/genotype
  std::map<std::string, bool> pheno_colKeep_names, cov_colKeep_names; // true for qVar, false for catVar
  std::map <int, bool> chrKeep_test;
  std::map <std::string, uint64> snpID_to_ind;
  ArrayXb ind_ignore, has_missing, ind_in_analysis;
  uint32_t step1_snp_count = 0;
  std::vector<bool> geno_mask;
  std::map <std::string, std::vector<int>> setID_to_ind;//chr,index

};

template <typename T> 
void start_log(T,const std::string,MeasureTime*,mstream&);

void print_help(bool);
void read_params_and_check(int argc,char *argv[],struct param*,struct in_files*,struct filter*,MeasureTime*,mstream&);
void print_header(std::ostream&);
void set_ridge_params(int,std::vector<double>&,const std::string,mstream&);
void print_usage_info(struct param*,struct in_files*,mstream&);
int chrStrToInt(const std::string, const int);
std::vector<std::string> check_name(std::string const&,mstream&);
double convertDouble(const std::string&,struct param*,mstream&);
double convertNumLevel(const std::string&,std::map<std::string,int>&,struct param*,mstream&);
std::string print_csv(const std::vector<std::string>&);

template <typename KEY, typename VALUE> 
bool in_map(KEY element, std::map<KEY,VALUE>& emap){
  return emap.find(element) != emap.end();
}


#endif
