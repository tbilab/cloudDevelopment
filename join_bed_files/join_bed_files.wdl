version 1.0

## Version 06-06-2021
##
## This workflow concatenates .bed files (containing singular chromosome data) to one .bed file containing all chromosome data the user needs.
## PLINK2 documentation: https://www.cog-genomics.org/plink/2.0/
##
## Input files need to all be VCF files.
##
## This workflow will not work if the link to download BEDTOOLS changes (see https://github.com/arq5x/bedtools2/releases/download/v2.30.0/bedtools.static.binary).
##
## Cromwell version support - Successfully tested on v63
##
## Distributed under terms of the MIT License
## Copyright (c) 2021 Brian Sharber
## Contact <brian.sharber@vumc.org>

workflow join_bed {
    input {
        Array[File] input_files    # Input files - need to be bed files.
        String output_name # Output name - user-defined name for the converted file.
        String plink2_docker = "briansha/plink2:latest"
    }

      call Convert_File {
        input:
          input_files = input_files,
          output_name = output_name,
          docker = plink2_docker
    }

    output {
        File output_bed = Convert_File.output_file
    }

    meta {
    	author : "Brian Sharber"
        email : "brian.sharber@vumc.org"
        description : "This workflow concatenates .bed files (containing singular chromosome data) to one .bed file containing all chromosome data the user needs using BEDTOOLS."
    }
}

task Convert_File {
    input {
        Array[File] input_files
        String output_name
        String docker # Docker image containing PLINK2.
        Int? disk_size_override
        Float memory = 3.5
        Int cpu = 1
        Int preemptible = 1
        Int maxRetries = 0
    }
    Float bed_files_size = size(input_files, "GiB")
    Int disk = select_first([disk_size_override, ceil(20.0 + 2.0 * bed_files_size)])

    command <<<
        set -euo pipefail
        wget https://github.com/arq5x/bedtools2/releases/download/v2.30.0/bedtools.static.binary
        mv bedtools.static.binary bedtools
        chmod a+x bedtools
        ./bedtools
        cat ~{sep=' ' input_files} \
        | sort -k1,1 -k2,2n | ./bedtools merge > ~{output_name}
    >>>

    output {
        File output_file = "${output_name}"
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
