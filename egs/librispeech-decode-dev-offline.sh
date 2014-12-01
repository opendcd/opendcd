#!/bin/bash
# Copyright 2014 Paul R. Dixon (info@opendcd.org)
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
# WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
# MERCHANTABLITY OR NON-INFRINGEMENT.
# See the Apache 2 License for the specific language governing permissions and
# limitations under the License.

# script to decode the pre-computed nnets segments and
# score the outputs, useful for tuning and benchmarking
# the system


b=15
for f in  aa ab ac ad ;
do
 ../src/bin/dcd-recog \
  --word_symbols_table=words.txt \
  --decoder_type=hmm_lattice \
  --beam=15 \
  --acoustic_scale=0.1 \
  --max_arcs=7000 \
  graph_test_tgsmall/arcs.far \
  graph_test_tgsmall/C.det.L.G.fst ark:dev-clean.$f.ark \
  dev-clean.$f.far &> dev-clean.$f.log &
done
wait

../src/bin/farprintnbeststrings \
  --symbols=words.txt --print_weights=false \
  --format=kaldi --wildcards=3 dev-clean.a?.far | \
  compute-wer --text --mode=present ark:- ark:LibriSpeech/dev-clean/text
