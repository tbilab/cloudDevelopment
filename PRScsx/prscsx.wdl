version 1.0

## Version 1-10-2021
##
## This workflow integrates GWAS summary statistics and external LD reference panels
## from multiple populations to improve cross-population polygenic prediction.
##
## This workflow uses the PRScsx tool - https://github.com/getian107/PRScsx
##
## For this workflow to run correctly:
## - In task Predict for the References: At least one file path must be provided and boolean be true.
##
## Cromwell version support - Successfully tested on v72
##
## Distributed under terms of the MIT License
## Copyright (c) 2021 Brian Sharber
## Contact <brian.sharber@vumc.org>

workflow prscsx {
    input {
        String prscsx_docker = "briansha/prscsx:latest"
    }
    call Predict {
        input:
            docker = prscsx_docker,
    }

    output {
          Array[File] concatenated_files = Predict.concatenated_files
          Array[File] scored_files = Predict.scored_files
    }

    meta {
    	author : "Brian Sharber"
        email : "brian.sharber@vumc.org"
        description : "This workflow integrates GWAS summary statistics and external LD reference panels from multiple populations to improve cross-population polygenic prediction."
    }
}

task Predict {
    input {
        File bed
        File bim
        File fam
        String bim_prefix
        Array[File] sst_file_paths
        String sst_file_names
        String n_gwas
        String? chrom
        String pop
        String out_dir
        String out_name

        # References
        File? afr_path
        File? amr_path
        File? eas_path
        File? eur_path
        File? sas_path
        Boolean afr = false
        Boolean amr = false
        Boolean eas = false
        Boolean eur = false
        Boolean sas = false

        # SNP
        File snp

        # Runtime
        # For huge bed files (such as 1.6 TB), you do not want to use too much CPU and memory.
        # - Alot of time will be spent on localization - localizing the bed file to the docker container.
        String docker
        Int? disk_size_override
        Float memory = 8.0
        Int cpu = 2
        Int maxRetries = 0
    }
    String bed_prefix = basename(bed, ".bed")
    Float bed_file_size = size(bed, "GiB")
    Int disk = select_first([disk_size_override, ceil(100.0 + 2.0 * bed_file_size)])

    # A preemptible instance is very likely to fail after 24 hours have passed.
    # Skipping the preemptible and going straight to an on-demand instance will prevent the analysis from
    # needing to run a second time once the preemptible fails in the time it takes attempting to localize
    # huge files to the Docker container.
    Int preemptible = if bed_file_size < 2000 then 1 else 0

    command <<<
        set -euo pipefail
        mv /usr/local/bin/PRScsx .
        mv ~{bed} .
        mv ~{bim} .
        mv ~{fam} .
        for file in ~{sep=' ' sst_file_paths}; do \
            mv $file .; \
        done

        # LD reference panels constructed using the UK Biobank data
        ~{if (afr) then "mv ~{afr_path} . \n" +
            " tar -zxvf ldblk_ukbb_afr.tar.gz" else " "}
        ~{if (amr) then "mv ~{amr_path} . \n" +
            " tar -zxvf ldblk_ukbb_amr.tar.gz" else " "}
        ~{if (eas) then "mv ~{eas_path} . \n" +
            " tar -zxvf ldblk_ukbb_eas.tar.gz" else " "}
        ~{if (eur) then "mv ~{eur_path} . \n" +
            " tar -zxvf ldblk_ukbb_eur.tar.gz" else " "}
        ~{if (sas) then "mv ~{sas_path} . \n" +
            " tar -zxvf ldblk_ukbb_sas.tar.gz" else " "}

        # SNP info file
        mv ~{snp} .

        # The output directory must exist first (it won't be created by PRScsx).
        mkdir ~{out_dir}

        #running PRScsx
        pwd
        python3 PRScsx/PRScsx.py \
        --ref_dir=`pwd` \
        ~{if defined(bim_prefix) then "--bim_prefix=~{bim_prefix} " else " "} \
        ~{if defined(sst_file_names) then "--sst_file=~{sst_file_names} " else " "} \
        ~{if defined(n_gwas) then "--n_gwas=~{n_gwas} " else " "} \
        ~{if defined(pop) then "--pop=~{pop} " else " "} \
        ~{if defined(chrom) then "--chrom=~{chrom} " else " "} \
        ~{if defined(out_dir) then "--out_dir=~{out_dir} " else " "} \
        ~{if defined(out_name) then "--out_name=~{out_name} " else " "}

        #concatenating results
        #(the chr*.txt files here are an output from above) (there would be one line of this for each population used)
        ~{if (afr) then "cat ~{out_dir}/~{out_name}_AFR*.txt > ~{out_name}_AFR_concat.txt" else " "}
        ~{if (amr) then "cat ~{out_dir}/~{out_name}_AMR*.txt > ~{out_name}_AMR_concat.txt" else " "}
        ~{if (eas) then "cat ~{out_dir}/~{out_name}_EAS*.txt > ~{out_name}_EAS_concat.txt" else " "}
        ~{if (eur) then "cat ~{out_dir}/~{out_name}_EUR*.txt > ~{out_name}_EUR_concat.txt" else " "}
        ~{if (sas) then "cat ~{out_dir}/~{out_name}_SAS*.txt > ~{out_name}_SAS_concat.txt" else " "}

        ls
        ls ~{out_dir}
        #scoring the results
        ~{if (afr) then "plink2 -bfile ~{bed_prefix} -score ~{out_name}_AFR_concat.txt 2 4 6 -out ~{out_name}_AFR_scored" else " "}
        ~{if (amr) then "plink2 -bfile ~{bed_prefix} -score ~{out_name}_AMR_concat.txt 2 4 6 -out ~{out_name}_AMR_scored" else " "}
        ~{if (eas) then "plink2 -bfile ~{bed_prefix} -score ~{out_name}_EAS_concat.txt 2 4 6 -out ~{out_name}_EAS_scored" else " "}
        ~{if (eur) then "plink2 -bfile ~{bed_prefix} -score ~{out_name}_EUR_concat.txt 2 4 6 -out ~{out_name}_EUR_scored" else " "}
        ~{if (sas) then "plink2 -bfile ~{bed_prefix} -score ~{out_name}_SAS_concat.txt 2 4 6 -out ~{out_name}_SAS_scored" else " "}
        ls
    >>>

    output {
        Array[File] concatenated_files = glob("*_concat.txt")
        Array[File] scored_files = glob("*_scored.sscore")
    }

    runtime {
        docker: docker
        memory: memory + " GiB"
		disks: "local-disk " + disk + " HDD"
        cpu: cpu
        preemptible: preemptible
        maxRetries: maxRetries
    }
}
