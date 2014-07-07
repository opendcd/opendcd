// fstexprintstrings.cc
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
// Copyright 2013-2014 Paul R. Dixon
//
// /file 
// Print paths from acyclic FST

#include <fstream>
#include <list>
#include <vector>
#include <fst/util.h>
#include <fst/script/fst-class.h>
#include <fst/script/arg-packs.h>
#include <fst/script/script-impl.h>
#include <fst/symbol-table.h>
#include <fst/fstlib.h>
#include <fst/visit.h>

using namespace std;

DEFINE_string(isymbols, "", "");
DEFINE_string(osymbols, "", "");
DEFINE_bool(acceptor, true, "");
DEFINE_bool(print_epsilons, false, "");
DEFINE_bool(print_weights, true, "");
DEFINE_bool(reverse, true, "");

namespace fst {

//Compute all the simple paths in an acyclic WFST
//It is assumed that final states have no out-going arcs
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

  while(!queue.Empty()) {
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
  typedef args::Package<const FstClass&,
                        const SymbolTable*,
                        vector<string>* > PrintStringsArgs;

template <class Label, class Weight>
struct PathInserter {
  PathInserter(const SymbolTable* syms, vector<string>* strings) 
    : syms_(syms), strings_(strings) { }

  template<class Iterator>
  void PathToString(Iterator begin, Iterator end, stringstream& ss) {  
    for ( ; begin != end; ++begin) {
      const Label& l = *begin;
      if (l == 0 && !FLAGS_print_epsilons)
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
    //typename list<Label>::const_reverse_iterator it = path.rbegin();
    if (FLAGS_print_weights)
        ss << "(" << w << ")";
    strings_->push_back(ss.str());
    //reverse(strings_->begin(), strings_->end());
  }

  const SymbolTable* syms_;
  vector<string>* strings_;
};

  template<class Arc>
  void PrintStrings(PrintStringsArgs* args) {
    //LOG(INFO) << "Print strings";
    typedef typename Arc::StateId StateId;
    typedef typename Arc::Label Label;
    typedef typename Arc::Weight Weight;
    const Fst<Arc> &fst = *(args->arg1.GetFst<Arc>());
    PathInserter<Label, Weight> pi(args->arg2, args->arg3);
    AllPaths(fst, pi);
  }

void PrintStrings(const FstClass &ifst,
                  const SymbolTable* isyms,
                  vector<string>* strings) {
   PrintStringsArgs args(ifst, isyms, strings);

   Apply< Operation<PrintStringsArgs> >("PrintStrings",
                                        ifst.ArcType(), &args);
}

typedef LexicographicArc<TropicalWeight, TropicalWeight> TLexArc;

REGISTER_FST_OPERATION(PrintStrings, TLexArc, PrintStringsArgs);
REGISTER_FST_OPERATION(PrintStrings, StdArc, PrintStringsArgs);
REGISTER_FST_OPERATION(PrintStrings, LogArc, PrintStringsArgs);
}  // namespace script
}  // namespace fst


using namespace fst;
int main(int argc, char **argv) {
  namespace s = fst::script;

  string usage = "Prints the paths in a FST.\n\n  Usage: ";
  usage += argv[0];
  usage += " in.fst [out.text]\n";

  std::set_new_handler(FailedNewHandler);
  SetFlags(usage.c_str(), &argc, &argv, true);
  if (argc > 3) {
    ShowUsage();
    return 1;
  }

  const SymbolTable* isyms = NULL;
  const SymbolTable* osyms = NULL;

  if (!FLAGS_isymbols.empty()) {
    isyms = SymbolTable::ReadText(FLAGS_isymbols);
    if (!isyms) 
      return 1;
  }

  if (!FLAGS_osymbols.empty()) {
    osyms = SymbolTable::ReadText(FLAGS_isymbols);
    if (osyms) 
      return 1;
  }


  string in_name = (argc > 1 && (strcmp(argv[1], "-") != 0)) ? argv[1] : "";
  string out_name = argc > 2 ? argv[2] : "";

  s::FstClass *ifst = s::FstClass::Read(in_name);
  if (!ifst)
    return 1;

  if (isyms == NULL)
      isyms = ifst->InputSymbols();

  if (osyms == NULL)
      osyms = ifst->OutputSymbols();
  
  ofstream ofs;
  ostream* os;
  if (out_name != "") {
    ofs.open(out_name.c_str());
    if (!ofs.is_open())
      LOG(FATAL) << "Failed to open output file : ";
    os = &ofs;
  } else {
    os = &cout;
  }

  vector<string> strings;
  //s::PrintStringsArgs args(*ifst, isyms, &strings);
  s::PrintStrings(*ifst, isyms, &strings);
  
  for (vector<string>::iterator it = strings.begin(); it != strings.end();
    ++it) {
      cout << *it << endl;
  }
  return 0;
}
