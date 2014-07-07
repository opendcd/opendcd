// fstconfidence.cc
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2013-2014 Yandex LLC
// \file
// Use modified to Kaldi MBR decoder to compute lattice confidence scores
//
#include <iostream>
#include <fst/fstlib.h>

#include <dcd/lattice-arc.h>
#include <dcd/mbr-decode.h>

using namespace std;
using namespace fst;

template<class Arc>
void Nbest(const Fst<Arc>& fst, int n, MutableFst<Arc>* ofst) {
  VectorFst<Arc> ifst(fst);
  Project(&ifst, PROJECT_OUTPUT);
  RmEpsilon(&ifst);
  vector<typename Arc::Weight> d;
  typedef AutoQueue<typename Arc::StateId> Q;
  AnyArcFilter<Arc> filter;
  Q q(ifst, &d, filter);
  ShortestPathOptions<Arc, Q, AnyArcFilter<Arc> > opts(&q, filter);
  opts.nshortest = n;
  opts.unique = true;
  ShortestPath(ifst, ofst, &d, opts);
}

template<class Arc>
void RmWeights(MutableFst<Arc>* fst) {
  ArcMap(fst, RmWeightMapper<Arc>());
}

template<class Arc>
void ToTropical(const Fst<Arc>& ifst, MutableFst<StdArc>* ofst) {
  ArcMap(ifst, ofst, WeightConvertMapper<Arc, StdArc>());
}

template<class Arc>
void ToLog(const Fst<Arc>& ifst, MutableFst<LogArc>* ofst) {
  ArcMap(ifst, ofst, WeightConvertMapper<Arc, LogArc>());
}

REGISTER_FST(VectorFst, KaldiLatticeArc);

DECLARE_int32(v);

int main(int argc, char *argv[]) {
  string usage = "MBR Decode FST files.\n\n  Usage: ";
  usage += argv[0];
  usage += " in.fst [out.fst]\n";
  std::set_new_handler(FailedNewHandler);
  SetFlags(usage.c_str(), &argc, &argv, true);
 
  Fst<KaldiLatticeArc> *fst = Fst<KaldiLatticeArc>::Read("");
  VectorFst<KaldiLatticeArc> ifst(*fst);
  Project(&ifst, PROJECT_OUTPUT);
  RmEpsilon(&ifst);
  StdVectorFst ofst;
  MbrDecode(ifst, &ofst);
  ofst.Write("");
  return 0;
}
