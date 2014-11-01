#!/usr/bin/env python
import sys
import os

list_of_files = {}
for (dirpath, dirnames, filenames) in os.walk(sys.argv[1]):
  for filename in filenames:
    if filename[-5:] == '.flac': 
      print "%s  flac -c -d -s %s |"% (filename[:-5], os.sep.join([dirpath, filename]))
