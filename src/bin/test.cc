#include<iostream>

#include <fst/fstlib.h>

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

int main(int argc, char *argv[]) {
  StdFst* fst = StdFst::Read("");
  StdVectorFst ofst;
  Nbest(*fst, 10, &ofst);
  RmWeights(&ofst);
  Determinize(&ofst);
  RmEpsilon(&ofst);
  //ofst.Write("det.nbest.fst");
  //Determinize(&ofst);
  //Compose and veto
  return 0;
}
