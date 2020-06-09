FROM proot-me/proot:centos

RUN yum update -y && \
    yum install -y yum-utils && \
    yum-config-manager --enable extras && \
    yum makecache && \
    yum group install -y 'Development Tools' && \
    yum install -y \
      git \
      libarchive-devel \
      libtalloc-devel \
      python-devel \
      sloccount \
      strace \
      swig \
      uthash-devel

