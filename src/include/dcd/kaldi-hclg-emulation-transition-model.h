// hmm-transition-model.h
// LICENSE-GOES-HERE
//
// Copyright 2013- Yandex LLC
// Author: dixonp@yandex-team.ru (Paul Dixon)
//
// \file
// Simple transition model for generic WFSTs

#ifndef DCD_KALDI_GENERIC_TRANSITION_MODEL_H__
#define DCD_KALDI_GENERIC_TRANSITION_MODEL_H__

#include <fstream>
#include <iostream>

#include <dcd/foreach.h>
#include <dcd/lattice.h>
#include <dcd/token.h>
#include <dcd/utils.h>
#include <dcd/constants.h>

namespace dcd {

using fst::CountStates;
using fst::ArcIterator;
using fst::StateIterator;
using fst::StdFst;

template<class Decodable>
class KaldiGenericTransitionModel {
 private:
  KaldiGenericTransitionModel() 
    :  acoustic_scale_(1.0f) { }
 public:
  static KaldiGenericTransitionModel* ReadFsts(const std::string& path,
                                          float scale = 1.0f) {
    KaldiGenericTransitionModel *mdl = new GenericTransitionModel;
    if (ReadFstArcTypes(path, &mdl->fsts_, scale, false)) {
      vector<const StdFst*>& fsts_ = mdl->fsts_;
      for (int i = 0; i != fsts_.size(); ++i) 
        mdl->num_states_.push_back(CountStates(*fsts_[i]));
      return mdl;
    }
    delete mdl;
    return 0;
  }

  //Just for debbugging in the inital state
  void DumpInfo(Logger& logger = dcd::logger) const { }
  
  //Test that an input label is valid
  bool IsValidILabel(int ilabel) const { return ilabel < fsts_.size(); }

  //Set the input the 
  void SetFeatures(const vector<vector<float> > &features,
                   const SearchOptions &opts) {
    features_ = &features;
    index_ = 1;
    current_frame_ = &((*features_)[0][0]);
    acoustic_scale_ = opts.acoustic_scale;
  }

  int Next() {
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
  inline float StateCost(int slabel) {
    return -current_frame_[slabel - 1] * acoustic_scale_;
  }

  int index_;
  const vector<vector<float> > *features_;
  const float *current_frame_;
  float acoustic_scale_;
  vector<const StdFst*> fsts_;
  vector<int> num_states_;
};

} //namespace dcd

#endif //DCD_KALDI_GENERIC_TRANSITION_MODEL_H__

