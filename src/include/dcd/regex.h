// regex.h
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
// Author: jorono@yandex-team.ru (Josef Novak)
/// \file
/// Basic regular-expression to FST compiler class.
/// First cut requires a postfix-style expression.

#ifndef DCD_REGEX_H__
#define DCD_REGEX_H__

#include <fst/fstlib.h>


namespace dcd {
namespace fst {
// A simple regular-expression to WFSA converter. 
// This is purely based on standard OpenFst algorithms, 
// which are applied to a postfix-style regular expression.
// The algorithm is somewhat Thompson-like, however due to 
// the use of standard OpenFst algorithms, it tends to 
// generate a large number of epsilon transitions.
// 
// TODO: Make this an OpenFst compliant lazy-fst.
template<class Arc>
class Regex {

 public:
  explicit Regex<Arc>(deque<string> expression, bool optimize = true,
		      SymbolTable* syms = 0, string eps = "<eps>")
      : expression_(expression),
        optimize_(optimize),
        syms_(syms == 0 ? new SymbolTable("syms") : syms),
        eps_(eps) { 
    if (syms_->NumSymbols() == 0)
      syms_->AddSymbol(eps_);
  }

  void RegexToPostfix(string regex) {
    // TODO: write the regex-to-postfix expression converter
  }

  // Actually convert the postfix expression to an FST.
  void RegexToFst() {
    while (expression_.size() > 0) {
      string tok_ = expression_.front();
      expression_.pop_front();
      // TODO: All but the default statement are amenable to 
      // a 'switch'.  Is there a *simple* way to leverage that?
      if (tok_ == ".")
	RegexConcat();
      else if (tok_ == "|")
	Split();
      else if (tok_ == "*")
	Closure(&fsts_[fsts_.size()-1], CLOSURE_STAR);
      else if (tok_ == "+")
	Closure(&fsts_[fsts_.size()-1], CLOSURE_PLUS);
      else if (tok_ == "?")
	ZeroOrOne();
      else
	TokToFst(tok_);
    }

    // Is there a more concise way to do this?
    if (optimize_) {
      RmEpsilon(&fsts_[0]);
      fsts_[0] = VectorFst<Arc>(DeterminizeFst<Arc>(fsts_[0]));
      Minimize<Arc>(&fsts_[0]);
    } 

    return;
  }

  void Write(string ofile) {
    fsts_[0].SetInputSymbols(syms_);
    fsts_[0].SetOutputSymbols(syms_);
    fsts_[0].Write(ofile);
    return;
  }

  ~Regex() {
    delete syms_;
  }

 private:
  deque<string> expression_;
  bool optimize_;
  SymbolTable* syms_;
  string eps_;
  vector<VectorFst<Arc> > fsts_; //fragment stack

  void RegexConcat( ){
    VectorFst<Arc> fst2 = fsts_.back();
    fsts_.pop_back();
    Concat(&fsts_[fsts_.size()-1], fst2);
    return;
  }

  void Split( ){
    VectorFst<Arc> fst2 = fsts_.back();
    fsts_.pop_back();
    Union(&fsts_[fsts_.size()-1], fst2);
    return;
  }

  void ZeroOrOne() {
    int i = fsts_.size()-1;
    int s = fsts_[i].AddState();
    fsts_[i].AddArc(
      fsts_[i].Start(), 
      Arc(0, 0, Arc::Weight::One(), s));
    fsts_[i].SetFinal(s, Arc::Weight::One());
    return;
  }

  void TokToFst(string tok_) {
    VectorFst<Arc> fst;
    fst.AddState();
    fst.SetStart(0);
    fst.AddArc(0, Arc(syms_->AddSymbol(tok_), 
		      syms_->AddSymbol(tok_), 
		      Arc::Weight::One(), 1));
    fst.SetFinal(fst.AddState(), Arc::Weight::One());
    fsts_.push_back(fst);
    return;
  }
};

} //namespace fst
} //namespace dcd

#endif // DCD_REGEX_H__
