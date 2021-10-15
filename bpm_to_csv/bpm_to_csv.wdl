version 1.0

## Version 06-29-2021
##
## This workflow converts bpm and egt files into a .bpm.csv file using GATK's tool BpmToNormalizationManifestCsv
## Documentation: https://gatk.broadinstitute.org/hc/en-us/articles/360051306971-BpmToNormalizationManifestCsv-Picard-
##
## Cromwell version support - Successfully tested on v65
##
## Distributed under terms of the MIT License
## Copyright (c) 2021 Brian Sharber
## Contact <brian.sharber@vumc.org>

workflow vcf_to_plink {
    input {
        String gatk_docker = "us.gcr.io/broad-gatk/gatk:4.1.9.0"
    }
    call Convert_File {
      input:
        docker = gatk_docker

    }

    output {
        File output_csv = Convert_File.output_csv
    }

    meta {
    	author : "Brian Sharber"
        email : "brian.sharber@vumc.org"
        description : "This workflow converts bpm files to csv files."
    }
}

task Convert_File {
    input {
        File input_file
        File cluster_file
        String docker
        Int disk = 400
        Float memory = 3.5
        Int cpu = 1
        Int preemptible = 1
        Int maxRetries = 0
    }
    String output_prefix = basename(input_file)

    command <<<
        set -euo pipefail
        wget https://github.com/broadinstitute/picard/releases/download/2.25.6/picard.jar
        java -jar picard.jar BpmToNormalizationManifestCsv \
        INPUT=~{input_file} \
        CLUSTER_FILE=~{cluster_file} \
        OUTPUT=~{output_prefix}.csv

        ls
    >>>

    output {
        File output_csv = "${output_prefix}.csv"
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
