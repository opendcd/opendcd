#!/bin/bash
./makescp.py `pwd`/LibriSpeech/dev-clean/ > dev-clean-all.scp
awk '{print $1,$1}' < dev-clean-all.scp  > dev-clean-all.utt2spk

~/tools/kaldi-trunk/src/online2bin/online2-wav-nnet2-am-compute \
--online=true \
--apply-log=true \
--config=online_nnet2_decoding.conf \
nnet_a/final.mdl \
ark,t:dev-clean-all.utt2spk \
"ark:~/tools/kaldi-trunk/src/featbin/wav-copy scp,p:dev-clean-all.scp ark:- |" \
ark:- | \
../src/kaldibin/dcd-recog-kaldi \
  --word_symbols_table=words.txt \
  --decoder_type=hmm_lattice \
  --beam=15 \
  --acoustic_scale=0.1 \
  --gen_lattice=true \
  graph_test_tgsmall/arcs.far \
  graph_test_tgsmall/C.det.L.G.fst \
  ark:- dev-clean-all.far
