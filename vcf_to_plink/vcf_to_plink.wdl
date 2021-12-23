version 1.0

## Version 06-10-2021
##
## This workflow converts VCF files to PLINK (.bed, .bim, .fam) files - (see https://www.cog-genomics.org/plink/2.0/).
## PLINK2 documentation: https://www.cog-genomics.org/plink/2.0/
##
## Input files need to all be VCF files.
## - VCF.GZ files are recommended to use as input, as that will save on storage space and localization time.
## 
## Cromwell version support - Successfully tested on v63
##
## Distributed under terms of the MIT License
## Copyright (c) 2021 Brian Sharber
## Contact <brian.sharber@vumc.org>

workflow vcf_to_plink {

    input {
        Array[File] input_file    # Input files - need to be VCF files.
        Array[String] output_name # Output names - user-defined names for the converted files.
        String plink2_docker = "briansha/plink2:terra"
    }
    
    scatter (file_and_output in zip(input_file, output_name)) {
        call Convert_File { 
          input: 
            input_file = file_and_output.left,
            output_name = file_and_output.right,
            docker = plink2_docker
        }
    }

    output {
        Array[File] output_bed = Convert_File.bed_file
        Array[File] output_bim = Convert_File.bim_file
        Array[File] output_fam = Convert_File.fam_file
    }
    
    meta {
    	author : "Brian Sharber"
        email : "brian.sharber@vumc.org"
        description : "This workflow converts VCF files to PLINK (.bed, .bim, .fam) files using plink2."
    }
}

# PLINK 2.0 has parallelism capability.
# Doubling the CPU will cut the execution time in half.
# Make sure to adjust the memory to accommodate the CPU - usually CPUx4 is a good hedge.
task Convert_File {
    input {
        File input_file
        String output_name
        String docker # Docker image containing PLINK2.
        Int? disk_size_override
        Float memory = 128.0
        Int cpu = 32
        Int preemptible = 1
        Int maxRetries = 0
    }
    Float vcf_size = size(input_file, "GiB")
    Int disk = select_first([disk_size_override, ceil(24.0 + 10.0 * vcf_size)])

    command <<<
        set -euo pipefail
        plink2 \
        --vcf ~{input_file} \
        --make-bed \
        --threads ~{cpu} \
        --out ~{output_name}
    >>>
    
    output {
        File bed_file = "${output_name}.bed"
        File bim_file = "${output_name}.bim"
        File fam_file = "${output_name}.fam"
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
