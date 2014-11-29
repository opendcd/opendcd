#!/bin/bash
# Copyright 2014 Paul R. Dixon (info@opendcd.org)
# Simple script to take a wav specified in the first
# argument and run it through the feature extraction
# and decoding pipeline
# Examples assumes the Librispeech smal models have been
# downloaded and
# ./librispeech-decode-wav.sh tmp.wav

../src/kaldibin/online2-wav-nnet2-am-compute2 \
  --online=true \
  --apply-log=true \
  --config=online_nnet2_decoding.conf \
  nnet_a/final.mdl \
  $1 "ark:-" 2> /dev/null| \
../src/bin/dcd-recog \
  --word_symbols_table=words.txt \
  --decoder_type=hmm_lattice \
  --beam=15 \
  --acoustic_scale=0.1 \
  --gen_lattice=true \
  graph_test_tgsmall/arcs.far \
  graph_test_tgsmall/C.det.L.G.fst \
  ark:- /dev/null
