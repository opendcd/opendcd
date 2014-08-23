#!/bin/bash
set -x
LANG=$1
EXP=$2
GRAPH=$3
KALDI_ROOT=$4

tscale=1.0
loopscale=0.1
N=3
P=1

if [ "$#" -ne 4 ]; then
  echo "usage: makeclevel.sh lang.dir[in] model.dir[in] graph.dir[out] kaldi.root[in]"
  exit 1;
fi

required="$LANG/L_disambig.fst $LANG/phones/disambig.int $LANG/G.fst"
for f in $required; do
  [ ! -f $f ] && echo "mkgraph.sh: expected $f to exist" && exit 1;
done

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/fst/

mkdir -p ${GRAPH}

if [ 1 == 1 ]; then
  fstdeterminize ${LANG}/L_disambig.fst  ${GRAPH}/det.L.fst

${KALDI_ROOT}/src/fstbin/fstcomposecontext \
  --context-size=3 --central-position=1 --binary=false \
  --read-disambig-syms=${LANG}/phones/disambig.int \
  --write-disambig-syms=${GRAPH}/disambig_ilabels_3_1.int \
  ${GRAPH}/ilabels_3_1 ${GRAPH}/det.L.fst | fstarcsort - ${GRAPH}/C.det.L.fst

${KALDI_ROOT}/src/fstbin/fstrmsymbols ${GRAPH}/disambig_ilabels_3_1.int \
  ${GRAPH}/C.det.L.fst - | fstconvert --fst_type=olabel_lookahead \
  --save_relabel_opairs=${GRAPH}/g.irelabel ${GRAPH}/C.det.L.fst \
  ${GRAPH}/la.C.det.L.fst

fi

fstrelabel --relabel_opairs=${GRAPH}/g.irelabel ${LANG}/G.fst \
  | fstarcsort | fstconvert --fst_type=const - ${GRAPH}/G.fst

fstcompose ${GRAPH}/la.C.det.L.fst ${GRAPH}/G.fst | \
  fstconvert --fst_type=const - ${GRAPH}/C.det.L.G.fst 
