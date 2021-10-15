version 1.0

## Version 06-12-2021
##
## This workflow creates vcf index files (vcf.gz.csi) from their vcf.gz files.
## PLINK2 documentation: https://www.cog-genomics.org/plink/2.0/
##
## Input files need to all be VCF files.
##
## Cromwell version support - Successfully tested on v63
##
## Distributed under terms of the MIT License
## Copyright (c) 2021 Brian Sharber
## Contact <brian.sharber@vumc.org>

workflow vcf_index_file_creation {

    input {
        Array[File] input_file    # Input files - need to be VCF files.
        String plink2_docker = "briansha/plink2:terra"
    }

    scatter (file in input_file) {
        call Index_Creation {
          input:
            input_file = file,
            docker = plink2_docker
        }
    }

    output {
        Array[File] output_vcf_index_file = Index_Creation.output_vcf_index_file
        Array[File] output_bgzipped_vcf_file = Index_Creation.output_bgzipped_vcf_file
    }

    meta {
    	author : "Brian Sharber"
        email : "brian.sharber@vumc.org"
        description : "This workflow creates vcf index files from vcf.gz files using plink2."
    }
}

task Index_Creation {
    input {
        File input_file
        String docker # Docker image containing PLINK2.
        Int? disk_size_override
        Float memory = 3.5
        Int cpu = 1
        Int preemptible = 1
        Int maxRetries = 0
    }
    String file_for_gunzip = basename(input_file)
    String file_for_bgzip = basename(input_file, ".gz")
    String file_for_tabix = basename(input_file)
    Float vcf_size = size(input_file, "GiB")
    Int disk = select_first([disk_size_override, ceil(10.0 + 21.0 * vcf_size)]) # A vcf file is quite larger than a vcf.gz file.

    command <<<
        set -euo pipefail
        mv ~{input_file} .
        gunzip ~{file_for_gunzip}
        bgzip ~{file_for_bgzip}
        tabix --csi -p vcf ~{file_for_tabix}
    >>>

    output {
        File output_vcf_index_file = "${file_for_tabix}.csi"
        File output_bgzipped_vcf_file = "${file_for_tabix}"
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
