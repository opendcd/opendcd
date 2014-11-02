#!/bin/bash

for f in  aa ab ac ad ;
do 
 ../src/bin/dcd-recog \
  --word_symbols_table=words.txt \
  --decoder_type=hmm_lattice \
  --beam=10 \
  --acoustic_scale=0.1 \
  graph_test_tgsmall/arcs.far \
  graph_test_tgsmall/C.det.L.G.fst ark:dev-clean.$f.ark \
  dev-clean.$f.far &> dev-clean.$f.log &
done

wait
