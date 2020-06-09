#!/usr/bin/awk -f
# Convert Dockerfile path to image tag
# input: ./docker/alpine/x86_64/gcc/Dockerfile
# output: alpine-x86_64-gcc
{
  gsub(/\//,"-"); # replace / with -
  split($0,a,"docker-"); # remove leading directories
  split(a[2],b,"-Docker"); # remove trailing Dockerfile
  print b[1] # print final tag
}
