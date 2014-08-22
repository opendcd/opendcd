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


#required="$lang/L.fst $lang/G.fst $lang/phones.txt $lang/words.txt $lang/phones/silence.csl $lang/phones/disambig.int $model $tree"
#for f in $required; do
#  [ ! -f $f ] && echo "mkgraph.sh: expected $f to exist" && exit 1;
#done

mkdir -p ${GRAPH}

if [ 2 == 1 ]; then
  fstdeterminize ${LANG}/L_disambig.fst  ${GRAPH}/det.L.fst
fi
${KALDI_ROOT}/src/fstbin/fstcomposecontext \
  --context-size=3 --central-position=1 --binary=false \
  --read-disambig-syms=${LANG}/phones/disambig.int \
  --write-disambig-syms=${GRAPH}/disambig_ilabels_3_1.int \
  ${GRAPH}/ilabels_3_1 ${GRAPH}/det.L.fst | fstarcsort - ${GRAPH}/C.det.L.fst

${KALDI_ROOT}/src/fstbin/fstrmsymbols ${GRAPH}/disambig_ilabels_3_1.int \
  ${GRAPH}/C.det.L.fst - \
 fstconvert --fst_type=olabel_lookahead \
  --save_relabel_opairs=${GRAPH}/g.irelabel ${GRAPH}/C.det.L.fst \
  ${GRAPH}/la.C.det.L.fst

fstrelabel --relabel_opairs=${GRAPH}/g.irelabel ${LANG}/G.fst \
  | fstarcsort - ${GRAPH}/G.fst

fstcompose ${GRAPH}/la.C.det.L.fst ${GRAPH}/G.fst ${GRAPH}/C.det.L.G.fst
