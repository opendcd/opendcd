#!/bin/bash
set -x
LANG=$1
EXP=$2
GRAPH=$3
KALDI_ROOT=../../../

tscale=1.0
loopscale=0.1
N=3
P=1

#required="$lang/L.fst $lang/G.fst $lang/phones.txt $lang/words.txt $lang/phones/silence.csl $lang/phones/disambig.int $model $tree"
#for f in $required; do
#  [ ! -f $f ] && echo "mkgraph.sh: expected $f to exist" && exit 1;
#done


mkdir -p ${GRAPH}

if [ 1 == 2 ]; then
fstdeterminize ${LANG}/L_disambig.fst > ${GRAPH}/det.L.fst

${KALDI_ROOT}/src/fstbin/fstcomposecontext \
  --context-size=3 --central-position=1 \
  --read-disambig-syms=${LANG}/phones/disambig.int \
  --write-disambig-syms=${LANG}/disambig_ilabels_3_1.int \
  ${GRAPH}/ilabels_3_1 ${GRAPH}/det.L.fst | fstarcsort > ${GRAPH}/CL.fst

./make-h-transducer --disambig-syms-out=${GRAPH}/h.disambig.int \
  ${GRAPH}/ilabels_3_1 \
  ${EXP}/tree \
  ${EXP}/final.mdl > ${GRAPH}/Ha.fst

fstdeterminize ${GRAPH}/Ha.fst > ${GRAPH}/det.Ha.fst

fstconvert --fst_type=olabel_lookahead \
  --save_relabel_opairs=${GRAPH}/cl.irelabel ${GRAPH}/det.Ha.fst > ${GRAPH}/la.Ha.fst
fstrelabel --relabel_ipairs=${GRAPH}/cl.irelabel ${GRAPH}/CL.fst | fstarcsort \
  | fstcompose ${GRAPH}/la.Ha.fst - > ${GRAPH}/det.HaCL.fst
fi

${KALDI_ROOT}/src/fstbin/fstrmsymbols \
  ${GRAPH}/h.disambig.int ${GRAPH}/det.HaCL.fst \
  | ${KALDI_ROOT}/src/fstbin/fstrmepslocal | fstarcsort \
  | ${KALDI_ROOT}/src/bin/add-self-loops --self-loop-scale=0.1 \
  --reorder=true ${EXP}/final.mdl - > ${GRAPH}/HCL.fst

fstconvert --fst_type=olabel_lookahead \
  --save_relabel_opairs=${GRAPH}/g.irelabel ${GRAPH}/HCL.fst > ${GRAPH}/left.fst

fstrelabel --relabel_opairs=${GRAPH}/g.irelabel ${LANG}/G.fst \
  | fstarcsort > ${GRAPH}/right.fst
