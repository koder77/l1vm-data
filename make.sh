#!/bin/sh
export CC=clang && export CCPP=clang++ && zerobuild force
clang prog/client.c -o prog/client
l1com prog/3n1-client
l1com prog/3n1-server
l1com prog/l1vm-data
