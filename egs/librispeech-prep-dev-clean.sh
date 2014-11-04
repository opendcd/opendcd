#!/bin/bash
#./makescp.py `pwd`/LibriSpeech/dev-clean/ > dev-clean.scp
#awk '{print $1,$1}' < dev-clean.scp  > dev-clean.utt2spk
#split -l 676 dev-clean.utt2spk dev-clean-utt2spk.
for f in  aa ab ac ad ;
do 
  echo $f
  ~/tools/kaldi/src/online2bin/online2-wav-nnet2-am-compute \
  --online=false \
  --apply-log=true \
  --config=online_nnet2_decoding.conf \
  nnet_a/final.mdl \
  ark,t:dev-clean-utt2spk.$f \
  "ark:~/tools/kaldi/src/featbin/wav-copy scp,p:dev-clean-scp.$f ark:- |" \
  ark:dev-clean.$f.ark &> dev-clean.$f.log &
done
wait
