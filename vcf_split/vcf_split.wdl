version 1.0

## Version 09-18-2021
##
## This workflow splits VCF files into smaller pieces.
## Snapshot 4 showed gunzip, zcat, and pigz to all take around the same time to finish for file decompression.
## pigz - 40 minutes faster than zcat and gunzip when given 7 more CPU than them.
## zcat - 10 minutes faster than gunzip
## gunzip - the slowest.
##
## Using gunzip - it's proven reliable and can use the fewest CPU and resources to get the job done.
## - Can use preemptible node with fewer resources and not have the preemptible node fail - pigz failed using a preemptible node.
##
## Will be using the split command for this workflow to split based on variants.
## 15000 variants per file by default - controlled by the n_variants variable.
##
## Input files need to all be VCF.gz files.
##
## Cromwell version support - Successfully tested on v67
##
## Distributed under terms of the MIT License
## Copyright (c) 2021 Brian Sharber
## Contact <brian.sharber@vumc.org>

workflow vcf_split {
    input {
        Array[File] input_file    # Input files - need to be VCF.gz files.
        Array[String] chr_list    # Chromosome number matching the VCF.gz file - both of the arrays need to match up - chr1.vcf.gz pairs with chr1, etc.
        String docker = "briansha/vcf_split:latest"
    }

    scatter (file_and_chr in zip(input_file, chr_list)) {
      call SplitVcfGunzip {
        input:
          input_file = file_and_chr.left,
          chr = file_and_chr.right,
          docker = docker
        }
    }

    scatter (array_of_files in SplitVcfGunzip.output_vcfs) {
      call CreateIndex {
        input:
          output_vcfs = array_of_files,
          docker = docker
      }
    }

    output {
        Array[Array[File]] output_bgzipped_vcf_indexes = CreateIndex.output_bgzipped_vcf_indexes
        Array[Array[File]] output_bgzipped_vcfs = CreateIndex.output_bgzipped_vcfs
    }

    meta {
    	author : "Brian Sharber"
        email : "brian.sharber@vumc.org"
        description : "This workflow splits vcf.gz files into smaller vcf files - then converts each to a vcf.gz file and creates an index file (vcf.gz.csi)."
    }
}

task SplitVcfGunzip {
    input {
        File input_file
        String chr
        Int n_variants = 15000  # Number of variants to split by - each resulting file will contain this many variants.

        # Runtime
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

    # A vcf file is quite larger than a vcf.gz file. Normally 35.0 * vcf_size, but we're splitting the file - so 35.0 * 2 * vcf_size - but when testing, still ran out of storage...so 90.0 * vcf_size
    Int disk = select_first([disk_size_override, ceil(10.0 + 90.0 * vcf_size)])

    #decompress with gunzip
    #grab the header
    #grab the non header lines
    #split into chunks with n_variants lines
    #reattach the header to each and clean up
    # Useful commands to put at the end:
    # - ls
    # - du -d 1 -h
    command <<<
        set -euo pipefail
        mv ~{input_file} .
        gunzip -d ~{file_for_gunzip}

        head -n 10000 ~{file_for_bgzip} | grep "^#" > header
        grep -v "^#" ~{file_for_bgzip} > variants
        split -l ~{n_variants} variants
        for i in x*; do cat header $i > ~{chr}_${i}.vcf && rm -f $i; done
        rm -f header variants ~{file_for_bgzip}
    >>>

    output {
        Array[File] output_vcfs = glob("*.vcf")
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

task CreateIndex {
    input {
        Array[File] output_vcfs

        # Runtime
        String docker # Docker image containing PLINK2.
        Int? disk_size_override
        Float memory = 3.5
        Int cpu = 1
        Int preemptible = 1
        Int maxRetries = 0
    }
    Float vcf_size = size(output_vcfs, "GiB")
    Int disk = select_first([disk_size_override, ceil(10.0 + 2.0 * vcf_size)]) # A vcf file is quite larger than a vcf.gz file.

    #bgzip the file and create a .csi index for the file
    command <<<
        set -euo pipefail
        for file in ~{sep=' ' output_vcfs}; do \
          bgzip $file; \
          tabix --csi -p vcf ${file}.gz; \
          mv ${file}.gz .; \
          mv ${file}.gz.csi .; \
        done
    >>>

    output {
        Array[File] output_bgzipped_vcf_indexes = glob("*.gz.csi")
        Array[File] output_bgzipped_vcfs = glob("*.gz")
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
