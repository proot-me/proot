FROM ghcr.io/proot-me/proot:centos

RUN dnf install -y \
      https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm \
      dnf-plugins-core && \
    dnf config-manager --set-enabled crb && \
    dnf update -y && \
    dnf install -y \
      gcc \
      git \
      libarchive-devel \
      libtalloc-devel \
      lzop \
      make \
      pkg-config \
      python3-devel \
      strace \
      swig \
      uthash-devel
