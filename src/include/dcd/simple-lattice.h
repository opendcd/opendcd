// simple-lattice.h
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
// Author : Paul R. Dixon
// \file
// Simple lattice that does not implement full lattice recognition and does not
// use memory pools. Serves as an example to show how create a lattice
// implementation for the decoder
//
#ifndef DCD_SIMPLE_LATTICE_H__
#define DCD_SIMPLE_LATTICE_H__

#include <string>
#include <utility>
#include <vector>

#include <fst/vector-fst.h>

#include <dcd/constants.h>
#include <dcd/kaldi-lattice-arc.h>
#include <dcd/log.h>
#include <dcd/search-opts.h>

using fst::VectorFst;
using fst::MutableFst;

namespace dcd {

class SimpleLattice {
 public:
  struct State;
  typedef State* LatticeState;
  struct LatticeArc {
    LatticeArc() {
      Clear();
    }

    explicit LatticeArc(const LatticeArc& other)
        : prevstate_(other.prevstate_), ilabel_(other.ilabel_),
          olabel_(other.olabel_), dur_(other.dur_),
          am_weight_(other.am_weight_), lm_weight_(other.lm_weight_),
          dur_weight_(other.dur_weight_) { }

    LatticeArc(State* prevstate, int ilabel, int olabel, float am_weight,
               float lm_weight, float dur_weight)
        : prevstate_(prevstate), ilabel_(ilabel), olabel_(olabel),
          dur_(-1), am_weight_(am_weight), lm_weight_(lm_weight),
          dur_weight_(dur_weight) { }

    State* prevstate_;
    int ilabel_;
    int olabel_;
    int dur_;
    float am_weight_;
    float lm_weight_;
    float dur_weight_;

    void Clear() {
      prevstate_ = 0;
      ilabel_ = kNoLabel;
      olabel_ = kNoLabel;
      dur_ = kNoLabel;
      am_weight_ = kMaxCost;
      lm_weight_ = kMaxCost;
      dur_weight_ = kMaxCost;
    }

    LatticeArc& operator= (const LatticeArc& other) {
      prevstate_ = other.prevstate_;
      ilabel_ = other.ilabel_;
      olabel_ = other.olabel_;
      dur_ = other.dur_;
      am_weight_ = other.am_weight_;
      lm_weight_ = other.lm_weight_;
      dur_weight_ = other.dur_weight_;
      return *this;
    }

    State* PrevState() const { return prevstate_; }

    int ILabel() const { return ilabel_; }

    int OLabel() const { return olabel_; }
  };


  struct State {
    void Init(int time, int state, int id, int index,
              float forwards_cost = kMaxCost) {
      Clear();
      time_ = time;
      state_ = state;
      id_ = id;
      index_ = index;
      forwards_cost_ = forwards_cost;
    }

    //  Create an OpenFst version of the best sequence from this state
    template<class Arc>
    int GetBestSequence(MutableFst<Arc>* ofst) const {
      // PROFILE_FUNC();
      assert(index_ != -1);
      int d = -1;
      if (PrevState()) {
        int s = PrevState()->GetBestSequence(ofst);
        d = ofst->AddState();
        ofst->AddArc(s, Arc(best_arc_.ilabel_, best_arc_.olabel_,
                            Arc::Weight::One(), d));
      } else {
        d = ofst->AddState();
        ofst->SetStart(d);
      }
      return d;
    }

    State* PrevState() const {
      return best_arc_.prevstate_;
    }

    // Add a new arc to lattice state
    template<class T>
    pair<float, float> AddArc(State* src, float cost,  const T& arc,
                              float threshold, const SearchOptions& opts) {
      float arc_cost = cost - src->ForwardsCost();
      float am_cost = arc_cost - arc.Weight();
      LatticeArc lattice_arc(src, arc.ILabel(), arc.OLabel(), am_cost,
                             arc.Weight(), 0.0f);
      if (cost < forwards_cost_) {
        // New best token arriving
        best_arc_ = lattice_arc;
        forwards_cost_ = cost;
      }
      return pair<float, float>(cost, forwards_cost_);
    }

    // Here we assume the start state has a null best_previous_ pointer
    bool IsStart() const { return !best_arc_.prevstate_; }

    void GcClearMark() { }

    bool GcMarked() const { return true; }

    void GcMark() { }

    void Clear() {
      state_ = kNoStateId;
      time_ = -1;
      id_ = -1;
      marked_ = false;
      index_ = -1;
      forwards_cost_ = kMaxCost;
      backwards_cost_ = kMaxCost;
      best_arc_.Clear();
    }

    void Check() { }

    int Index() const { return index_; }

    void SetIndex(int index) { index_ = index; }

    float ForwardsCost() const { return forwards_cost_; }

    float BackwardsCost() const { return backwards_cost_; }

    int Id() const { return id_; }

   protected:
      // This is internal and ls should not be modified  by this function
    template<class Arc>
    void GetLattice(State* ls, MutableFst<Arc>* lattice) const {
    }

    // ls is internal and should not be written by this function
    template<class Arc>
    void GetBestSequence(State* ls, MutableFst<Arc>* best) {
    }


    int state_;  // The state in the search fst
    int time_;  // Time the lattice state was created
    int id_;  // Unique id assigned to this state
    int marked_;  // Integer useful for storing other values, e.g. refs count
    // GC marks, id for value into VectorFsts when generating
    int index_;  // Where the lattice state is in the vector of lattice states;
    float forwards_cost_;
    float backwards_cost_;

    LatticeArc best_arc_; // The best lattice arc arriving in this state
    friend class SimpleLattice;
  };

 public:
  explicit SimpleLattice(const SearchOptions& opts, ostream* logstream =
                         &std::cerr)
      : next_id_(0), num_allocs_(0), num_frees_(0) { }

  virtual ~SimpleLattice() {
    Clear();
  }

  void Check() { }

  State* CreateStartState(int state) {
    State* ls =  NewState(-1, state);
    ls->forwards_cost_ = 0.0f;
    return ls;
  }

  void DumpInfo() { }

  State* NewState(int time, int state) {
    State* lattice_state = new State;
    lattice_state->Init(time, state, next_id_++, used_list_.size());
    ++num_allocs_;
    used_list_.push_back(lattice_state);
    return lattice_state;
  }

  void FreeState(State* lattice_state) {
    ++num_frees_;
  }

  State* AddState(int time, int state) {
    return NewState(time, state);
  }

  void DeleteState(State* ls) {
    delete ls;
  }

  // First field is the best cost arriving in the lattice state (forward cost)
  // Second field is the the cost of the SearchArc arrvining in the lattice
  // state
  template<class SearchArc>
  pair<float, float> AddArc(State* src, State* dest, float cost,
                            const SearchArc& arc, float threshold,
                            const SearchOptions &opts) {
    // Add the lattice arc in the reverse direction
    return dest->AddArc(src, cost, arc, threshold, opts);
  }

  int NumStates() const { return num_allocs_ - num_frees_; }

  void Clear() {
    for (int i = 0; i != used_list_.size(); ++i)
      delete used_list_[i];
    used_list_.clear();
  }

  void GcClearMarks() { }

  int GcSweep(vector<int>* early_mission = 0) { return 0; }

  void GetUsageStatistics(UtteranceSearchStatstics *search_statistics) { }

  template<class Arc>
  int GetLattice(State* state, MutableFst<Arc>* ofst) {
    return 0;
  }

  // Debugging and check functions
  void SortLists() {
  }

  bool IsActive(State* state) const {
    return true;
  }

  bool IsFree(State* state) const {
    return true;
  }

  int FreeListSize() const { return 0; }

  // Debug function to build a tranducers from the partial traceback
  template<class Arc>
  void DumpToFst(VectorFst<Arc>* ofst, fst::SymbolTable* ssyms = 0) {
    typedef typename Arc::Weight W;
  }

  static string Type() {
    return "SimpleLattice";
  }

 protected:
  vector<State*> used_list_;
  int next_id_;
  int num_allocs_;
  int num_frees_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleLattice);
};
}  // namespace dcd

#endif  // DCD_SIMPLE_LATTICE_H__
