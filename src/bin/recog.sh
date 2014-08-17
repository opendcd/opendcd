#!/bin/bash
ROOT=/Users/dixonp/Desktop/opendcd/test
nice -n 19 ./dcd-recog \
  --verbose=0 \
  --decoder_type=hmm_lattice_kaldi \
  --beam=11 \
  --acoustic-scale=0.11 \
  --trans-scale=0.1 \
  --word_symbols_table=${ROOT}/words.txt \
  --use_lattice_pool=true \
  --gc_period=25 \
  --gc_check=false \
  --gen_lattice=true \
  --use_search_pool=false \
  --cache_dest_states=true \
  --early_mission=false \
  --dump_traceback=false \
  --acoustic_lookahead=1 \
  ${ROOT}/subset.arcs.fsts \
  ${ROOT}/CLG.lkhd.relabel.const.fst \
  ark:${ROOT}/testset.gmm.likes lats.far
