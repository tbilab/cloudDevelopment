# Filename: Dockerfile
# No global variables
# Author: Brian Sharber - brian.sharber@vumc.org

FROM ubuntu:18.04

WORKDIR /cromwell_root/

RUN apt-get update && apt-get install -y --no-install-recommends \
      git \
      curl \
      zip \
      unzip \
      gzip \
      g++ \
      make \
      python3 \
      gfortran \
      zlib1g-dev \
      libgfortran4 \
      liblapacke-dev \
      libopenblas-dev \
      libbz2-dev \
      liblzma-dev \
      libssl-dev \
      libfontconfig1-dev \
      libharfbuzz-dev \
      libfribidi-dev \
      libfreetype6-dev \
      libpng-dev \
      libtiff5-dev \
      libjpeg-dev \
      libgit2-dev \
      libxml2-dev \
      libncurses5-dev \
      rsync

# HTSLib
ADD https://github.com/samtools/htslib/releases/download/1.12/htslib-1.12.tar.bz2 .
RUN tar -xvf htslib-1.12.tar.bz2 && rm htslib-1.12.tar.bz2
WORKDIR /cromwell_root/htslib-1.12
RUN ./configure --prefix=/usr/local
RUN make
RUN make install
WORKDIR /cromwell_root/
RUN rm -r htslib-1.12

# R
RUN apt update -qq
RUN apt install -y apt-transport-https software-properties-common
RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E298A3A825C0D65DFD57CBB651716619E084DAB9
RUN add-apt-repository 'deb https://cloud.r-project.org/bin/linux/ubuntu bionic-cran35/'
RUN apt update
RUN DEBIAN_FRONTEND=noninteractive apt install -y --no-install-recommends r-base

# STITCH
RUN git clone --recursive https://github.com/rwdavies/STITCH.git
WORKDIR STITCH
RUN ./scripts/install-dependencies.sh
WORKDIR releases
ADD https://github.com/rwdavies/stitch/releases/download/1.6.6/STITCH_1.6.6.tar.gz .
RUN R CMD INSTALL STITCH_1.6.6.tar.gz
WORKDIR /cromwell_root/

# QUILT, samtools, and bcftools
RUN git clone --recursive https://github.com/rwdavies/QUILT.git
WORKDIR QUILT
RUN ./scripts/install-dependencies.sh
WORKDIR releases
ADD https://github.com/rwdavies/quilt/releases/download/0.1.9/QUILT_0.1.9.tar.gz .
RUN R CMD INSTALL QUILT_0.1.9.tar.gz
WORKDIR /cromwell_root/QUILT
ADD http://www.stats.ox.ac.uk/~rdavies/QUILT_example_2021_01_15A.tgz .
RUN tar -xzvf QUILT_example_2021_01_15A.tgz && rm QUILT_example_2021_01_15A.tgz
RUN cp samtools /usr/local/bin
RUN cp bcftools /usr/local/bin
WORKDIR /cromwell_root/
RUN cp -r QUILT /usr/local/bin
WORKDIR /cromwell_root/QUILT
