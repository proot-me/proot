FROM ghcr.io/proot-me/proot:debian

USER root

RUN apt-get update && apt-get install -y --no-install-recommends \
      curl \
      docutils-common \
      git \
      libarchive-dev \
      libtalloc-dev \
      make \
      sloccount \
      strace \
      swig \
      uthash-dev \
      xsltproc

USER proot
