// dcd-lex.cc
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
// Copyright 2014 Paul R. Dixon (info@opendcd.org)
// \file
// Construct a simple lexicon transducer from a lexicon in CMUDict format.
// This approach can perform a direct deterministic transducer and therefore
// does not need the determinize operation or the addition of auxillary
// symbols. The algorithm reverse sorts the pronucations to make splitting of a
// final transitions trival for the case when one pronucation is the prefix of
// another word.
// TODO(Paul) Add a progress bar when building
#include <fstream>
#include <iostream>

#include <fst/vector-fst.h>

using namespace std;
using namespace fst;

DECLARE_string(fst_field_separator);

DEFINE_string(save_isymbols, "", "Save input relabel pairs to file");
DEFINE_string(save_osymbols, "", "Save output relabel pairs to file");
DEFINE_string(isymbols, "", "Input label symbol table");
DEFINE_string(osymbols, "", "Input output symbol table");
DEFINE_bool(keep_isymbols, true, "Store input label symbol table with Lex FST");
DEFINE_bool(keep_osymbols, true, "Store input label symbol table with Lex FST");
DEFINE_string(closure_type, "start", "Closure type, one of:"
              "\"start\", \"none\", \"arc\"");
DEFINE_bool(deterministic, true, "Perform a direct deterministic"
            "construction of the lexicon transducer");
DEFINE_string(aux_type, "none", "Auxilary symboles type, one of:"
              "\"none\", \"word\"");
DEFINE_bool(sort, true, "Perform sorting of the lexicon");

static const int kLineLen = 8094;

template<class Arc>
class LexCompiler {
 public:
  typedef typename Arc::Weight Weight;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef Fst<Arc> FST;

  LexCompiler(istream& istrm, SymbolTable* isyms, SymbolTable* osyms) {
    string separator = " \t";//FLAGS_fst_field_separator + "\n";
    char line[kLineLen];
    int nline = 0;
    while (istrm.getline(line, kLineLen)) {
      ++nline;
      vector<char *> col;
      SplitToVector(line, separator.c_str(), &col, true);
      if (col.size() == 0 || col[0][0] == '\0' || col[0][0] == '#')  // empty line
        continue;
      LexEntry entry;
      for (int i = 1; i < col.size(); ++i)
        entry.phonemes.push_back(isyms->AddSymbol(col[i]));
      entry.word = osyms->AddSymbol(col[0]);
      entry.id = 1;
      entry.line = line;
      entries_.push_back(entry);
    }
    VLOG(1) << "Parsed  " << entries_.size() << " lex entries";
    // Reverse sort the lexicon so prefixes get different final
    // transitions then perform direc construction of the lexicon
    if (FLAGS_sort)
      std::sort(entries_.begin(), entries_.end(), LexCompare());
    StateId s = lexfst_.AddState();
    lexfst_.SetStart(s);
    StateId f = FLAGS_closure_type == "start" ? s : lexfst_.AddState();
    lexfst_.SetFinal(f, Weight::One());
    if (FLAGS_closure_type == "arc")
      lexfst_.AddArc(f, Arc(0, 0, 0, s));
    for (int i = 0; i != entries_.size(); ++i) {
      s = lexfst_.Start();
      const LexEntry& entry = entries_[i];
      const vector<Label>& phonemes = entry.phonemes;
      for (int j = 0; j != phonemes.size(); ++j) {
        Label phone = phonemes[j];
        Label word = (j + 1) == phonemes.size() ? entry.word : 0;
        ArcIterator<FST> aiter(lexfst_, s);
        aiter.Seek(phone);
        // If we are not performing a direct construction then
        // always split the prefixes
        if (aiter.Done() || !FLAGS_deterministic) {
          StateId d = (j + 1) == phonemes.size() ? f : lexfst_.AddState();
          lexfst_.AddArc(s, Arc(phone, word, 0, d));
          s = d;
        } else {
          if (j + 1 == phonemes.size()) {
            // last transition which is a prefix of another sequence so split
            lexfst_.AddArc(s, Arc(phone, word, 0, f));
          } else {
            // transition already exis
            s = aiter.Value().nextstate;
          }
        }
      }
    }
    if (FLAGS_keep_isymbols)
      lexfst_.SetInputSymbols(isyms);
    if (FLAGS_keep_osymbols)
      lexfst_.SetOutputSymbols(osyms);
  }

  const Fst<Arc>& GetFst() const { return lexfst_; }

 private:
  struct LexEntry {
    vector<Label> phonemes;
    Label word;
    Weight weight;
    int id;
    string line;
    string ToString(const SymbolTable& isyms, const SymbolTable& osyms) {
      stringstream ss;
      ss << osyms.Find(word);
      for (size_t i = 0; i != phonemes.size(); ++i)
        ss << " " << isyms.Find(phonemes[i]) << "(" << phonemes[i] << ")";
      return ss.str();
    }
  };

  struct LexCompare {
    bool operator () (const LexEntry& a, const LexEntry& b) const {
      return !lexicographical_compare(a.phonemes.begin(), a.phonemes.end(),
                                      b.phonemes.begin(), b.phonemes.end());
    }
  };
  vector<LexEntry> entries_;
  VectorFst<Arc> lexfst_;
  DISALLOW_COPY_AND_ASSIGN(LexCompiler);
};

SymbolTable* ReadOrCreateSymbolTable(const string& path) {
  if (FLAGS_isymbols.empty()) {
    SymbolTable* syms = new SymbolTable("");
    syms->AddSymbol("<EPS>", 0);
    return syms;
  }
  SymbolTable* syms = SymbolTable::ReadText(FLAGS_isymbols);
  if (!syms)
    FSTERROR() << "ReadOrCreateSymbolTable : Failed to read text symbol table from" << path;
  string eps = syms->Find(int64(0));
  if (eps.size() && eps != "<EPS>")
    FSTERROR() << "ReadOrCreateSymbolTable : 0 is assigned to non-epsilon symbol";
  syms->AddSymbol("<EPS>", 0);
  return syms;
}

int main(int argc, char **argv) {
  string usage = "Compile a lexicon to an FST";
  set_new_handler(FailedNewHandler);
  SET_FLAGS(usage.c_str(), &argc, &argv, true);
  if (argc > 3) {
    ShowUsage();
    return 1;
  }

  ifstream istrm(argc == 1 ? "/dev/stdin" : argv[1]);
  if (!istrm.is_open()) {
    FSTERROR() << "Couldn't open input file : " << argv[1];
    return 1;
  }

  SymbolTable* isyms = ReadOrCreateSymbolTable(FLAGS_isymbols);
  SymbolTable* osyms = ReadOrCreateSymbolTable(FLAGS_osymbols);

  LexCompiler<StdArc> lc(istrm, isyms, osyms);

  if (!FLAGS_save_isymbols.empty())
    isyms->WriteText(FLAGS_save_isymbols);

  if (!FLAGS_save_osymbols.empty())
    osyms->WriteText(FLAGS_save_osymbols);

  if (argc == 3)
    fprintf(stderr, "%s\n", argv[3]);

  lc.GetFst().Write(argc < 3 ? "" : argv[2]);
  return 0;
}
