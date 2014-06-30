// hmm-transition-model.h
// LICENSE-GOES-HERE
//
// Copyright 2013- Yandex LLC
// Authors: dixonp@yandex-team.ru (Paul Dixon),
//          jorono@yandex-team.ru (Josef Novak)
//
// \file
// Simple transition model for generic WFSTs
// This version accepts a Kaldi-style custom
// decodable.  It should be agnostic to the am
// and decodable type.  It is NOT online compatible.

#ifndef CDC_KALDI_GENERIC_TRANSITION_MODEL_H__
#define DCD_KALDI_GENERIC_TRANSITION_MODEL_H__

#include <fstream>
#include <iostream>

#include <dcd/foreach.h>
#include <dcd/lattice.h>
#include <dcd/token.h>
#include <dcd/utils.h>
#include <dcd/constants.h>
#include "itf/decodable-itf.h"

namespace dcd {

using fst::CountStates;
using fst::ArcIterator;
using fst::StateIterator;
using fst::StdFst;

class KaldiGenericTransitionModel {
 private:
  KaldiGenericTransitionModel(kaldi::DecodableInterface* decodable) 
      :  decodable_(decodable) { }

  static KaldiGenericTransitionModel* ReadFsts(const std::string& path,
						kaldi::DecodableInterface* decodable,
						float scale = 1.0f) {
    KaldiGenericTransitionModel *mdl = 
      new KaldiGenericTransitionModel(decodable);

    if (ReadFstArcTypes(path, &mdl->fsts_, scale, false)) {
      vector<const StdFst*>& fsts_ = mdl->fsts_;
      for (int i = 0; i != fsts_.size(); ++i) 
        mdl->num_states_.push_back(CountStates(*fsts_[i]));
      return mdl;
    }

    delete mdl;
    return 0;
  }

  ~KaldiGenericTransitionModel() {
    for (int i = 0; i != fsts_.size(); ++i)
      if (fsts_[i]) 
        delete fsts_[i];
  }

  //Just for debbugging in the inital state
  void DumpInfo(Logger& logger = dcd::logger) const { }
  
  //Test that an input label is valid
  bool IsValidILabel(int ilabel) const { return ilabel < fsts_.size(); }

  //Set the input features.  Just a placebo for the online decodable
  void SetFeatures(const vector<vector<float> > &features,
                   const SearchOptions &opts) {
    features_ = &features;
    index_ = 1;
    current_frame_ = &((*features_)[0][0]);
    acoustic_scale_ = opts.acoustic_scale;
  }

  int Next() {
    //This just increments the index until
    // Done() returns TRUE
    if (!Done())
      current_frame_ = &((*features_)[index_++][0]);
    return index_;
  }

  bool Done() { return features_->size() == index_; }

  //Inform the decoder of an epsilon like label
  bool IsNonEmitting(int ilabel) const { return ilabel == 0; }
 
  //Additional cost to leave the transition model
  float GetExitWeight(int ilabel) const { return 0.0f; }

  int NumStates(int ilabel) const { return num_states_[ilabel] - 1; }


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

  //Expand the tokens in the arc or (sub network)
  float Expand(int ilabel, Token* tokens, float threshold,
               const SearchOptions& opts, Lattice* lattice = 0) {
    const StdFst& topo = *fsts_[ilabel];
    int numstates = num_states_[ilabel];
    float bestcost = kMaxCost;
    vector<Token> nexttokens(numstates);
    //Assume the last state does not have any outgoing arcs
    for (int i = 0; i != numstates - 1; ++i) {
      if (tokens[i].Active()) {
        for (ArcIterator<StdFst> aiter(topo, i); !aiter.Done(); aiter.Next()) {
          const StdArc& arc = aiter.Value();
          float cost = nexttokens[arc.nextstate].Combine(tokens[i], 
                        + arc.weight.Value() + StateCost(arc.ilabel));
          if (cost > threshold)
            nexttokens[arc.nextstate].Clear();
          else
            bestcost = min(bestcost, cost);
        }
      }
    }

    for (int i = 0; i != numstates; ++i)
      tokens[i] = nexttokens[i];
    return bestcost;
 }

 protected:
  kaldi::DecodableInterface* decodable_;

  inline float StateCost(int slabel) {
    //Loglikelihoods for frames are returned here, and used by the 
    // decoder during the search. This one has to be negated...
    return -decodable_->LogLikelihood(index_-1, slabel-1);
  }

  int index_;
  const vector<vector<float> > *features_;
  const float *current_frame_;
  float acoustic_scale_;
  vector<const StdFst*> fsts_;
  vector<int> num_states_;
};

} //namespace dcd

#endif // DCD_KALDI_GENERIC_TRANSITION_MODEL_H__
