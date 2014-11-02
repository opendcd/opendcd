/// farprintnbeststrings.cc
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
// Copyright Paul R. Dixon 2012, 2014
// Copyright Yandex LLC 2013-2014
// file
// Modified version of fstxprintnbeststring to handle FAR input
//

#include <fst/disambiguate.h>
#include <fst/project.h>
#include <fst/push.h>
#include <fst/relabel.h>
#include <fst/rmepsilon.h>
#include <fst/shortest-path.h>
#include <fst/verify.h>
#include <fst/script/arg-packs.h>
#include <fst/script/script-impl.h>
#include <fst/extensions/far/far.h>
#include <fst/extensions/far/main.h>
#include <fst/extensions/far/farscript.h>

#include <dcd/kaldi-lattice-arc.h>
#include <dcd/mbr-decode.h>
#include <dcd/utils.h>


using namespace std;

DEFINE_string(symbols, "", "");
DEFINE_string(wildcards, "", "");
DEFINE_bool(print_weights, true, "");
DEFINE_bool(reverse, false, "");
DEFINE_bool(mbr, false, "");
DEFINE_bool(unique, true, "Unique nbest");
DEFINE_int32(nshortest, 1, "# of shortest paths");
DEFINE_bool(verify, false, "");
DEFINE_bool(push, false, "");
DEFINE_bool(to_real, false, "Print costs in the real semiring");
DEFINE_string(format, "", "Format output");
DEFINE_string(disambiguate, "", "");

namespace fst {

// Compute all the simple paths in an acyclic WFST
// It is assumed that final states have no out-going arcs
template<class Arc, class C>
int AllPaths(const Fst<Arc>& fst, C& c) {
  typedef typename Arc::StateId S;
  typedef typename Arc::Weight W;
  typedef typename Arc::Label L;
  typedef ArcIterator< Fst<Arc> > AI;

  struct Element {
    L label;
    W cost;
  };

  vector<AI*> arc_iterators;
  list<L> current_path;
  list<W> current_cost;
  current_cost.push_back(W::One());
  int num_paths = 0;

  S start = fst.Start();
  if (start == kNoStateId) {
    return -1;
  }

  LifoQueue<S> queue;
  queue.Enqueue(start);

  while (!queue.Empty()) {
    S s = queue.Head();
    queue.Dequeue();
    if ( s >= arc_iterators.size())
      arc_iterators.resize(s + 1);

    if (arc_iterators[s] == NULL)
      arc_iterators[s] = new AI(fst, s);

    AI& iter = *arc_iterators[s];
    if (fst.Final(s) == W::Zero()) {
      if (!iter.Done()) {
        queue.Enqueue(s);
        const Arc& arc = iter.Value();
        iter.Next();
        current_path.push_back(arc.ilabel);
        current_cost.push_back(Times(arc.weight, current_cost.back()));
        queue.Enqueue(arc.nextstate);
      } else {
        if (current_path.size()) {
          current_path.pop_back();
          current_cost.pop_back();
        }
        if (s != fst.Start()) {
          delete arc_iterators[s];
          arc_iterators[s] = NULL;
        }
      }
    } else {
      if (!iter.Done())
        LOG(FATAL) << "Final state has out going arcs";
      c(current_path, current_cost.back());
      current_path.pop_back();
      num_paths++;
    }
  }

  for (size_t i = 0; i != arc_iterators.size(); ++i)
    delete  arc_iterators[i];
  return num_paths;
}

namespace script {

typedef args::Package<const vector<string>&, const string&>
    FarPrintNBestStringsArgs;

template <class Label, class Weight>
struct PathInserter {
  PathInserter(const SymbolTable* syms, vector<pair<string, Weight> >* strings)
      : syms_(syms), strings_(strings) { }

  template<class Iterator>
void PathToString(Iterator begin, Iterator end, stringstream& ss) {
  for ( ; begin != end; ++begin) {
    const Label& l = *begin;
    if (l == 0)
      continue;
    if (syms_ == 0)
      ss << l << " ";
    else
      ss << syms_->Find(l) << " ";
  }
}

void operator()(const list<Label>& path, const Weight& w) {
  stringstream ss;
  if (FLAGS_reverse)
    PathToString(path.rbegin(), path.rend(), ss);
  else
    PathToString(path.begin(), path.end(), ss);
  strings_->push_back(pair<string, Weight>(ss.str(), w));
}

const SymbolTable* syms_;
vector<pair<string, Weight> >* strings_;
};


template<class Arc>
void NShortest(const Fst<Arc>& ifst, MutableFst<Arc> *nbest) {
  vector<typename Arc::Weight> d;
  typedef AutoQueue<typename Arc::StateId> Q;
  AnyArcFilter<Arc> filter;
  Q q(ifst, &d, filter);
  ShortestPathOptions<Arc, Q, AnyArcFilter<Arc> > opts(&q, filter);
  opts.nshortest = FLAGS_nshortest;
  opts.unique = FLAGS_unique;
  fst::ShortestPath(ifst, nbest, &d, opts);
}


template<class Weight>
float ToReal(const Weight& w) {
  return expf(-w.Value());
}

float ToReal(const KaldiLatticeWeight& w) {
  return expf(-w.Value1() -w.Value2());
}

template<class Arc>
void PrintStrings(const Fst<Arc>& ifst, const string& key,
                  const SymbolTable* symbols, int utt) {
  typedef typename Arc::Weight Weight;
  typedef typename Arc::Label Label;
  if (FLAGS_verify && !Verify(ifst))
    LOG(FATAL) << "Bad fst detected : " << key;
  if (ifst.Start() != kNoStateId) {
    vector<pair<string, Weight> > strings;
    PathInserter<Label, Weight> pi(symbols, &strings);
    AllPaths(ifst, pi);
    stringstream ss;
    for (int i = 0; i != strings.size(); ++i) {
      if (FLAGS_print_weights) {
        if (FLAGS_to_real)
          ss <<  ToReal(strings[i].second) << " ";
        else
          ss <<  strings[i].second << " ";
      }
      if (FLAGS_format == "rnnlm")
        ss << utt << " ";
      ss << strings[i].first;
      if (FLAGS_format == "sclite")
        ss << "(" << key << ")";
      ss << "\n";
    }
    cout << ss.str();
  } else {
    LOG(ERROR) << "No best for : " << key;
  }
}


template<class Arc>
void FarPrintNBestStrings(FarPrintNBestStringsArgs* args) {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  if (FLAGS_symbols.empty())
    FSTERROR() << "--symbols flag must be set";
  SymbolTable* symbols = SymbolTable::ReadText(FLAGS_symbols);
  if (!symbols) {
    return;
  }

  FarReader<Arc>* reader = FarReader<Arc>::Open(args->arg1);
  if (!reader) {
    return;
  }

  vector<pair<Label, Label> > relabel_pairs;
  if (!FLAGS_wildcards.empty()) {
    char* str = new char[FLAGS_wildcards.size() + 1];
    strcpy(str, FLAGS_wildcards.c_str());
    vector<char*> fields;
    SplitToVector(str, ",", &fields, true);
    for (int i = 0; i != fields.size(); ++i) {
      Label n = atoi(fields[i]);
      relabel_pairs.push_back(pair<Label, Label>(n , 0));
    }
    delete[] str;
  }

  for (int utt = 0; !reader->Done(); reader->Next(), ++utt) {
    VectorFst<Arc> ifst(reader->GetFst());
    string key = reader->GetKey();
    int ndx = key.find_first_of('_');
    if (ndx > 0)
      key = key.substr(ndx + 1, key.size() - ndx - 1);
    fst::Project(&ifst, PROJECT_OUTPUT);
    if (relabel_pairs.size())
      fst::Relabel(&ifst, relabel_pairs, relabel_pairs);
    fst::RmEpsilon(&ifst);
    VectorFst<Arc> nbest;
    NShortest(ifst, &nbest);
    PrintStrings(nbest, key, symbols, utt);
  }
}

void FarPrintNBestStrings(const vector<string>& ifilenames,
                          const string& arc_type) {
  FarPrintNBestStringsArgs args(ifilenames, arc_type);
  Apply< Operation<FarPrintNBestStringsArgs> >("FarPrintNBestStrings",
                                               arc_type, &args);
}

REGISTER_FST_OPERATION(FarPrintNBestStrings, StdArc, FarPrintNBestStringsArgs);
REGISTER_FST_OPERATION(FarPrintNBestStrings, KaldiLatticeArc,
                       FarPrintNBestStringsArgs);
}  // namespace script

REGISTER_FST(VectorFst, KaldiLatticeArc);
// REGISTER_FST(ConstFst, KaldiLatticeArc);
}  // namespace fst


using namespace fst;

int main(int argc, char **argv) {
  namespace s = fst::script;

  string usage = "Useful nbest string printer for FAR files.\n\n  Usage: ";
  usage += argv[0];
  usage += " in.fst [out.text]\n";

  std::set_new_handler(FailedNewHandler);
  SetFlags(usage.c_str(), &argc, &argv, true);

  vector<string> ifilenames;
  for (int i = 1; i < argc; ++i)
    ifilenames.push_back(strcmp(argv[i], "") != 0 ? argv[i] : "");
  if (ifilenames.empty())
    ifilenames.push_back("");

  string arc_type = fst::LoadArcTypeFromFar(ifilenames[0]);
  s::FarPrintNBestStrings(ifilenames, arc_type);
  return 0;
}
