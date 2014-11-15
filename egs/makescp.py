#!/usr/bin/env python
import sys
import os

list_of_files = []
for (dirpath, dirnames, filenames) in os.walk(sys.argv[1]):
  for filename in filenames:
    if filename[-5:] == '.flac': 
      list_of_files.append(os.sep.join([dirpath, filename]))
list_of_files.sort()
for f in list_of_files:
  print f
