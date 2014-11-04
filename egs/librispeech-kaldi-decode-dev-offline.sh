#!/bin/bash
#Script to launch Kaldi decoder on the precompute features.
#Used as a baseline comparison
#Author : Paul R. Dixon
set -x
b=15
for f in  aa ab ac ad ;
do 
~/tools/kaldi/src/bin/latgen-faster-mapped \
  --max-active=7000 --beam=$b --acoustic-scale=0.1 \
  --word-symbol-table=tmp/words.txt \
  nnet_a/final.mdl \
  tmp/HCLG.fst \
  ark:dev-clean.$f.ark \
  ark:dev-clean.kaldi.decode.$f.ark &> dev-clean.kaldi.decode.$f.log &
done
wait
