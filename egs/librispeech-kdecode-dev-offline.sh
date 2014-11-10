#!/bin/bash
#Script to deccode the segments and score the outputs
b=15
for f in  aa ab ac ad ;
do 
 ../src/kaldibin/dcd-recog-kaldi \
  --word_symbols_table=words.txt \
  --decoder_type=hmm_lattice \
  --beam=15 \
  --acoustic_scale=0.1 \
  --max_arcs=7000 \
  graph_test_tgsmall/arcs.far \
  graph_test_tgsmall/C.det.L.G.fst ark:dev-clean.$f.ark \
  dev-clean.k.$f.far &> dev-clean.k.$f.log &
done
wait

../src/bin/farprintnbeststrings \
  --symbols=words.txt --print_weights=false \
  --format=kaldi --wildcards=3 dev-clean.k.a?.far | \
  compute-wer --text --mode=present ark:- ark:LibriSpeech/dev-clean/text
