FROM ghcr.io/proot-me/proot:alpine

USER root

RUN apk add --no-cache \
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
      py3-docutils \
      python3-dev \
      strace \
      swig \
      talloc-dev \
      uthash-dev

USER proot
