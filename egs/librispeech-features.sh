#!/bin/bash

#Extract nnet activations for batch and offline testing
#Use with caution as the output can become large

~/tools/kaldi/src/online2bin/online2-wav-nnet2-am-compute \
  --online=true \
  --apply-log=true \
  --config=online_nnet2_decoding.conf \
  nnet_a/final.mdl \
  ark:test-clean.utt2psk \
  "ark:~/tools/kaldi/src/featbin/wav-copy scp,p:test-clean.scp ark:- |" \
  ark:test-clean.ark

