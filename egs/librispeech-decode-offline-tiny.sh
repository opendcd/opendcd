#!/bin/bash
 ../src/kaldibin/dcd-recog-kaldi \
  --word_symbols_table=words.txt \
  --decoder_type=hmm_lattice \
  --beam=15 \
  --acoustic_scale=0.1 \
  --gen_lattice=true \
  graph_test_tgsmall/arcs.far \
  graph_test_tgsmall/C.det.L.G.fst \
  ark:dev-clean-tiny.ark \
  test-clean.far
