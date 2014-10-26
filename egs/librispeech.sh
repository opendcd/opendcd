#!/bin/bash
set -x
#First we fetch the small pre-built models then and the online neural networks
#models
LIBRISPEECH=http://www.kaldi-asr.org/downloads/build/6/trunk/egs/librispeech/s5
LANG=lang_test_tgsmall
mkdir -p $LANG/
mkdir -p $LANG/phones
mkdir -p nnet_a
REQUIRED="phones/disambig.int L_disambig.fst G.fst words.txt"
for F in $REQUIRED; do
  D=dirname $F
  B=basename $F
  if [ ! -f $LANG/$F ]; then
    wget -P $LANG/$D/  $LIBRISPEECH/$DATA/$LANG/$F
  fi
done

REQUIRED="tree final.mdl"
for F in $REQUIRED; do
  if [ ! -f nnet_a/$F ]; then
  wget $LIBRISPEECH exp/nnet2_online/nnet_a/tree -P nnet_a/
  fi
done
#We also need the models for the iVector extractor
IVECTOR=http://www.kaldi-asr.org/downloads/build/6/trunk/egs/librispeech/s5/exp/nnet2_online/nnet_a_online/ivector_extractor
REQUIRED="final.dubm final.ie final.mat global_cmvn.stats online_cmvn.conf splice_opts"
for F in $REQUIRED; do
  if [ ! -f ivector_extractor/$F ]; then
    wget -P ivector_extractor/ $IVECTOR/$F
  fi
done
#Fetch some test data and make s short test
if [ ! -f test-clean.tar.gz ]; then
  wget http://www.openslr.org/resources/12/test-clean.tar.gz
  tar -zxf test-clean.tar.gz
fi

./makescp.py "LibriSpeech/test-clean/1089/134686/" > test-clean.scp
cut -f 1 -d' ' test-clean.scp  > names.txt
paste names.txt names.txt > test-clean.utt2psk

#Build the graph
../script/makeclevel.sh lang_test_tgsmall  nnet_a graph_test_tgsmall ../../kaldi

