#!/usr/bin/env python

#Run on the Kaldi src directory eg
#makekaldibc.py /path/to/kaldi/trunk/src > kaldi.bc
#and source the output . kaldi.bc

import sys,os,stat
import commands

def runcommand(cmd):
    return commands.getoutput(cmd)

cmd = """_CMD() 
{
    local cur prev opts filters len pprev
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    if (( $COMP_CWORD > 2)) ; then
      pprev="${COMP_WORDS[COMP_CWORD-2]}"
    else
      pprev="NULL"
    fi

    opts="OPTS"

    if [[ ${cur} == -* ]] ; then
    COMPREPLY=($(compgen -W "${opts}" -- ${cur}))
    return 0
    fi
}
complete -o default -o nospace -F _CMD CMD"""

def makecompletetions(dir):
  for dirname, dirnames, filenames in os.walk(dir):
      for filename in filenames:
        fn = os.path.join(dirname, filename)
        f = os.path.splitext(filename)[0]
        if ".py" in fn:
          continue
        if ".sh" in fn:
          continue
        if ".pl" in fn:
          continue
        executable = stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH
        if os.path.isfile(fn):
          st = os.stat(fn)
          mode = st.st_mode
          if mode & executable:
            #print fn
            flags = []
            for line in runcommand(fn + " --help").split("\n"):
              #print line
              line = line.strip()
              if line.startswith("--"):
                flags.append(line.split()[0].strip())
            if len(flags) > 0:
              #print fn, flagsa
              opts = " ".join(flags)
              print cmd.replace("OPTS",opts).replace("CMD",f)
              #print f,opts
            #print(fn, oct(mode))



if __name__ == "__main__":
  for i in range(1, len(sys.argv)):
    #print sys.argv[i]
    makecompletetions(sys.argv[i])
