FROM ghcr.io/proot-me/proot:debian-x86_64

USER root

RUN apt-get update && apt-get install -y --no-install-recommends \
      gcc \
      gdb \
      lcov

USER proot
