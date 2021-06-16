version 1.0

## Version 06-15-2021
##
## This WDL workflow runs QUILT.
## QUILT documentation - https://github.com/rwdavies/QUILT#paragraph-installation
##
## Cromwell version support - Successfully tested on v63
##
## Distributed under terms of the MIT License
## Copyright (c) 2021 Brian Sharber
## Contact <brian.sharber@vumc.org>

workflow run_quilt {
    input {
        String quilt_docker = "briansha/quilt:latest" # Docker image that comes with QUILT and STITCH installed.
    }

    call Imputation {
        input:
          docker = quilt_docker
    }

    output {
        File zipped_output = Imputation.zipped_output
	}

    meta {
	author : "Brian Sharber"
	email : "brian.sharber@vumc.org"
	description : "This WDL workflow runs QUILT - https://github.com/rwdavies/QUILT#paragraph-installation"
    }
}

# Perform rapid diploid genotype imputation from low-coverage sequence using a large reference panel.
task Imputation {
    input {
        String outputdir                                            # Name the output directory to create to store all output.
        String output_zipped_name = "output.zip"                    # Name the .zip file that will contain the output directory that stores all output.
        String chr                                                  # What chromosome to run. Should match BAM headers
        String? regionStart                                         # When running imputation, where to start from. The 1-based position x is kept if regionStart <= x <= regionEnd [default NA]
        String? regionEnd                                           # When running imputation, where to stop [default NA]
        String? buffer                                              # Buffer of region to perform imputation over. So imputation is run form regionStart-buffer to regionEnd+buffer, and reported for regionStart to regionEnd, including the bases of regionStart and regionEnd [default NA]
        File? bamlist                                               # Path to file with bam file locations. File is one row per entry, path to bam files. Bam index files should exist in same directory as for each bam, suffixed either .bam.bai or .bai [default ""]
        File? cramlist                                              # Path to file with cram file locations. File is one row per entry, path to cram files. cram files are converted to bam files on the fly for parsing into QUILT [default ""]
        File? sampleNames_file                                      # Optional, if not specified, sampleNames are taken from the SM tag in the header of the BAM / CRAM file. This argument is the path to file with sampleNames for samples. It is used directly to name samples in the order they appear in the bamlist / cramlist [default ""]
        File? reference                                             # Path to reference fasta used for making cram files. Only required if cramlist is defined [default ""]
        Int? nCores                                                 # How many cores to use [default 1]
        Int? nGibbsSamples                                          # How many Gibbs samples to use [default 7]
        Int? n_seek_its                                             # How many iterations between first using current haplotypes to update read labels, and using current read labels to get new reference haplotypes, to perform [default 3]
        Int? Ksubset                                                # How many haplotypes to use in the faster Gibbs sampling [default 400]
        Int? Knew                                                   # How many haplotypes to replace per-iteration after doing the full reference panel imputation [default 100]
        Int? K_top_matches                                          # How many top haplotypes to store in each grid site when looking for good matches in the full haplotype reference panel. Large values potentially bring in more haplotype diversity, but risk losing haplotypes that are good matches over shorter distances [default 5]
        String output_gt_phased_genotypes = "TRUE"                  # When TRUE, output GT entry contains phased genotypes (haplotypes). When FALSE, it is from the genotype posteriors, and masked when the maximum genotype posterior entry is less than 0.9 [default TRUE]
        Float? heuristic_match_thin                                 # What fraction of grid sites to use when looking for good matches in the full haplotype reference panel. Smaller values run faster but potentially miss haplotypes [default 0.1]
        String? output_filename                                     # Override the default bgzip-VCF / bgen output name with this given file name. Please note that this does not change the names of inputs or outputs (e.g. RData, plots), so if outputdir is unchanged and if multiple QUILT runs are processing on the same region then they may over-write each others inputs and outputs. [default NULL]
        String? RData_objects_to_save                               # Can be used to name interim and misc results from imputation to save an an RData file. Default NULL means do not save such output [default NULL]
        String? output_RData_filename                               # Override the default location for miscellaneous outputs saved in RData format [default NULL]
        File? prepared_reference_filename                           # Optional path to prepared RData file with reference objects. Can be used instead of outputdir to coordinate use of QUILT_prepare_reference and QUILT [default ""]
        String save_prepared_reference = "FALSE"                    # If preparing reference as part of running QUILT, whether to save the prepared reference output file. Note that if the reference was already made using QUILT_prepare_reference, this is ignored [default FALSE]
        String? tempdir                                             # What directory to use as temporary directory. If set to NA, use default R tempdir. If possible, use ramdisk, like /dev/shm/ [default NA]
        Int? bqFilter                                               # Minimum BQ for a SNP in a read. Also, the algorithm uses bq<=mq, so if mapping quality is less than this, the read isnt used [default as.integer(17)]
        Int? panel_size                                             # Integer number of reference haplotypes to use, set to NA to use all of them [default NA]
        File? posfile                                               # Optional, only needed when using genfile or phasefile. File with positions of where to impute, lining up one-to-one with genfile. File is tab seperated with no header, one row per SNP, with col 1 = chromosome, col 2 = physical position (sorted from smallest to largest), col 3 = reference base, col 4 = alternate base. Bases are capitalized. Example first row: 1<tab>1000<tab>A<tab>G<tab> [default ""]
        File? genfile                                               # Path to gen file with high coverage results. Empty for no genfile. If both genfile and phasefile are given, only phasefile is used, as genfile (unphased genotypes) is derivative to phasefile (phased genotypes). File has a header row with a name for each sample, matching what is found in the bam file. Each subject is then a tab seperated column, with 0 = hom ref, 1 = het, 2 = hom alt and NA indicating missing genotype,
                                                                    #         with rows corresponding to rows of the posfile. Note therefore this file has one more row than posfile which has no header [default ""]
        File? phasefile                                             # Path to phase file with truth phasing results. Empty for no phasefile. Supercedes genfile if both options given. File has a header row with a name for each sample, matching what is found in the bam file. Each subject is then a tab seperated column, with 0 = ref and 1 = alt, separated by a vertical bar |, e.g. 0|0 or 0|1. Note therefore this file has one more row than posfile which has no header. [default ""]
        Float? maxDifferenceBetweenReads                            # How much of a difference to allow the reads to make in the forward backward probability calculation. For example, if P(read | state 1)=1 and P(read | state 2)=1e-6, re-scale so that their ratio is this value. This helps prevent any individual read as having too much of an influence on state changes, helping prevent against influence by false positive SNPs [default 1e10]
        String make_plots = "FALSE"                                 # Whether to make some plots of per-sample imputation. Especially nice when truth data. This is pretty slow though so useful more for debugging and understanding and visualizing performance [default FALSE]
        String verbose = "TRUE"                                     # whether to be more verbose when running [default TRUE]
        Int? shuffle_bin_radius                                     # Parameter that controls how to detect ancestral haplotypes that are shuffled during EM for possible re-setting. If set (not NULL), then recombination rate is calculated around pairs of SNPs in window of twice this value, and those that exceed what should be the maximum (defined by nGen and maxRate) are checked for whether they are shuffled [default 5000]
        Float? iSizeUpperLimit                                      # Do not use reads with an insert size of more than this value [default 1e6]
        String record_read_label_usage = "FALSE"                    # Whether to store what read labels were used during the Gibbs samplings (i.e. whether reads were assigned to arbitrary labelled haplotype 1 or 2) [default FALSE]
        String record_interim_dosages = "FALSE"                     # Whether to record interim dosages or not [default FALSE]
        String use_bx_tag = "TRUE"                                  # Whether to try and use BX tag in same to indicate that reads come from the same underlying molecule [default TRUE]
        Int? bxTagUpperLimit                                        # When using BX tag, at what distance between reads to consider reads with the same BX tag to come from different molecules [default 50000]
        String addOptimalHapsToVCF = "FALSE"                        # Whether to add optimal haplotypes to vcf when phasing information is present, where optimal is imputation done when read label origin is known [default FALSE]
        String estimate_bq_using_truth_read_labels = "FALSE"        # When using phasefile with known truth haplotypes, infer truth read labels, and use them to infer the real base quality against the bam recorded base qualities [default FALSE]
        String override_default_params_for_small_ref_panel = "TRUE" # When set to TRUE, then when using a smaller reference panel size (fewer haplotypes than Ksubset), parameter choices are reset appropriately. When set to FALSE, original values are used, which might crash QUILT [default TRUE]
        Int? gamma_physically_closest_to                            # For HLA imputation, the physical position closest to the centre of the gene [default NA]
        Int? seed                                                   # The seed that controls random number generation. When NA, not used# [default NA]
        String hla_run = "FALSE"                                    # Whether to use QUILT to generate posterior state probabilities as part of QUILT-HLA [default FALSE]
        Int? downsampleToCov                                        # What coverage to downsample individual sites to. This ensures no floating point errors at sites with really high coverage [default 30]
        Float? minGLValue                                           # For non-Gibbs full imputation, minimum allowed value in haplotype gl, after normalization. In effect, becomes 1/minGLValue becomes maximum difference allowed between genotype likelihoods [default 1e-10]
        Int? minimum_number_of_sample_reads                         # Minimum number of sample reads a sample must have for imputation to proceed. Samples that have fewer reads than this will not be imputed in a given region and all output will be set to missing [default 2]
        String? nGen                                                # Number of generations since founding or mixing. Note that the algorithm is relatively robust to this. Use nGen = 4 * Ne / K if unsure [default NA]
        File? reference_haplotype_file                              # Path to reference haplotype file in IMPUTE format (file with no header and no rownames, one row per SNP, one column per reference haplotype, space separated, values must be 0 or 1) [default ""]
        File? reference_legend_file                                 # Path to reference haplotype legend file in IMPUTE format (file with one row per SNP, and a header including position for the physical position in 1 based coordinates, a0 for the reference allele, and a1 for the alternate allele) [default ""]
        File? reference_sample_file                                 # Path to reference sample file (file with header, one must be POP, corresponding to populations that can be specified using reference_populations) [default ""]
        Array[String]? reference_populations                        # Vector with character populations to include from reference_sample_file e.g. CHB, CHS [default NA]
        Int? reference_phred                                        # Phred scaled likelihood or an error of reference haplotype. Higher means more confidence in reference haplotype genotypes, lower means less confidence [default 30]
        File? reference_exclude_samplelist_file                     # File with one column of samples to exclude from reference samples e.g. in validation, the samples you are imputing [default ""]
        File? region_exclude_file                                   # File with regions to exclude from constructing the reference panel. Particularly useful for QUILT_HLA, where you want to exclude SNPs in the HLA genes themselves, so that reads contribute either to the read mapping or state inference. This file is space separated with a header of Name, Chr, Start and End, with Name being the HLA gene name (e.g. HLA-A),
                                                                    #           Chr being the chromosome (e.g. chr6), and Start and End are the 1-based starts and ends of the genes (i.e. where we don't want to consider SNPs for the Gibbs sampling state inference) [default ""]
        File? genetic_map_file                                      # Path to file with genetic map information, a file with 3 white-space delimited entries giving position (1-based), genetic rate map in cM/Mbp, and genetic map in cM. If no file included, rate is based on physical distance and expected rate (expRate) [default ""]
        Int? nMaxDH                                                 # Integer Maximum number of distinct haplotypes to store in reduced form. Recommended to keep as 2 ** N - 1 where N is an integer greater than 0 i.e. 255, 511, etc [default NA]
        String make_fake_vcf_with_sites_list = "FALSE"              # Whether to output a list of sites as a minimal VCF, for example to use with GATK 3 to genotype given sites [default FALSE]
        String? output_sites_filename                               # If make_fake_vcf_with_sites_list is TRUE, optional desired filename where to output sites VCF [default NA]
        Int? expRate                                                # Expected recombination rate in cM/Mb [default 1]
        Int? maxRate                                                # Maximum recomb rate cM/Mb [default 100]
        Float? minRate                                              # Minimum recomb rate cM/Mb [default 0.1]
        String print_extra_timing_information = "FALSE"             # Print extra timing information, i.e. how long sub-processes take, to better understand why things take as long as they do [default FALSE]
        String? block_gibbs_iterations                              # What iterations to perform block Gibbs sampling for the Gibbs sampler [default c(3,6,9)]
        Int? n_gibbs_burn_in_its                                    # How many iterations to run the Gibbs sampler for each time it is run [default 20]
        String plot_per_sample_likelihoods = "FALSE"                # Plot per sample likelihoods i.e. the likelihood as the method progresses through the Gibbs sampling iterations [default FALSE]
        String use_small_eHapsCurrent_tc = "FALSE"                  # For testing purposes only [default FALSE]
        Boolean help = false                                        # Show this help message and exit

        String docker
        Float memory = 3.5
        Int disk = 200
        Int cpu = 4
        Int preemptible = 1
    	Int maxRetries = 0
    }

    command <<<
        set -euo pipefail
        cp -r /usr/local/bin/QUILT/* .
        ls

        ./QUILT.R     \
        ~{if defined(outputdir) then "--outputdir=~{outputdir} " else " "} \
        ~{if defined(chr) then "--chr=~{chr} " else " "} \
        ~{if defined(regionStart) then "--regionStart=~{regionStart} " else " "} \
        ~{if defined(regionEnd) then "--regionEnd=~{regionEnd} " else " "} \
        ~{if defined(buffer) then "--buffer=~{buffer} " else " "} \
        ~{if defined(bamlist) then "--bamlist=~{bamlist} " else " "} \
        ~{if defined(cramlist) then "--cramlist=~{cramlist} " else " "} \
        ~{if defined(sampleNames_file) then "--sampleNames_file=~{sampleNames_file} " else " "} \
        ~{if defined(reference) then "--reference=~{reference} " else " "} \
        ~{if defined(nCores) then "--nCores=~{nCores} " else " "} \
        ~{if defined(nGibbsSamples) then "--nGibbsSamples=~{nGibbsSamples} " else " "} \
        ~{if defined(n_seek_its) then "--n_seek_its=~{n_seek_its} " else " "} \
        ~{if defined(Ksubset) then "--Ksubset=~{Ksubset} " else " "} \
        ~{if defined(Knew) then "--Knew=~{Knew} " else " "} \
        ~{if defined(K_top_matches) then "--K_top_matches=~{K_top_matches} " else " "} \
        ~{if defined(heuristic_match_thin) then "--heuristic_match_thin=~{heuristic_match_thin} " else " "} \
        ~{if defined(output_filename) then "--output_filename=~{output_filename} " else " "} \
        ~{if defined(RData_objects_to_save) then "--RData_objects_to_save=~{RData_objects_to_save} " else " "} \
        ~{if defined(output_RData_filename) then "--output_RData_filename=~{output_RData_filename} " else " "} \
        ~{if defined(prepared_reference_filename) then "--prepared_reference_filename=~{prepared_reference_filename} " else " "} \
        ~{if defined(tempdir) then "--tempdir=~{tempdir} " else " "} \
        ~{if defined(bqFilter) then "--bqFilter=~{bqFilter} " else " "} \
        ~{if defined(panel_size) then "--panel_size=~{panel_size} " else " "} \
        ~{if defined(posfile) then "--posfile=~{posfile} " else " "} \
        ~{if defined(genfile) then "--genfile=~{genfile} " else " "} \
        ~{if defined(phasefile) then "--phasefile=~{phasefile} " else " "} \
        ~{if defined(maxDifferenceBetweenReads) then "--maxDifferenceBetweenReads=~{maxDifferenceBetweenReads} " else " "} \
        ~{if defined(shuffle_bin_radius) then "--shuffle_bin_radius=~{shuffle_bin_radius} " else " "} \
        ~{if defined(iSizeUpperLimit) then "--iSizeUpperLimit=~{iSizeUpperLimit} " else " "} \
        ~{if defined(bxTagUpperLimit) then "--bxTagUpperLimit=~{bxTagUpperLimit} " else " "} \
        ~{if defined(gamma_physically_closest_to) then "--gamma_physically_closest_to=~{gamma_physically_closest_to} " else " "} \
        ~{if defined(seed) then "--seed=~{seed} " else " "} \
        ~{if defined(downsampleToCov) then "--downsampleToCov=~{downsampleToCov} " else " "} \
        ~{if defined(minGLValue) then "--minGLValue=~{minGLValue} " else " "} \
        ~{if defined(minimum_number_of_sample_reads) then "--minimum_number_of_sample_reads=~{minimum_number_of_sample_reads} " else " "} \
        ~{if defined(nGen) then "--nGen=~{nGen} " else " "} \
        ~{if defined(reference_haplotype_file) then "--reference_haplotype_file=~{reference_haplotype_file} " else " "} \
        ~{if defined(reference_legend_file) then "--reference_legend_file=~{reference_legend_file} " else " "} \
        ~{if defined(reference_sample_file) then "--reference_sample_file=~{reference_sample_file} " else " "} \
        ~{if defined(reference_populations) then "--reference_populations=~{reference_populations} " else " "} \
        ~{if defined(reference_phred) then "--reference_phred=~{reference_phred} " else " "} \
        ~{if defined(reference_exclude_samplelist_file) then "--reference_exclude_samplelist_file=~{reference_exclude_samplelist_file} " else " "} \
        ~{if defined(region_exclude_file) then "--region_exclude_file=~{region_exclude_file} " else " "} \
        ~{if defined(genetic_map_file) then "--genetic_map_file=~{genetic_map_file} " else " "} \
        ~{if defined(nMaxDH) then "--nMaxDH=~{nMaxDH} " else " "} \
        ~{if defined(output_sites_filename) then "--output_sites_filename=~{output_sites_filename} " else " "} \
        ~{if defined(expRate) then "--expRate=~{expRate} " else " "} \
        ~{if defined(maxRate) then "--maxRate=~{maxRate} " else " "} \
        ~{if defined(minRate) then "--minRate=~{minRate} " else " "} \
        ~{if defined(block_gibbs_iterations) then "--block_gibbs_iterations=~{block_gibbs_iterations} " else " "} \
        ~{if defined(n_gibbs_burn_in_its) then "--n_gibbs_burn_in_its=~{n_gibbs_burn_in_its} " else " "} \
        ~{if defined(output_gt_phased_genotypes) then "--output_gt_phased_genotypes=~{output_gt_phased_genotypes} " else " "} \
        ~{if defined(save_prepared_reference) then "--save_prepared_reference=~{save_prepared_reference} " else " "} \
        ~{if defined(make_plots) then "--make_plots=~{make_plots} " else " "} \
        ~{if defined(verbose) then "--verbose=~{verbose} " else " "} \
        ~{if defined(record_read_label_usage) then "--record_read_label_usage=~{record_read_label_usage} " else " "} \
        ~{if defined(record_interim_dosages) then "--record_interim_dosages=~{record_interim_dosages} " else " "} \
        ~{if defined(use_bx_tag) then "--use_bx_tag=~{use_bx_tag} " else " "} \
        ~{if defined(addOptimalHapsToVCF) then "--addOptimalHapsToVCF=~{addOptimalHapsToVCF} " else " "} \
        ~{if defined(estimate_bq_using_truth_read_labels) then "--estimate_bq_using_truth_read_labels=~{estimate_bq_using_truth_read_labels} " else " "} \
        ~{if defined(override_default_params_for_small_ref_panel) then "--override_default_params_for_small_ref_panel=~{override_default_params_for_small_ref_panel} " else " "} \
        ~{if defined(hla_run) then "--hla_run=~{hla_run} " else " "} \
        ~{if defined(make_fake_vcf_with_sites_list) then "--make_fake_vcf_with_sites_list=~{make_fake_vcf_with_sites_list} " else " "} \
        ~{if defined(print_extra_timing_information) then "--print_extra_timing_information=~{print_extra_timing_information} " else " "} \
        ~{if defined(plot_per_sample_likelihoods) then "--plot_per_sample_likelihoods=~{plot_per_sample_likelihoods} " else " "} \
        ~{if defined(use_small_eHapsCurrent_tc) then "--use_small_eHapsCurrent_tc=~{use_small_eHapsCurrent_tc} " else " "} \
        ~{if help then "--help " else " "}

        zip -r ~{output_zipped_name} ~{outputdir}
	>>>

    runtime {
	  docker: docker
	  memory: memory + " GiB"
	  disks: "local-disk " + disk + " HDD"
          cpu: cpu
	  preemptible: preemptible
          maxRetries: maxRetries
	}

    output {
         File zipped_output = "${output_zipped_name}"
    }
}
