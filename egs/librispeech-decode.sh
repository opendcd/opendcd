#!/bin/bash


~/tools/kaldi/src/online2bin/online2-wav-nnet2-am-compute \
  --config=online_nnet2_decoding.conf \
  --online=false \
  nnet_a/final.mdl \
  ark:utt2spk \
  "ark,s,cs:~/tools/kaldi/src/featbin/wav-copy scp,p:wav.scp ark:- |" \
  ark:scores.ark


