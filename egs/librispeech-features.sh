#!/bin/bash
~/tools/kaldi/src/online2bin/online2-wav-nnet2-am-compute \
  --online=true \
  --apply-log=true \
  --config=online_nnet2_decoding.conf \
  nnet_a/final.mdl \
  ark:utt2spk \
  "ark,s,cs:~/tools/kaldi/src/featbin/wav-copy scp,p:wav.scp ark:- |" \
  ark:feats.ark

