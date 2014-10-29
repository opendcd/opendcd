// ngram-count.h
//
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
// Copyright 2009-2013 Brian Roark and Google, Inc.
// Authors: roarkbr@gmail.com  (Brian Roark)
//          allauzen@google.com (Cyril Allauzen)
//          riley@google.com (Michael Riley)
// Stripper: jorono@yandex-team.ru (Josef Novak)
//
// \file
// NGram counting class
// Stripped out of OpenGrn Ngram library and namepsace changes 
// for indepenendent use

#ifndef DCD_NGRAM_COUNT_H__
#define DCD_NGRAM_COUNT_H__

#include <algorithm>
#include <string>
#include <utility>
#include <unordered_map>

#include <fst/fst.h>
#include <fst/mutable-fst.h>
#include <fst/shortest-distance.h>
#include <fst/topsort.h>

namespace dcd {

using std::pair;
using std::vector;
using std::string;
using std::pop_heap;
using std::push_heap;

using std::unordered_map;
using std::hash;

using fst::Fst;
using fst::MutableFst;
using fst::VectorFst;
using fst::ShortestDistance;
using fst::TopSort;

using fst::ArcIterator;
using fst::MutableArcIterator;
using fst::StateIterator;

using fst::kString;
using fst::kTopSorted;
using fst::kUnweighted;

// NGramCounter class:
// TODO: Add a description of the class
template <class W, class L = int32>
class NGramCounter {
 public:
  typedef W Weight;
  typedef L Label;

  // Construct an NGramCounter object counting n-grams of order less
  // or equal to 'order'. When 'epsilon_as_backoff' is 'true', the epsilon
  // transition in the input Fst are treated as failure backoff transitions
  // and would trigger the length of the current context to be decreased
  // by one ("pop front").
  NGramCounter(size_t order, bool epsilon_as_backoff = false,
               float delta = 1e-9F, int bo_ilabel_id = 0,
               int bo_olabel_id = 0)
      : order_(order), pair_arc_maps_(order),
        epsilon_as_backoff_(epsilon_as_backoff), delta_(delta),
        bo_ilabel_id_(bo_ilabel_id), 
        bo_olabel_id_(bo_olabel_id) {
    CHECK(order != 0);
    backoff_ = states_.size();
    states_.push_back(CountState(-1, 1, Weight::Zero(), -1));
    if (order == 1) {
      initial_ = backoff_;
    } else {
      initial_ = states_.size();
      states_.push_back(CountState(backoff_, 2, Weight::Zero(), -1));
    }
  }

  // Extract counts from the input acyclic Fst.  Return 'true' when
  // the counting from the Fst was successful and false otherwise.
  template <class A>
  bool Count(const Fst<A> &fst) {
    if (fst.Properties(kString, false)) {
      return CountFromStringFst(fst);
    } else if (fst.Properties(kTopSorted, true)) {
      return CountFromTopSortedFst(fst);
    } else {
      VectorFst<A> vfst(fst);
      return Count(&vfst);
    }
  }

  // Extract counts from input mutable acyclic Fst, top sort the input
  // fst in place when needed.  Return 'true' when the counting from
  // the Fst was successful and false otherwise.
  template <class A>
  bool Count(MutableFst<A> *fst) {
    if (fst->Properties(kString, true))
      return CountFromStringFst(*fst);
    bool acyclic = TopSort(fst);
    if (!acyclic) {
      //TODO(allauzen): support key in error message.
      LOG(ERROR) << "NGramCounter::Count: input not an acyclic fst";
      return false;
    }
    return CountFromTopSortedFst(*fst);
  }

  // Get an Fst representation of the ngram counts.
  template <class A>
  void GetFst(MutableFst<A> *fst) {
    VLOG(1) << "# states = " << states_.size();
    VLOG(1) << "# arcs = " << arcs_.size();

    fst->DeleteStates();
    for (size_t s = 0; s < states_.size(); ++s) {
      fst->AddState();
      fst->SetFinal(s, states_[s].final_count.Value());
      if (states_[s].backoff_state != -1)
        fst->AddArc(s, A(bo_ilabel_id_, bo_olabel_id_, A::Weight::Zero(),
                         states_[s].backoff_state));
    }
    for (size_t a = 0; a < arcs_.size(); ++a) {
      const CountArc &arc = arcs_[a];
      fst->AddArc(arc.origin,
                  A(arc.label, arc.label, arc.count.Value(), arc.destination));
    }
    fst->SetStart(initial_);
    StateCounts(fst);
  }

  // Given a state ID and a label, returns the ID of the corresponding
  // arc, creating the arc if it does not exist already.
  ssize_t FindArc(ssize_t state_id, Label label) {
    const CountState &count_state = states_[state_id];

    // First determine if there already exists a corresponding arc
    if (count_state.first_arc != -1) {
      if (arcs_[count_state.first_arc].label == label)
        return count_state.first_arc;

      const PairArcMap &arc_map = pair_arc_maps_[count_state.order - 1];
      typename PairArcMap::const_iterator iter =
          arc_map.find(make_pair(label, state_id));
      if (iter != arc_map.end())
        return iter->second;
    }
    // Otherwise, this arc needs to be created
    return AddArc(state_id, label);
  }

  // Gets the start state of the counts (<s>)
  ssize_t NGramStartState() {
    return initial_;
  }

  // Gets the unigram state of the counts
  ssize_t NGramUnigramState() {
    return backoff_;
  }

  // Gets the backoff state for a given state
  ssize_t NGramBackoffState(ssize_t state_id) {
    return states_[state_id].backoff_state;
  }

  // Gets the next state from a found arc
  ssize_t NGramNextState(ssize_t arc_id) {
    if (arc_id < 0 || arc_id >= arcs_.size())
      return -1;
    return arcs_[arc_id].destination;
  }

  // Sets the weight for an n-gram ending with the stop symbol </s>
  bool SetFinalNGramWeight(ssize_t state_id, Weight weight) {
    if (state_id < 0 || state_id >= states_.size())
      return 0;
    states_[state_id].final_count = weight;
    return 1;
  }

  // Sets the weight for a found n-gram
  bool SetNGramWeight(ssize_t arc_id, Weight weight) {
    if (arc_id < 0 || arc_id >= arcs_.size())
      return 0;
    arcs_[arc_id].count = weight;
    return 1;
  }

  // Size of ngram model is the sum of the number of states and number of arcs
 ssize_t GetSize() const {
   return states_.size() + arcs_.size();
 }

 private:
  // Data representation for a state
  struct CountState {
    ssize_t backoff_state;  // ID of the backoff state for the current state
    size_t order;           // N-gram order of the state (of the outgoing arcs)
    Weight final_count;     // Count for n-gram corresponding to superfinal arc
    ssize_t first_arc;      // ID of the first outgoing arc at that state.

    CountState(ssize_t s, size_t o, Weight c, ssize_t a)
        : backoff_state(s), order(o), final_count(c), first_arc(a) {}
  };

  // Data represention for an arc
  struct CountArc {
    ssize_t origin;       // ID of the origin state for this arc
    ssize_t destination;  // ID of the destination state for this arc
    Label label;          // Label
    Weight count;         // Count of the n-gram corresponding to this arc
    ssize_t backoff_arc;  // ID of backoff arc

    CountArc(ssize_t o, size_t d, Label l, Weight c, ssize_t b)
        : origin(o), destination(d), label(l), count(c), backoff_arc(b) {}
  };

  // A pair (Label, State ID)
  typedef pair<ssize_t, ssize_t> Pair;

  struct PairHash {
    size_t operator()(const Pair &p) const {
             return (static_cast<size_t>(p.first) * 55697) ^
                 (static_cast<size_t>(p.second) * 54631);
             // TODO(allauzen): run benchmark using Compose's hash function
             // return static_cast<size_t>(p.first + p.second * 7853);
    }
  };

  typedef unordered_map<Pair, size_t, PairHash> PairArcMap;
  // TODO(allauzen): run benchmark using map instead of unordered map
  // typedef std::map<Pair, size_t> PairArcMap;

  // Create the arc corresponding to label 'label' out of the state
  // with ID 'state_id'.
  size_t AddArc(ssize_t state_id, Label label) {
    CountState count_state = states_[state_id];
    ssize_t arc_id = arcs_.size();

    // Update the hash entry for the new arc
    if (count_state.first_arc == -1)
      states_[state_id].first_arc = arc_id;
    else
      pair_arc_maps_[count_state.order - 1].insert(
          make_pair(make_pair(label, state_id), arc_id));

    // Pre-fill arc with values valid when order_ == 1 and returns
    // if nothing else needs to be done
    arcs_.push_back(CountArc(state_id, initial_, label, Weight::Zero(), -1));
    if (order_ == 1)
      return arc_id;

    // First compute the backoff arc
    ssize_t backoff_arc = count_state.backoff_state == -1 ?
        -1 : FindArc(count_state.backoff_state, label);

    // Second compute the destination state
    ssize_t destination;
    if (count_state.order == order_) {
      // The destination state is the destination of the backoff arc
      destination = arcs_[backoff_arc].destination;
    } else {
      // The destination state needs to be created
      destination = states_.size();
      CountState next_count_state(
          backoff_arc == -1 ? backoff_ : arcs_[backoff_arc].destination,
          count_state.order + 1,
          Weight::Zero(),
          -1);
      states_.push_back(next_count_state);
    }

    // Update destination and backoff_arc with the newly computed values
    arcs_[arc_id].destination = destination;
    arcs_[arc_id].backoff_arc = backoff_arc;

    return arc_id;
  }

  // Increase the count of n-gram corresponding to the arc labeled 'label'
  // out of state of ID 'state_id' by 'count'.
  ssize_t UpdateCount(ssize_t state_id, Label label, Weight count) {
    ssize_t arc_id = FindArc(state_id, label);
    ssize_t nextstate_id = arcs_[arc_id].destination;
    while (arc_id != -1) {
      arcs_[arc_id].count = Plus(arcs_[arc_id].count, count);
      arc_id = arcs_[arc_id].backoff_arc;
    }
    return nextstate_id;
  }

  // Increase the count of n-gram corresponding to the super-final arc
  // out of state of ID 'state_id' by 'count'.
  void UpdateFinalCount(ssize_t state_id, Weight count) {
    while (state_id != -1) {
      states_[state_id].final_count =
          Plus(states_[state_id].final_count, count);
      state_id = states_[state_id].backoff_state;
    }
  }

  template <class A>
  void StateCounts(MutableFst<A> *fst) {
    for (size_t s = 0; s < states_.size(); ++s) {
      Weight state_count = states_[s].final_count;
      if (states_[s].backoff_state != -1) {
        MutableArcIterator< MutableFst<A> > aiter(fst, s);
        ssize_t bo_pos = -1;
        for (; !aiter.Done(); aiter.Next()) {
          const A &arc = aiter.Value();
          if (arc.ilabel != bo_ilabel_id_) {
            state_count = Plus(state_count, arc.weight.Value());
          } else {
            bo_pos = aiter.Position();
          }
        }
        CHECK_GE(bo_pos, 0);
        aiter.Seek(bo_pos);
        A arc = aiter.Value();
        arc.weight = state_count.Value();
        aiter.SetValue(arc);
      }
    }
  }

  template <class A>
  bool CountFromTopSortedFst(const Fst<A> &fst);
  template <class A>
  bool CountFromStringFst(const Fst<A> &fst);

  struct PairCompare {
    bool operator()(const Pair &p1, const Pair &p2) {
      return p1.first == p2.first ? p1.second > p2.second : p1.first > p2.first;
    }
  };

  size_t order_;               // Maximal order of n-gram being counted
  vector<CountState> states_;  // Vector mapping state IDs to CountStates
  vector<CountArc> arcs_;      // Vector mapping arc IDs to CountArcs
  ssize_t initial_;            // ID of start state
  ssize_t backoff_;            // ID of unigram/backoff state
  vector<PairArcMap> pair_arc_maps_;  // Map (label, state ID) pairs to arc IDs
  bool epsilon_as_backoff_;    // Treat epsilons as backoff trans. in input Fsts
  float delta_;                // Delta value used by shortest-distance
  int bo_ilabel_id_;
  int bo_olabel_id_;           // Should always be <eps>, but whatever
  DISALLOW_COPY_AND_ASSIGN(NGramCounter);
};


template <class W, class L>
template <class A>
bool NGramCounter<W, L>::CountFromStringFst(const Fst<A> &fst) {

  CHECK(fst.Properties(kString, false));
  VLOG(2) << "Counting from string Fst";

  ssize_t count_state = initial_;
  typename A::StateId fst_state = fst.Start();
  Weight weight = fst.Properties(kUnweighted, false) ?
      Weight::One() : Weight(ShortestDistance(fst).Value());

  while (fst.Final(fst_state) == A::Weight::Zero()) {
    VLOG(2) << "fst_state = " << fst_state << ", count_state = "
            << count_state;
    ArcIterator< Fst<A> > aiter(fst, fst_state);
    const A &arc = aiter.Value();
    if (arc.ilabel != bo_ilabel_id_) {
      count_state = UpdateCount(count_state, arc.ilabel, weight);
    } else if (epsilon_as_backoff_) {
      ssize_t next_count_state = NGramBackoffState(count_state);
      count_state = next_count_state == -1 ? count_state : next_count_state;
    }
    fst_state = arc.nextstate;
    aiter.Next();
    CHECK(aiter.Done());
  }

  UpdateFinalCount(count_state, weight);
  return true;
}


template <class W, class L>
template <class A>
bool NGramCounter<W, L>::CountFromTopSortedFst(const Fst<A> &fst) {
  CHECK(fst.Properties(kTopSorted, false));

  VLOG(1) << "Counting from top-sorted Fst";

  // Compute shortest-distances from the initial state and to the
  // final states.
  vector<typename A::Weight> idistance;
  vector<typename A::Weight> fdistance;
  ShortestDistance(fst, &idistance, false, delta_);
  ShortestDistance(fst, &fdistance, true, delta_);

  vector<Pair> heap;
  unordered_map<Pair, typename A::Weight, PairHash> pair2weight_;
  PairCompare compare;
  Pair start_pair = make_pair(fst.Start(), initial_);
  pair2weight_[start_pair] = A::Weight::One();
  heap.push_back(start_pair);
  push_heap(heap.begin(), heap.end(), compare);

  size_t i = 0;

  while (!heap.empty()) {
    pop_heap(heap.begin(), heap.end(), compare);
    Pair current_pair = heap.back();
    typename A::StateId fst_state = current_pair.first;
    ssize_t count_state = current_pair.second;
    typename A::Weight current_weight = pair2weight_[current_pair];
    pair2weight_.erase(current_pair);
    heap.pop_back();

    VLOG(2) << "fst_state = " << fst_state << ", count_state = "
            << count_state;

    if (i % 1000000 == 0) {
      VLOG(2) << " # dequeued = " << i << ", heap cap = " <<
          heap.capacity() << ", hash cap = " << pair2weight_.max_size();
    }
    ++i;


    for (ArcIterator<Fst<A> > aiter(fst, fst_state); !aiter.Done();
         aiter.Next()) {
      const A &arc = aiter.Value();
      Pair next_pair(arc.nextstate, count_state);
      if (arc.ilabel != bo_ilabel_id_) {
        Weight count = Times(
            current_weight, Times(Times(idistance[fst_state], arc.weight),
                                  fdistance[arc.nextstate])).Value();
        next_pair.second = UpdateCount(count_state, arc.ilabel, count);
      } else if (epsilon_as_backoff_) {
        ssize_t next_count_state = NGramBackoffState(count_state);
        next_pair.second =
            next_count_state == -1 ? count_state : next_count_state;
      }
      typename A::Weight next_weight = Times(
          current_weight, Divide(Times(idistance[fst_state], arc.weight),
                                 idistance[arc.nextstate]));
      typename unordered_map<Pair, typename A::Weight, PairHash>::iterator
          iter = pair2weight_.find(next_pair);
      if (iter == pair2weight_.end()) {  // If pair not in heap, insert it.
        pair2weight_[next_pair] = next_weight;
        heap.push_back(next_pair);
        push_heap(heap.begin(), heap.end(), compare);
      } else {  // Otherwise, update the weight stored for it.
        iter->second = Plus(iter->second, next_weight);
      }
    }

    if (fst.Final(fst_state) != A::Weight::Zero()) {
      UpdateFinalCount(
          count_state,
          Times(current_weight,
                Times(idistance[fst_state], fst.Final(fst_state))).Value());
    }
  }
  return true;
}


}  // namespace dcd

#endif  // DCD_NGRAM_COUNT_H__
