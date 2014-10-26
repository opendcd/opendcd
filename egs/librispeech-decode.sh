#!/bin/bash
~/tools/kaldi/src/online2bin/online2-wav-nnet2-am-compute \
  --online=true \
  --apply-log=true \
  --config=online_nnet2_decoding.conf \
  nnet_a/final.mdl \
  ark:test-clean.utt2psk \
  "ark:~/tools/kaldi/src/featbin/wav-copy scp,p:test-clean.scp ark:- |" \
  ark:- 2> feats.log |\
../src/bin/dcd-recog \
  --word_symbols_table=words.txt \
  --decoder_type=hmm_lattice \
  --beam=15 \
  --acoustic_scale=0.1 \
  graph_test_tgsmall/arcs.far \
  graph_test_tgsmall/C.det.L.G.fst ark:- /dev/null
