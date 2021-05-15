version 1.0

## Version 05-06-2021
##
## This workflow converts VCF files to PLINK (.bed, .bim, .fam) files.
## PLINK2 documentation: https://www.cog-genomics.org/plink/2.0/
##
## Input files need to all be VCF files.
##
## This workflow will not work if the link to download PLINK2 changes (see https://www.cog-genomics.org/plink/2.0/).
##
## Cromwell version support - Successfully tested on v61
##
## Distributed under terms of the MIT License
## Copyright (c) 2021 Brian Sharber
## Contact <brian.sharber@vumc.org>

workflow vcf_to_plink {

    input {
        Array[File] input_file    # Input files - need to be VCF files.
        Array[String] output_name # Output names - user-defined names for the converted files.
        String docker_image = "briansha/plink2:latest"
        Int memory = 4
        Int disk = 200
        Int threads = 1
        Int preemptible = 1
        Int maxRetries = 0
    }

    scatter (file_and_output in zip(input_file, output_name)) {
        call Convert_File {
          input:
            input_file = file_and_output.left,
            output_name = file_and_output.right,
            docker_image = docker_image,
            memory = memory,
            disk = disk,
            threads = threads,
            preemptible = preemptible,
            maxRetries = maxRetries
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
        description : "Run PLINK2"
    }
}

task Convert_File {
    input {
        File input_file
        String output_name
        String docker_image # Docker image containing PLINK2.
        Int? memory
        Int? disk
        Int? threads
        Int? preemptible
        Int? maxRetries
    }

    command <<<
        wget http://s3.amazonaws.com/plink2-assets/alpha2/plink2_linux_avx2.zip
        unzip plink2_linux_avx2.zip
        ./plink2 \
        --vcf ~{input_file} \
        --make-bed \
        --out ~{output_name}

    >>>

    output {
        File bed_file = "${output_name}.bed"
        File bim_file = "${output_name}.bim"
        File fam_file = "${output_name}.fam"
    }

    runtime {
        docker: docker_image
        memory: memory + " GiB"
		    disks: "local-disk " + disk + " HDD"
        cpu: threads
        preemptible: preemptible
        maxRetries: maxRetries
    }
}
