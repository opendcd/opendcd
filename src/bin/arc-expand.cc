// arc-expand.h
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
// Expand the arc definition inside of an FST
#include <iostream>

#include <fst/arcfilter.h>
#include <fst/compat.h>
#include <fst/flags.h>
#include <fst/fst.h>
#include <fst/arcfilter.h>
#include <fst/queue.h>
#include <fst/shortest-distance.h>
#include <fst/verify.h>
#include <fst/extensions/far/far.h>

#include <dcd/arc-expand-fst.h>
#include <dcd/text-utils.h>
#include <dcd/utils.h>

using namespace fst;
using namespace std;
using namespace dcd;

template<class Arc>
bool ReadArcTypesFromFar(const string& path, vector<const Fst<Arc>*>* arcs) {
  FarReader<Arc>* reader = FarReader<Arc>::Open(path);
  if (!reader) {
    FSTERROR() << "Failed to open arc types file : " << path;
    return false;
  }
  for (; !reader->Done(); reader->Next()) {
    const Fst<Arc>& fst = reader->GetFst();
    VectorFst<Arc>* arc = new VectorFst<Arc>(fst);
    arcs->push_back(arc);
  }
  return true;
}

DEFINE_double(scale, 1.0, "Arc scaling factor");


struct TropicalWeightCompare {
  bool operator() (const TropicalWeight& a, 
      const TropicalWeight& b) {
    return a.Value() < b.Value();
  }
};

template<class Arc>
void EpsRmState(const Fst<Arc>& fst, int s) {
  vector<TropicalWeight> d;
  FifoQueue<int> q;
  EpsilonArcFilter<Arc> filter;
  ShortestDistanceState<StdArc, FifoQueue<int>, EpsilonArcFilter<Arc> > 
    sd_state(fst, &d, ShortestDistanceOptions<StdArc, FifoQueue<int>, 
        EpsilonArcFilter<Arc> >(&q, filter), true);
  sd_state.ShortestDistance(s);
  std::sort(d.begin(), d.end(), TropicalWeightCompare());
  int num_states = 0;
  for (int i = 0; i != d.size(); ++i) {
    if (d[0] == TropicalWeight::Zero())
      break;
    ++num_states;
  }
  LOG(INFO) << "#Num of states " << num_states;
}


int main(int argc, char **argv) {
 
  string usage = "Expand a CLG network down to the HCLG level.\n\n  Usage: ";
  usage += argv[0];
  usage += " [in.fst in.arcs [out.fst]]\n";

  std::set_new_handler(FailedNewHandler);
	SetFlags(usage.c_str(), &argc, &argv, true);
  //SET_FLAGS(usage.c_str(), &argc, &argv, true);

	if (argc < 2 || argc > 4) {
    ShowUsage();
    return 1;
  }

  VLOG(2) << "Reading fst from " << argv[1];
  
  StdFst* clg = StdFst::Read(argv[1]);
  if (!clg)
    FSTERROR() << "Failed to read fst from : " << argv[1];
  
  VLOG(2) << "Attempting to read arcs from " << argv[2];
  vector<const StdFst*> arcs;
  
  ReadFstArcTypes(argv[2], &arcs, 1.0f, false);
  
  VLOG(2) << "Read " << arcs.size() <<  " types";

  for (int i = 0; i < arcs.size();  ++i) {
    if (CountStates(*arcs[i]) == 6)
      LOG(INFO) << "Silence model " << i;
  }
  
  for (int i = 0; i != arcs.size(); ++i) {
    const StdFst& fst = *arcs[i];
    if (CountStates(fst) == 2)
      VLOG(2) << "Epsilon transition detected " <<  i;
  } 
  //LOG(INFO) << "CLG Closure";
  //EpsRmState(*clg, clg->Start());
  StdArcExpandFst hclg(*clg, arcs);
  //LOG(INFO) << "HCLG Closure";
  //EpsRmState(hclg, clg->Start());
  StdVectorFst out;
  ArcExpand(*clg, arcs, &out);
  out.Write(argc == 4 ? argv[3] : "");
  return 0;
}


