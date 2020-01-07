FROM debian:9 AS build
RUN apt-get update && apt-get install -q -y build-essential git libseccomp-dev libtalloc-dev \
 # deps for PERSISTENT_CHOWN extension
 libprotobuf-c-dev libattr1-dev
ADD . PRoot/
RUN cd PRoot \
  && cd src \
  && make && mv proot / && make clean

FROM scratch as proot
COPY --from=build /proot /runrootless-proot

#
# can install the binary using:
#
# docker build --target proot --output ~/.runrootless .
#
#