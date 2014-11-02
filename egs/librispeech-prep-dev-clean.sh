#!/bin/bash
./makescp.py `pwd`/LibriSpeech/dev-clean/ > dev-clean.scp
awk '{print $1,$1}' < dev-clean.scp  > dev-clean.utt2spk
