#!/bin/bash
set -x
./makescp.py `pwd`/LibriSpeech/dev-clean/ | head -n 10 > dev-clean-tiny.scp
awk '{print $1,$1}' < dev-clean-tiny.scp  > dev-clean-tiny.utt2spk

~/tools/kaldi-trunk/src/online2bin/online2-wav-nnet2-am-compute \
--online=false \
--apply-log=true \
--config=online_nnet2_decoding.conf \
nnet_a/final.mdl \
ark,t:dev-clean-tiny.utt2spk \
"ark:~/tools/kaldi-trunk/src/featbin/wav-copy scp,p:dev-clean-tiny.scp ark:- |" \
ark:dev-clean-tiny.ark &> dev-clean-tiny.log
