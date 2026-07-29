FROM ghcr.io/proot-me/proot:alpine

USER root

RUN apk update && \
    apk upgrade && \
    apk add \
      bash \
      bsd-compat-headers \
      coreutils \
      git \
      grep \
      libarchive-dev \
      libxslt \
      linux-headers \
      lzo \
      make \
      mcookie \
      musl-dev \
      python3-dev \
      py3-docutils \
      strace \
      swig \
      talloc-dev \
      uthash-dev

USER proot
