FROM ghcr.io/proot-me/proot:alpine-x86_64

USER root

RUN apk add \
      --repository=https://dl-cdn.alpinelinux.org/alpine/edge/testing \
      gcc \
      gdb \
      lcov

USER proot
