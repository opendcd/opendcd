#!/bin/bash

#First build a cascade with the small cascade and the nnet_a
mkdir -p data_test_tgsmall
mkdir -p data_test_tgsmall/phones
mkdir -p nnet_a

#wget http://www.kaldi-asr.org/downloads/build/6/trunk/egs/librispeech/s5/data/lang_test_tgsmall/{L_disambig.fst,G.fst,words.txt}
#wget http://www.kaldi-asr.org/downloads/build/6/trunk/egs/librispeech/s5/data/lang_test_tgsmall/phones/disambig.int -P data_test_tgsmall/phones/

if [ ! -f nnet_a/tree ]; then
  wget http://www.kaldi-asr.org/downloads/build/6/trunk/egs/librispeech/s5/exp/nnet2_online/nnet_a/tree -P nnet_a/
fi

if [ ! -f nnet_a/final.mdl ]; then
  wget http://www.kaldi-asr.org/downloads/build/6/trunk/egs/librispeech/s5/exp/nnet2_online/nnet_a/final.mdl -P nnet_a/
fi

#Build the graph
../script/makeclevel.sh data_test_tgsmall nnet_a graph_test_tgsmall ../../kaldi/

exit
#Now fetch some larger language models
if [ ! -f 3-gram.pruned.1e-7.arpa.gz ]; then
  wget http://www.openslr.org/resources/11/3-gram.pruned.1e-7.arpa.gz
fi

if [ ! -f 4-gram.arpa.gz ]; then
  wget http://www.openslr.org/resources/11/4-gram.arpa.gz
fi


online2-wav-nnet2-latgen-faster --online=false --do-endpointing=false
--config=exp/nnet2_online/nnet_a_online/conf/online_nnet2_decoding.conf
--max-active=7000 --beam=15.0 --lattice-beam=6.0 --acoustic-scale=0.1
--word-symbol-table=exp/tri6b/graph_tgsmall/words.txt
exp/nnet2_online/nnet_a_online/final.mdl exp/tri6b/graph_tgsmall/HCLG.fst
ark:exp/nnet2_online/nnet_a_online/decode_dev_clean_tgsmall_utt_offline/per_utt/utt2spk.1
"ark,s,cs:wav-copy scp,p:data/dev_clean/split30/1/wav.scp ark:- |" "ark:|gzip -c
> exp/nnet2_online/nnet_a_online/decode_dev_clean_tgsmall_utt_offline/lat.1.gz" 
