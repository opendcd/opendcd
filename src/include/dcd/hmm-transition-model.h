// hmm-transition-model.h
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
// Simple and slighlty optimized transition model for
// left-to-right and ergodic hmms

#ifndef DCD_HMM_TRANSITION_MODEL_H__
#define DCD_HMM_TRANSITION_MODEL_H__

#include <fstream>
#include <iostream>
#include <fst/extensions/far/far.h>

#include <dcd/lattice.h>
#include <dcd/token.h>
#include <dcd/utils.h>
#include <dcd/constants.h>

using fst::FarReader;
using fst::Fst;
using fst::StateIterator;

using std::ifstream;
using std::istream;
using std::ofstream;
using std::ostream;
using std::ostream;
using std::string;

namespace dcd {

const int kEpsilon = 0;
const int kDisambiguation = 1;
const int kLeftToRight = 2;
const int kErgodic = 3;

/*
   template<class Arc>
int CountArcs(const Fst<Arc>& fst) {
typedef Fst<Arc> FST;
int numarcs = 0;
for (StateIterator<FST> siter(fst); !siter.Done(); siter.Next())
numarcs += fst.NumArcs(siter.Value());
return numarcs;
}
*/

struct HMMArc {
  int nexstate;
  float weight;
};

struct HMMState {
  int state;
  int numarcs;
  HMMArc arcs[0];
};

struct LRHMMState {
  int state;
  float next;
  float loop;
};

struct LRHMMM {
  LRHMMState states[4];
};

// typedef TokenTpl<Lattice::LatticeState*> Token;
// This class encapsulates the left-to-right
// or ergodic transition model used in Kaldi's silence
//
template<class Decodable>
class HMMTransitionModel {
  typedef pair<float, float> FloatPair;
  HMMTransitionModel()
      : num_hmms_(0), total_num_states_(0), acoustic_scale_(0.0f),
      hmm_syms_("hmmsyms") { }

 public:
  typedef Decodable FrontEnd;
  virtual ~HMMTransitionModel() {
    for (int i = 0; i != fsts_.size(); ++i)
      delete fsts_[i];
  }

  static HMMTransitionModel* Read(const string& path) {
    ifstream ifs(path.c_str(), ifstream::binary);
    if (!ifs.is_open())
      return 0;
    return Read(&ifs);
  }

  static HMMTransitionModel* Read(istream* strm) {
    HMMTransitionModel* mdl = new HMMTransitionModel;
    ReadType(*strm, &mdl->num_hmms_);
    ReadType(*strm, &mdl->total_num_states_);
    ReadType(*strm, &mdl->num_eps_);
    ReadType(*strm, &mdl->num_bakis_);
    ReadType(*strm, &mdl->num_ergodic_);
    ReadType(*strm, &mdl->state_labels_);
    ReadType(*strm, &mdl->state_offsets_);
    ReadType(*strm, &mdl->weights_);
    ReadType(*strm, &mdl->weight_offsets_);
    ReadType(*strm, &mdl->next_states_);
    ReadType(*strm, &mdl->types_);
    return mdl;
  }

  bool Write(ostream* strm) {
    //TODO add a header
    WriteType(*strm, num_hmms_);
    WriteType(*strm, total_num_states_);
    WriteType(*strm, num_eps_);
    WriteType(*strm, num_bakis_);
    WriteType(*strm, num_ergodic_);
    WriteType(*strm, state_labels_);
    WriteType(*strm, state_offsets_);
    WriteType(*strm, weights_);
    WriteType(*strm, weight_offsets_);
    WriteType(*strm, next_states_);
    WriteType(*strm, types_);
    return true;
  }

  bool Write(const string& path) {
    ofstream ofs(path.c_str(), ofstream::binary);
    if (!ofs.is_open())
      return false;
    return Write(&ofs);
  }

  //Returns the final weight to leave the arc
  //or some other exit penalty
  float GetExitWeight(int ilabel) const {
    if (types_[ilabel] == kLeftToRight) {
      int woffset = weight_offsets_[ilabel];
      const float* weights = &weights_[woffset];
      return weights[6];
    }
    return 0.0f;
  }

  bool WriteText(const string& filename) {
    ofstream ofs(filename.c_str());
    return WriteText(ofs);
  }

  bool WriteEpsType(ostream& strm, int type) {
    int woffset = weight_offsets_[type];
    int soffset = state_offsets_[type];
    float* weights = &weights_[woffset];
    int* labels = &state_labels_[soffset];
    strm << "HMM " << type << " 0 1" << endl;
    strm << "0 1 " << labels[0] << " " << weights[0] << endl;
    return false;
  }

  bool WriteLeftToRightType(ostream& strm, int type) {
    int woffset = weight_offsets_[type];
    int soffset = state_offsets_[type];
    float* weights = &weights_[woffset + 1]; //Add one extra for the sentinel
    int* labels = &state_labels_[soffset + 1];
    strm << "HMM " << type << " " << 3 << endl;
    for (int j = 0; j != 3; ++j) {
      strm << j << " " << j << " " << *labels << " " << *weights++
          << endl;
      strm << j << " " << j + 1 << " " << *labels++ << " "
          << *weights++ << endl;
    }
    return false;
  }

  bool WriteErgodicType(ostream& strm, int type) {
    int woffset = weight_offsets_[type];
    int soffset = state_offsets_[type];
    float* weights = &weights_[woffset + 1]; //Add one extra for the sentinel
    int* labels = &state_labels_[soffset + 1];
    int* next_states = &next_states_[woffset + 1];
    strm << "HMM " << type << endl;
    int j = 0;
    for (j = 0 ; j != 4; ++j)
      for (int k = 0; k != 4; ++k)
        strm << j << " " << *next_states++ << " " << *labels << " "
            <<  *weights++ << endl;

    strm << j << " " << j << " " << *labels << " " << *weights++ << endl;
    strm << j << " " << j << " " << *labels << " " << *weights++ << endl;
    return false;
  }

  bool WriteTypeText(ostream& strm, int type) {
    switch (types_[type]) {
      case kEpsilon: WriteEpsType(strm, type); break;
      case kLeftToRight: WriteLeftToRightType(strm, type); break;
      case kErgodic: WriteErgodicType(strm, type); break;
      default: return false;
    }
    return false;
  }

  bool WriteText(ostream& strm) {
    for (int i = 0; i != num_hmms_; ++i) {
      switch (types_[i]) {
        case kEpsilon: WriteEpsType(strm, i); break;
        case kLeftToRight: WriteLeftToRightType(strm, i); break;
        case kErgodic: WriteErgodicType(strm, i); break;
        default: LOG(FATAL) << "Unknown HMM types : " << types_[i]; break;
      }
    }
    return true;
  }

  template<class Arc>
static HMMTransitionModel* ReadFar(const string& path) {
  FarReader<Arc>* reader = FarReader<Arc>::Open(path);
  HMMTransitionModel * model = 0;
  if (reader) {
    model = ReadFar(reader);
    delete reader;
  }
  return model;
}

static HMMTransitionModel* ReadFsts(const string& path, float scale) {
  typedef StdArc Arc;
  vector<const Fst<Arc>*> arcs;
  if (!ReadFstArcTypes(path, &arcs, scale, false)) {
    FSTERROR() << "Failed to read FSTs format arc types from : " << path;
    return 0;
  }
  HMMTransitionModel* mdl = FromFsts(arcs);
  return mdl;
}

template<class Arc>
static HMMTransitionModel* FromFsts(const vector<const Fst<Arc>*>& fsts) {
  int numstates = 0;
  int numarcs = 0;
  int numeps = 0;
  int numbakis = 0;
  int numergodic = 0;
  int numhmms = 0;
  HMMTransitionModel* mdl = new HMMTransitionModel;
  //We assume bakis have three states and ergodic have five states
  for (int i = 0; i != fsts.size(); ++i) {
    const Fst<Arc>& fst = *(fsts[i]);
    int nstates = CountStates(fst);
    mdl->num_states_.push_back(nstates);
    numstates += nstates;
    numarcs += CountArcs(fst);
    mdl->weight_offsets_.push_back(mdl->weights_.size());
    mdl->state_offsets_.push_back(mdl->state_labels_.size());
    switch (nstates) {
      case 2: ++numeps;
              mdl->types_.push_back(kEpsilon);
              break;
      case 4: ++numbakis;
              mdl->types_.push_back(kLeftToRight);
              mdl->weights_.push_back(0.0f); //Sentinels for token expansion
              mdl->state_labels_.push_back(-1);
              mdl->next_states_.push_back(0);
              break;
      case 6: ++numergodic;
              mdl->types_.push_back(kErgodic);
              mdl->weights_.push_back(0.0f); //Sentinels for token expansion
              mdl->state_labels_.push_back(-1);
              mdl->next_states_.push_back(0);
              break;
      default: break;
    }

    for (int i = 0; i != nstates; ++i) {
      for (ArcIterator<Fst<Arc> > aiter(fst, i); !aiter.Done();
           aiter.Next()) {
        const Arc& arc = aiter.Value();
        mdl->weights_.push_back(arc.weight.Value());
        mdl->next_states_.push_back(arc.nextstate);
        if (aiter.Position() == 0) {
          mdl->state_labels_.push_back(arc.ilabel);
          if (nstates == 2)
            mdl->eps_labels_set_.insert(arc.ilabel);
          else
            mdl->state_labels_set_.insert(arc.ilabel);
        }
      }
    }
  }

  mdl->num_hmms_ = numhmms;
  mdl->total_num_states_ = numstates;
  mdl->num_eps_ = numeps;
  mdl->num_bakis_ = numbakis;
  mdl->num_ergodic_ = numergodic;
  mdl->fsts_ = fsts;

  //Sanity check to make sure the arrays were filled correctly
  int num_states = mdl->num_eps_ + mdl->num_bakis_ * 4 +
      mdl->num_ergodic_ * 6;
  assert(num_states == mdl->state_labels_.size());

  int num_trans = mdl->num_eps_ + mdl->num_bakis_ * 7 +
      mdl->num_ergodic_ * 18;

  return mdl;

}

template<class Arc>
static HMMTransitionModel* ReadFar(FarReader<Arc>* reader) {
  int numstates = 0;
  int numarcs = 0;
  int numeps = 0;
  int numbakis = 0;
  int numergodic = 0;
  int numhmms = 0;
  HMMTransitionModel* mdl = new HMMTransitionModel;
  //We assume bakis have three states and ergodic have five states
  for (; !reader->Done(); reader->Next(), ++numhmms) {
    const string& str = reader->GetKey();
    mdl->hmm_syms_.AddSymbol(str);
    const Fst<Arc>& fst = reader->GetFst();
    int nstates = CountStates(fst);
    numstates += nstates;
    numarcs += CountArcs(fst);
    mdl->weight_offsets_.push_back(mdl->weights_.size());
    mdl->state_offsets_.push_back(mdl->state_labels_.size());
    switch (nstates) {
      case 2: ++numeps;
              mdl->types_.push_back(kEpsilon);
              break;
      case 4: ++numbakis;
              mdl->types_.push_back(kLeftToRight);
              mdl->weights_.push_back(0.0f); //Sentinels for token expansion
              mdl->state_labels_.push_back(-1);
              mdl->next_states_.push_back(0);
              break;
      case 6: ++numergodic;
              mdl->types_.push_back(kErgodic);
              mdl->weights_.push_back(0.0f); //Sentinels for token expansion
              mdl->state_labels_.push_back(-1);
              mdl->next_states_.push_back(0);
              break;
      default: break;
    }

    for (int i = 0; i != nstates; ++i) {
      for (ArcIterator<Fst<Arc> > aiter(fst, i); !aiter.Done();
           aiter.Next()) {
        const Arc& arc = aiter.Value();
        mdl->weights_.push_back(arc.weight.Value());
        mdl->next_states_.push_back(arc.nextstate);
        if (aiter.Position() == 0) {
          mdl->state_labels_.push_back(arc.ilabel);
          if (nstates == 2)
            mdl->eps_labels_set_.insert(arc.ilabel);
          else
            mdl->state_labels_set_.insert(arc.ilabel);
        }
      }
    }
  }

  mdl->num_hmms_ = numhmms;
  mdl->total_num_states_ = numstates;
  mdl->num_eps_ = numeps;
  mdl->num_bakis_ = numbakis;
  mdl->num_ergodic_ = numergodic;

  //Sanity check to make sure the arrays were filled correctly
  int num_states = mdl->num_eps_ + mdl->num_bakis_ * 4 +
      mdl->num_ergodic_ * 6;
  assert(num_states == mdl->state_labels_.size());

  int num_trans = mdl->num_eps_ + mdl->num_bakis_ * 7 +
      mdl->num_ergodic_ * 18;

  LOG(INFO) << num_states << " " << mdl->state_labels_.size() << " "
      << numstates;

  LOG(INFO) << num_trans << " " << mdl->weights_.size() << " "
      << numarcs;

  return mdl;
}
//Ergodic HMMs are more tricky, in this version just do
//the simpliest way
template<class Token>
inline FloatPair ExpandErgodic(int label, Token* tokens, float* weights, int* states) {
  Token token_scratch_[kMaxTokensPerArc];
  ClearTokens(token_scratch_, kMaxTokensPerArc);
  float best_cost = kMaxCost;

  best_cost = min(token_scratch_[1].Combine(tokens[0]), best_cost);

  //First transition outgoing arcs
  for (int i = 1; i != 5; ++i)
    best_cost = min(token_scratch_[i].Combine(tokens[1], *weights++), best_cost);

  //Clearly re-arraning this will allow parallel expansion
  for (int n = 2; n != 5; ++n)
    for (int i = 2; i != 6; i++)
      best_cost = min(token_scratch_[i].Combine(tokens[n], *weights++), best_cost);

  //Last transition
  best_cost = min(token_scratch_[5].Combine(tokens[5], *weights++), best_cost);

  //Delete the first token so we don't re-expand on the next frame
  tokens[0].Clear();
  for (int i = 1; i != 6; ++i)
    tokens[i] = token_scratch_[i];

  return FloatPair(best_cost, kMaxCost);
}

template<class Token>
void ClearTokens(Token* tokens, int num) {
  for (int i = 0; i != num; ++i)
    tokens[i].Clear();
}

//For left-to-right HMMs just work backwards down the array
//The prefetch work in reverse
template<class Token>
inline FloatPair ExpandLeftToRight(int label, Token* tokens, float* weights,
                                   int* states) {
  PROFILE_FUNC();
  float best_cost = kMaxCost;
  for (int i = 3; i > 0; --i) {
    if (tokens[i].Active() || tokens[i - 1].Active()) {
      //HMM weights are just a straight array
      //state n loop | state n - 1 next
      //  5          |  4
      //  3          |  2
      //  1          |  0
      int state = states[i];
      float am_cost = Score(state);
      float loop = tokens[i].Cost() + weights[2 * i - 1] + am_cost;
      float prev = tokens[i - 1].Cost() + weights[2 * i - 2] + am_cost;
      if (loop < prev) {
        tokens[i].SetCost(loop);
      } else {
        tokens[i].SetValue(tokens[i - 1].LatticeState(), prev);
      }
      best_cost = min(best_cost, tokens[i].Cost());
    }
  }
  //Clear the first token so it doesn't get used again during
  //the next expansion
  tokens[0].Clear();
  return FloatPair(best_cost, kMaxCost);
}

//Expand the tokens in the arc or (sub network)
template<class Options>
inline FloatPair ExpandGeneric(int ilabel, Options *opts) {
  PROFILE_FUNC();
  const StdFst& topo = *fsts_[ilabel];
  int numstates = num_states_[ilabel];
  float bestcost = kMaxCost;
  typedef typename Options::Token Token;
  Token *tokens = opts->tokens_;
  Token *nexttokens = opts->scratch_;
  float threshold = opts->threshold_;

  for (int i = 0; i != numstates; ++i)
    nexttokens[i].Clear();

  for (int i = 0; i != numstates - 1; ++i) {
    if (tokens[i].Active()) {
      for (ArcIterator<StdFst> aiter(topo, i); !aiter.Done(); aiter.Next()) {
        const StdArc& arc = aiter.Value();
        float cost = nexttokens[arc.nextstate].Combine(tokens[i],
                                                       + arc.weight.Value() + Score(arc.ilabel));
        if (cost > threshold) {
          //Maybe this doesn't help efficiency very much
          nexttokens[arc.nextstate].Clear();
        } else {
          bestcost = min(bestcost, cost);
        }
      }
    }
  }

  for (int i = 0; i != numstates; ++i)
    tokens[i] = nexttokens[i];

  return FloatPair(bestcost, kMaxCost);
}
//Expand the transition model corresponding
//to the ilabel and return the cost from the
//best scoring token
//Pass in an optional lattice pointer for state level lattice generation
template<class Options>
FloatPair Expand(int ilabel, Options* opts) {
  acoustic_scale_ = opts->opts_.acoustic_scale;
  int woffset = weight_offsets_[ilabel];
  float* weights = &weights_[woffset];
  int* states = &state_labels_[state_offsets_[ilabel]];
  float best_cost = kMaxCost;
  switch(types_[ilabel]) {
    case kEpsilon:
    case kDisambiguation: //Should never happen
      break;
    case kLeftToRight:
      return ExpandLeftToRight(ilabel, opts->tokens_, weights, states);
      break;
    case kErgodic:
      return ExpandGeneric(ilabel, opts);
      //return ExpandErgodic(ilabel, tokens, weights, states);
      break;
  }
  return FloatPair(best_cost, kMaxCost);
}

//Returns the score with slabel
//Scores index are zero based
inline float Score(int slabel) {
  return -decodable_->LogLikelihood(index_, slabel - 1) * acoustic_scale_;
}

//Set the input the
void SetInput(Decodable *decodable,
              const SearchOptions &opts) {
  index_ = 0;
  acoustic_scale_ = opts.acoustic_scale;
  decodable_ = decodable;
}

int Next() {
  return ++index_;
}

bool Done() { return decodable_->IsLastFrame(index_) ; }

int NumHmms() const { return num_hmms_; }

int Type(int ilabel) const { return types_[ilabel]; }

//Returns true for epsilon or disambiguation symbols
bool IsNonEmitting(int ilabel) const {
  return types_[ilabel] == kEpsilon || types_[ilabel] == kDisambiguation;
}

bool IsValidILabel(int ilabel) const {
  return true;
}

void DumpInfo(Logger& logger = dcd::logger) const {
  logger(INFO) << "Transition model info:" << endl
      << "\t\t  Num HMMs " << num_hmms_ << endl
      << "\t\t  Num of epsilons " << num_eps_ << endl
      << "\t\t  Num of Bakis " << num_bakis_ << ", Num of ergodic "
      << num_ergodic_ << endl
      << "\t\t  Total # of HMM states " << total_num_states_ << endl
      << "\t\t  Total # of HMM arcs " << weights_.size() << endl
      << "\t\t  # of HMM state labels " << state_labels_set_.size() << endl
      << "\t\t  # of epsilon labels " << eps_labels_set_.size() << endl;
}

int NumStates(int ilabel) const {
  if (ilabel >= num_states_.size())
    LOG(FATAL) << "Out of bounds arc look-up - relabelling or aux symbols? " << ilabel << " " <<  num_states_.size();
  return num_states_[ilabel] - 1;
  switch (types_[ilabel]) {
    case kEpsilon: return 0;
    case kLeftToRight: return 3;
    case kErgodic: return 5;
  }
  return -1;
}

static const string& Type() {
  static string type = "HMMTransitionModel";
  return type;
}

protected:
//Temporary tokens when expanding ergodic hmms
Decodable *decodable_;
int index_; //Current frame number

struct TransitionModelHeader {
  int num_hmms_;
  int num_states_;
  int num_arcs_;
  bool Read() {
    return true;
  }
  bool Write() {
    return true;
  }
};

//HMM and state information
int num_hmms_;
int num_eps_;
int num_bakis_;
int num_ergodic_;
int total_num_states_;
float acoustic_scale_;

set<int> state_labels_set_;
set<int> eps_labels_set_;

//HMM information
vector<int> types_;

vector<int> weight_offsets_;
vector<int> state_offsets_;

vector<float> weights_; //Arc transition probs
vector<int> next_states_; //Destination states
vector<int> state_labels_; //HMM states labels (normally would a label per
//arc but we use a Moore machine with the
//labels on the states because the transitions
//leaving a state all have the same labels

SymbolTable hmm_syms_;
vector<int> num_states_;
vector<const Fst<StdArc>*> fsts_;
private:
DISALLOW_COPY_AND_ASSIGN(HMMTransitionModel);
};
} //namepace dcd

#endif
