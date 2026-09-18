FROM ghcr.io/proot-me/proot:debian

USER root

RUN apt-get update && apt-get install -y --no-install-recommends \
      check \
      curl \
      docutils-common \
      git \
      libarchive-dev \
      libtalloc-dev \
      make \
      pkg-config \
      sloccount \
      strace \
      swig \
      uthash-dev \
      xsltproc

USER proot
