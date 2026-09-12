FROM quay.io/centos/centos:stream9

COPY . /usr/src/proot

RUN useradd -m -u 1000 proot && \
    chown -R proot:proot /usr/src/proot

WORKDIR /usr/src/proot
USER proot
