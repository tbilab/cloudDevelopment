version 1.0

## Version 06-10-2021
##
## This workflow converts VCF files into VCF files filtered according to text files containing values the user wants extracted.
## Then, those VCF files are converted into PLINK (.bed, .bim, .fam) files.
## PLINK2 documentation: https://www.cog-genomics.org/plink/2.0/
##
## Input files need to all be VCF files.
##
## Cromwell version support - Successfully tested on v63
##
## Distributed under terms of the MIT License
## Copyright (c) 2021 Brian Sharber
## Contact <brian.sharber@vumc.org>

workflow vcf_filtering {

    input {
        File input_parameters_file # File containing tab-separated list of: vcf file, text file, and user-defined name for the converted output file.
        String plink2_docker = "briansha/plink2:terra"
        Array[String] output_bed_names # User-defined prefix names for output .bed, .bim, .fam files for Convert_File.
    }
    Array[Array[String]] input_parameters = read_tsv(input_parameters_file)

    scatter (line in input_parameters) {
        call Filter_File {
          input:
            input_file = line[0],
            input_text_file = line[1],
            output_name = line[2],
            docker = plink2_docker
        }
    }

    scatter (file_and_output in zip(Filter_File.vcf_file, output_bed_names)) {
        call Convert_File {
          input:
            input_file = file_and_output.left,
            output_name = file_and_output.right,
            docker = plink2_docker
        }
    }

    output {
        Array[File] output_vcf = Filter_File.vcf_file
        Array[File] output_bed = Convert_File.bed_file
        Array[File] output_bim = Convert_File.bim_file
        Array[File] output_fam = Convert_File.fam_file
    }

    meta {
    	author : "Brian Sharber"
        email : "brian.sharber@vumc.org"
        description : "This workflow converts VCF files into VCF files filtered according to text files containing values the user wants extracted using plink2. Then, those VCF files are converted into PLINK (.bed, .bim, .fam) files."
    }
}

# Filters vcf or vcf.gz files according to .txt files containing values the user wants extracted into vcf files.
task Filter_File {
    input {
        File input_file
        File input_text_file
        String output_name
        String docker # Docker image containing PLINK2.
        Int? disk_size_override
        Float memory = 3.5
        Int cpu = 1
        Int preemptible = 1
        Int maxRetries = 0
    }
    Float vcf_size = size(input_file, "GiB")
    Int disk = select_first([disk_size_override, ceil(10.0 + 10.0 * vcf_size)]) # VCF files take up quite alot of space.

    command <<<
        set -euo pipefail
        plink2 \
        --extract ~{input_text_file} \
        --vcf ~{input_file} \
        --export vcf \
        --out ~{output_name}

        gzip ~{output_name}.vcf
    >>>

    output {
        File vcf_file = "${output_name}.vcf.gz"
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

task Convert_File {
    input {
        File input_file
        String output_name
        String docker # Docker image containing PLINK2.
        Int? disk_size_override
        Float memory = 3.5
        Int cpu = 1
        Int preemptible = 1
        Int maxRetries = 0
    }
    Float vcf_size = size(input_file, "GiB")
    Int disk = select_first([disk_size_override, ceil(10.0 + 3.0 * vcf_size)]) # .bed, .bim, and .fam files altogether don't take up as much space as non-gzipped VCFs.

    command <<<
        set -euo pipefail
        plink2 \
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
        docker: docker
        memory: memory + " GiB"
		disks: "local-disk " + disk + " HDD"
        cpu: cpu
        preemptible: preemptible
        maxRetries: maxRetries
    }
}
