#!/usr/bin/awk -f
# Convert Dockerfile path to image tag
# input: test/docker/alpine/x86/gcc/Dockerfile
# output: alpine-x86-gcc
{
  gsub(/\//,"-"); # replace / with -
  split($0,a,"test-docker-"); # remove leading directory name
  split(a[2],b,"-Docker"); # remove trailing Dockerfile
  print b[1] # print final tag
}
