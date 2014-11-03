#!/bin/bash
~/tools/kaldi/src/online2bin/online2-wav-nnet2-latgen-faster \
  --online=false --do-endpointing=false \
  --config=online_nnet2_decoding.conf \
  --max-active=7000 --beam=15.0 --lattice-beam=6.0 --acoustic-scale=0.1 \
  --word-symbol-table=tmp/words.txt \
  nnet_a/final.mdl \
  tmp/HCLG.fst \
  ark:utt2spk \
  "ark,s,cs:~/tools/kaldi/src/featbin/wav-copy scp,p:wav.scp ark:- |"\
  "ark:|gzip -c > lat.1.gz" 
