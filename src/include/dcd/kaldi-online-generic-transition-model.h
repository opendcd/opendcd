// kaldi-online-generic-transition-model.h
// LICENSE-GOES-HERE
//
// Copyright 2013- Yandex LLC
// Authors: dixonp@yandex-team.ru (Paul Dixon),
//          jorono@yandex-team.ru (Josef Novak)
//
// \file
// Simple transition model for generic WFSTs
// This version accepts a Kaldi-style custom
// online decodable compatible with nnet models.

#ifndef DCD_KALDI_ONLINE_GENERIC_TRANSITION_MODEL_H__
#define DCD_KALDI_ONLINE_GENERIC_TRANSITION_MODEL_H__

#include <fstream>
#include <iostream>

#include <dcd/foreach.h>
#include <dcd/lattice.h>
#include <dcd/token.h>
#include <dcd/utils.h>
#include <dcd/constants.h>
#include "dcd/frontend/online-nnet-transf-decodable.h"

namespace dcd {

using fst::CountStates;
using fst::ArcIterator;
using fst::StateIterator;
using fst::StdFst;

class KaldiOnlineGenericTransitionModel {
  typedef TokenTpl<Lattice::LatticeState*> Token;
 public:
 KaldiOnlineGenericTransitionModel()
   : decodable_(NULL) { }
  void SetDecodable(kaldi::DecodableInterface* decodable) {decodable_ = decodable;}

  static KaldiOnlineGenericTransitionModel* ReadFsts(const std::string& path,
						float scale = 1.0f) {
    KaldiOnlineGenericTransitionModel *mdl = new KaldiOnlineGenericTransitionModel;
    if (ReadFstArcTypes(path, &mdl->fsts_, scale, false)) {
      vector<const StdFst*>& fsts_ = mdl->fsts_;
      for (int i = 0; i != fsts_.size(); ++i) 
        mdl->num_states_.push_back(CountStates(*fsts_[i]));
      return mdl;
    }

    delete mdl;
    return 0;
  }

  ~KaldiOnlineGenericTransitionModel() {
    for (int i = 0; i != fsts_.size(); ++i)
      if (fsts_[i]) 
        delete fsts_[i];
  }

  //Just for debbugging in the inital state
  void DumpInfo(Logger& logger = dcd::logger) const { }
  
  //Test that an input label is valid
  bool IsValidILabel(int ilabel) const { return ilabel < fsts_.size(); }

  //Set the input features.  Just a placebo for the online decodable
  void SetFeatures() {
    index_ = 1;
  }

  int Next() {
    //This just increments the index until
    // Done() returns TRUE
    if (!Done())
      index_++;
    return index_;
  }

  bool Done() {
    //In the case of the online decodable this does almost all the
    // work.  IsLastFrame calls IsValidFrame which in turn requests
    // more frames as needed from the socket/source.  The result is
    // then used to compute the loglikelihoods need by the decoder.
    //When there are no more frames, it returns TRUE, and then the
    // next time that the Next() function/iterator is called, the
    // decoder knows it is time to stop.
    return decodable_->IsLastFrame(index_);
  }

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
    // decoder during the search.  Frame requests and checking are
    // requested by Done() then propagated up.
    return -decodable_->LogLikelihood(index_-1, slabel-1);
  }

  int index_;
  const vector<vector<float> > *features_;
  vector<const StdFst*> fsts_;
  vector<int> num_states_;
};

} //namespace dcd

#endif // DCD_KALDI_ONLINE_GENERIC_TRANSITION_MODEL_H__
