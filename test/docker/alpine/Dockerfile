FROM alpine:latest

COPY . /usr/src/proot

RUN adduser -D -u 1000 proot && \
    chown -R proot:proot /usr/src/proot

WORKDIR /usr/src/proot
USER proot
