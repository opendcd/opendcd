// generic-transition-model.h
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
// Author: Paul R. Dixon
// \file
// Simple transition model for generic FSTs

#ifndef DCD_GENERIC_TRANSITION_MODEL_H__
#define DCD_GENERIC_TRANSITION_MODEL_H__

#include <algorithm>
#include <fstream>
#include <iostream>
#include <utility>

#include <dcd/lattice.h>
#include <dcd/token.h>
#include <dcd/utils.h>
#include <dcd/constants.h>

namespace dcd {

using fst::CountStates;
using fst::ArcIterator;
using fst::StateIterator;
using fst::StdFst;

template<class F>
class GenericTransitionModel {
 private:
  GenericTransitionModel()
      : acoustic_scale_(1.0f) { }

 public:
  typedef F FrontEnd;
  static GenericTransitionModel* ReadFsts(const std::string& path,
                                          float scale = 1.0f) {
    GenericTransitionModel *mdl = new GenericTransitionModel;
    if (ReadFstArcTypes(path, &mdl->fsts_, scale, false)) {
      vector<const StdFst*>& fsts_ = mdl->fsts_;
      for (int i = 0; i != fsts_.size(); ++i) {
        int numstates = CountStates(*fsts_[i]);
        if (numstates >= kMaxTokensPerArc) {
          LOG(FATAL) << "Arc type has too many states : " << numstates;
        }
        mdl->num_states_.push_back(numstates);
      }
      return mdl;
    }
    delete mdl;
    return 0;
  }

  ~GenericTransitionModel() {
    for (int i = 0; i != fsts_.size(); ++i)
      if (fsts_[i])
        delete fsts_[i];
  }

  // Just for debbugging in the inital state
  void DumpInfo(Logger& logger = dcd::logger) const { }

  // Test that an input label is valid
  bool IsValidILabel(int ilabel) const { return ilabel < fsts_.size(); }

  void SetInput(FrontEnd* frontend,
                const SearchOptions &opts) {
    acoustic_scale_ = opts.acoustic_scale;
    frontend_ = frontend;
    index_ = 0;
  }

  int Next() {
    return ++index_;
  }

  bool Done() { return frontend_->IsLastFrame(index_); }

  // Inform the decoder of an epsilon like label
  bool IsNonEmitting(int ilabel) const { return ilabel == 0; }

  // Additional cost to leave the transition model
  float GetExitWeight(int ilabel) const { return 0.0f; }

  int NumStates(int ilabel) const { return num_states_[ilabel] - 1; }

  int NumTypes() const { return fsts_.size(); }

  template<class Token>
  void GetActiveStates(int ilabel, const Token* tokens,
                       vector<pair<int, float> >* costs) {
  const StdFst& topo = *fsts_[ilabel];
  int numstates = num_states_[ilabel];
  for (int i = 0; i != numstates - 1; ++i) {
    if (tokens[i].Active()) {
      for (ArcIterator<StdFst> aiter(topo, i); !aiter.Done(); aiter.Next()) {
        const StdArc& arc = aiter.Value();
        if (arc.ilabel)
          costs->push_back(pair<int, float>(arc.ilabel, tokens[i].Cost()));
      }
    }
  }
  }


  // Remove this, it is too expensive to use in practise
  float AcousticLookahead(int ilabel, int time) {
    if (frontend_->IsLastFrame(time))
      return 0;
    float lookahead = 0;
    const StdFst& topo = *fsts_[ilabel];
    int numstates = num_states_[ilabel];
    for (int i = 0; i != numstates - 1; ++i) {
      for (ArcIterator<StdFst> aiter(topo, i); !aiter.Done(); aiter.Next()) {
        const StdArc& arc = aiter.Value();
        lookahead = max(lookahead, StateCost(time + 1, arc.ilabel));
      }
    }
    return lookahead;
  }

  // Expand the tokens in the arc or (sub network)
  template<class Options>
  pair<float, float> Expand(int ilabel, Options* opts) {
    typedef typename Options::Token Token;
    const StdFst& topo = *fsts_[ilabel];
    int numstates = num_states_[ilabel];
    float bestcost = kMaxCost;
    float nextbestcost = kMaxCost;
    Token *tokens = opts->tokens_;
    Token *nexttokens = opts->scratch_;
    for (int i = 0; i < numstates; ++i)
      nexttokens[i].Clear();

    for (int i = 0; i != numstates - 1; ++i) {
      if (tokens[i].Active()) {
        for (ArcIterator<StdFst> aiter(topo, i); !aiter.Done(); aiter.Next()) {
          const StdArc& arc = aiter.Value();
          float extend = arc.weight.Value() + StateCost(arc.ilabel);
          //float ls =  frontend_->IsLastFrame(index_) ? kMaxCost
          //  : tokens[i].Cost() +  extend + arc.weight.Value() +
          //  StateCost(index_ + 1, arc.ilabel);
          float cost = nexttokens[arc.nextstate].Combine(tokens[i], extend);
          float ncost = kMaxCost;  // cost + extend * opts.acoustic_lookahead;
          //float ncost = frontend_->IsLastFrame(index_) ? 0 :
          //StateCost(index_+ 1, arc.ilabel);
          if (cost > opts->threshold_ || ncost > opts->lthreshold_) {
            //Maybe this doesn't help efficiency very much
            nexttokens[arc.nextstate].Clear();
          } else {
            bestcost = min(bestcost, cost);
            nextbestcost = max(nextbestcost, ncost);
            //nextbestcost = min(nextbestcost, ls);
            //nextbestcost = min(nextbestcost,
            //cost + extend * opts.acoustic_lookahead);
          }
        }
      }
    }

    for (int i = 0; i != numstates; ++i)
      tokens[i] = nexttokens[i];
    return pair<float, float>(bestcost, nextbestcost);
  }

  static const string &Type() {
    static string type = "GenericTransitionModel";
    return type;
  }

 protected:
  inline float StateCost(int slabel) {
    return -frontend_->LogLikelihood(index_, slabel - 1) * acoustic_scale_;
  }

  inline float StateCost(int index, int slabel) {
    return -frontend_->LogLikelihood(index, slabel - 1) * acoustic_scale_;
  }

  FrontEnd *frontend_;
  int index_;
  float acoustic_scale_;
  vector<const StdFst*> fsts_;
  vector<int> num_states_;
};
}  // namespace dcd
#endif  // DCD_GENERIC_TRANSITION_MODEL_H__

