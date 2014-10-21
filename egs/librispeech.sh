#!/bin/bash

if [ ! -f 3-gram.pruned.1e-7.arpa.gz ]; then
  wget http://www.openslr.org/resources/11/3-gram.pruned.1e-7.arpa.gz
fi

if [ ! -f 4-gram.arpa.gz ]; then
  wget http://www.openslr.org/resources/11/4-gram.arpa.gz
fi
