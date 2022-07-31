#!/bin/sh
# Runs every size and sampler, a few at a time. Takes about an hour on an M2.
clang -O2 -o factor factor.c -lm
mkdir -p results/factor
for n in 3 4 5 6 7 8 9 10 11 12; do for m in 0 1 2; do echo "$n $m"; done; done |
  xargs -P 6 -L 1 sh -c './factor $0 $1 300000000 > results/factor/n$0_m$1.json'
