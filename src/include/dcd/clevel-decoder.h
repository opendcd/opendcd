// arc-decoder.h
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
// Basic CLevelDecoder class

//Most classes have a Clear() method that will reset the internals of
//a class this more efficient than destroying and allocating new classes

#ifndef DCD_HMM_ARC_DECODER_H__
#define DCD_HMM_ARC_DECODER_H__

#include <algorithm> //For min
#include <fstream>
#include <iostream>
#include <limits>

#include <fst/fst.h>
#include <fst/mutable-fst.h>
#include <fst/project.h>
#include <fst/rmepsilon.h>
#include <fst/shortest-path.h>
#include <fst/vector-fst.h>

#include <dcd/config.h>
#include <dcd/constants.h>
#include <dcd/lattice.h>
#include <dcd/log.h>
#include <dcd/search-statistics.h>
#include <dcd/stl.h>
#include <dcd/token.h>
#include <dcd/utils.h>
#include <dcd/search-opts.h>


namespace dcd {

using std::ifstream;
using std::min;

using fst::ArcIterator;
using fst::StdArc;
using fst::VectorFst;
using fst::MutableFst;

using fst::kNoStateId;
using fst::PROJECT_OUTPUT;
using fst::AutoQueue;

//Forward declations for the various decoder classes
//these classes need to know the names of the other types
template<class FST, class TransModel, class L, class Statistics>
class CLevelDecoder;

template<class Weight>
inline float Value(const Weight& w, int time = -1) {
  return w.Value();
}

inline float Value(const KaldiLatticeWeight& w, int time = -1) {
  return w.Value1() + w.Value2();
}

//Templated on the Fst and acoustic model interfaces
//FST is uppercase to avoid collision with OpenFst naming
template<class FST, class TransModel, class L,
  class Statistics = NullStatistics>
class CLevelDecoder {
 public:
  class SearchArc;
  class SearchState;
  typedef typename L::LatticeState LatticeState;
  typedef TokenTpl<LatticeState> Token;
  typedef Pair<float,float> FloatPair;

  typedef typename HashMapHelper<int, SearchState*>::HashMap SearchHash;
  typedef typename VectorHelper<SearchState*>::Vector ActiveStateVector;
  typedef typename VectorHelper<SearchArc*>::Vector ActiveArcVector;
  typedef typename VectorHelper<SearchArc>::Vector ArcVector; 
  typedef deque<SearchState*> EpsQueue;

  struct ArcExpandOptions {
    ArcExpandOptions (float best, float threshold, float lbest, 
        float lthreshold, Token* tokens, Token* scratch, 
        const SearchOptions& opts, L* lattice ) 
      : best_(best), threshold_(threshold), lbest_(lbest), 
      lthreshold_(lthreshold), tokens_(tokens), scratch_(scratch), 
      opts_(opts), lattice_(lattice) { }
    typedef TokenTpl<LatticeState> Token;
    float best_;
    float threshold_;
    float lbest_;
    float lthreshold_;
    Token* tokens_;
    Token* scratch_; //temporary buffer
    const SearchOptions& opts_;
    L* lattice_;
  };

  class SearchArc {
   public:
    SearchArc(int ilabel, int olabel, float weight, int nextstate,
        float exit_weight, int num_states) 
      : dest_(0), ilabel_(ilabel), olabel_(olabel), weight_(weight), 
      nextstate_(nextstate), exit_weight_(exit_weight), 
      num_states_(num_states), num_expansions_(0), time_(-1) {
        //tokens_.resize(num_states + 1);
        ClearTokens();
      }

    //If cost to set the first token is less than the 
    //returns the costs associated with the first token in the arc
    float SetEntryToken(const Token& token, float threshold, int time,
        ActiveArcVector* active_arcs, const SearchOptions& opts) {
      if (token.Cost() + weight_ < threshold) {
        //The first token may not be a place holder
        //so we should call the weighted combine
        if (active_arcs && time_ == -1) {
          ClearTokens();
          num_expansions_ = 0;
          time_ = time;
          active_arcs->push_back(this);
        }
        float cost = tokens_[0].Combine(token, weight_);
        return cost;
      } 
      //Should be kMaxCost if nothing was activated
      return kMaxCost;
    }

    //active arcs have a position in the list
    bool Active() const {
      return time_ != -1;
    }

    //Cost to leave the arc
    float GetExitCost() const {
      return tokens_[num_states_].Cost() + exit_weight_;
    }

    //Last token to leave the arc
    Token GetExitToken() const {
      return tokens_[num_states_].Expand(exit_weight_);
    }

    const Token& GetToken(int i) const {
      return tokens_[i];
    }

    //Returns the best active token cost from 
    //inside the arc; 
    //TODO Maybe add the final hmm exit cost here 
    float GetBestCost() const {
      //Shouldn't need to check if the token is active
      //because a non active token should return kMaxCost
      float best = tokens_[0].Cost();
      for (int i = 1; i <= num_states_; ++i)
        best = min(best, tokens_[i].Cost());
      return best;
    }

    //Advance the tokens in the arc by one frame
    //and return the best token cost;
    inline pair<float, float> Expand(TransModel* transmodel,
        ArcExpandOptions* opts) {
      PROFILE_FUNC();
      ++num_expansions_;
      opts->tokens_ = &tokens_[0];
      return transmodel->Expand(ilabel_, opts);
    }

    float AcousticLookahead(TransModel* transmodel, int time) {
      return transmodel->AcousticLookahead(ilabel_, time);
    }

    float ExpandEpsilons(const TransModel& transmodel) {
      return kMaxCost;
    }

    //Returns the cost associated with the newly 
    //activated state. Cost could change due to
    //insertion penalty or duration penally, or get better due
    //on-the-fly rescoring
    float ExpandToFollowingState(CLevelDecoder* decoder) {
      if (!tokens_[num_states_].Active())
        return kMaxCost;
      SearchState* ss = FindNextState(*decoder);
      return kMaxCost;
    }

    //Get the last token associated with the Arc
    const Token& GetLastToken() const {
      return tokens_[num_states_];
    }

    bool HasActiveTokens() const {
      for (int i = 0; i <= num_states_; ++i)
        if (tokens_[i].Active())
          return true;
      return false;
    }

    //Prune the arcs internally and return true if tokens are active
    //threshold would usually be best_cost + beam
    bool Prune(float threshold) {
      bool active = false;
      for (int i = 0; i <= num_states_; ++i)
        if (tokens_[i].Cost() + threshold)
          tokens_[i].Clear();
        else 
          active = true;
      return active;
    }

    //Template this so we don't need to see a declaration
    //for the decoders type
    SearchState* FindNextState(CLevelDecoder* decoder) {
      if (!dest_)
        dest_ = decoder->FindSearchState(nextstate_);
      return dest_;
    }

    //Clear all the arc information. Used the when
    //the arc is permanently deactivated or 
    void Clear() {
      for (int i = 0; i < kMaxTokensPerArc; ++i) 
        tokens_[i].Clear();
      dest_ = 0;
      num_states_ = 0;
      nextstate_ = kNoStateId;
      weight_ = kMaxCost;
      time_ = -1;
      num_expansions_ = 0;
    }

    void ClearTokens() {
      for (int i = 0; i < kMaxTokensPerArc; ++i)
        tokens_[i].Clear();
    }

    int Deactivate(ActiveArcVector* active_arcs, int time) {
      PROFILE_FUNC();
      time_ = -1;
      ClearTokens();
      return num_expansions_;
    }

    void GcMark() {
      for (int i = 0; i <= num_states_; ++i)
        if (tokens_[i].Active())
          tokens_[i].GcMark();
    }

    //Accessors for the arc fields, useful to have a
    //function for collecting access stats
    inline int ILabel() const { return ilabel_; }

    inline int OLabel() const { return olabel_; }

    inline float Weight() const { return weight_; }

    inline int NextState() const { return nextstate_; }

    inline int NumStates() const { return num_states_; }

    inline const Token* Tokens() const { return &tokens_[0]; }

    string ToString() const { 
      stringstream ss;
      ss << ilabel_ << " " << olabel_ << " " << weight_ << " " << nextstate_;
      return ss.str();
    }

  protected:
    //Extra token for entry token
    //Token tokens_[kTokensPerArc + 1];
    //TODO allow for various token containers
    //vector<Token> tokens_;
    Token tokens_[kMaxTokensPerArc];
    SearchState* dest_; //Store a pointer to the next state
    int ilabel_; //input label of the search transducer
    int olabel_; //output label of the search transducer
    float weight_; //The weight from the Fst, potentially scaled
    int nextstate_; //next state of the search transducer
    float exit_weight_; //Weight for the final transition in the 
    ///HMM and/or insertion  penalty
    int num_states_; //number of states in the underlying transition model
    int num_expansions_;
    int time_; //time the arc was activated/deactivated
  };

  //Comparison for sorting the active arcs
  struct SearchArcILabelCompare {
    bool operator ()(const SearchArc* a, const SearchArc* b) { 
      return a->ILabel() < b->ILabel(); 
    }
  };

  struct SearchArcOLabelCompare {
    bool operator ()(const SearchArc* a, const SearchArc* b) { 
      return a->OLabel() < b->OLabel(); 
    }
  };

  struct SearchArcCostCompare {
    bool operator ()(const SearchArc* a, const SearchArc* b) { 
      return a->GetBestCost() < b->GetBestCost();
    }
  };

  class SearchState {
   public:
    SearchState() 
      : last_activated_(-1), ref_count_(0), index_(-1), 
        state_id_(-1), num_activations_(0), in_eps_queue_(false) { }
    //Initialize  a new search state and arcs
    //The transducers type F
    template<class F, class T>
      bool Init(const F& fst, int state, const T& trans_model, const SearchOptions& opts) {
        PROFILE_FUNC();
        typedef typename F::Arc Arc;
        state_id_ = state;
        eps_arcs_.clear();
        arcs_.clear();
        token_.Clear();
        last_activated_ = -1;
        num_activations_ = 0;
        ref_count_ = -1;
        index_ = -1;
        PROFILE_BEGIN(UnderlyingFstStateExpansion);
        //Might not be correct due to disam symbols
        int num_eps_arcs = fst.NumInputEpsilons(state); 
        int num_arcs = fst.NumArcs(state) - num_eps_arcs;
        final_cost_ = Value(fst.Final(state));
        PROFILE_END();
        eps_arcs_.reserve(num_eps_arcs);
        arcs_.reserve(num_arcs);
        for (ArcIterator<F> aiter(fst, state); !aiter.Done(); aiter.Next()) {
          const Arc& arc = aiter.Value();
          //There are three cases
          //label > 0 standard transition arc
          //label = 0 epsilon arc
          //label < 0 disambiguation symbol to be treats
          //the same as an epsilon transition

          //verify the transition label is valid, can crash
          //if aux symbols are left in the CLG, or logical to
          //physical mapping is forgotten
          if (!trans_model.IsValidILabel(arc.ilabel))
            return false;

          bool iseps = trans_model.IsNonEmitting(arc.ilabel);
          //TODO what about emitting negative costs loops
          if (iseps && state == arc.nextstate && Value(arc.weight) < 0) {
            logger(FATAL) << "Negative epsilon cycle detected!";
            return false;
          }
          //TODO is there any reason to allow epsilon transition to have
          //more than one token state?
          int num_states = iseps ? 0 : trans_model.NumStates(arc.ilabel);
          ArcVector& arcs = iseps ? eps_arcs_ : arcs_;
          //Idea: If we always have to add the cost of the last transition
          //of the HMM then add it to the arc weight and prune with it before
          //we enter the state. 
          //Answer: Doesn't seem to work well
          float exit_weight = trans_model.GetExitWeight(arc.ilabel);
          arcs.push_back(SearchArc(arc.ilabel, arc.olabel,
                Value(arc.weight) + opts.insertion_penalty, arc.nextstate, exit_weight, num_states));
        }
        return true;
      }

    //Expand state in active arcs if the state cost plus the arc
    //cost is less than the pruning threshold
    float ExpandIntoArcs(ActiveArcVector* active_arcs, float threshold, 
        int time, const SearchOptions& opts) {
      float best = kMaxCost;
      for (int i = 0; i != arcs_.size(); ++i) 
        best = min(best, arcs_[i].SetEntryToken(token_, threshold, time, 
              active_arcs, opts));
      return best;
    }

    void AddToEpsQueue(EpsQueue* q) {
      assert(!InEpsQueue());
      if (!InEpsQueue())
        q->push_back(this);
    }

    //Returns the best cost that was activated
    float ExpandEpsilonArcs(ActiveStateVector* active_states, EpsQueue* q,
        CLevelDecoder* decoder, float threshold, const SearchOptions& opts) {
      float best = kMaxCost;
      int num_activated = 0;
      for (int i = 0; i != eps_arcs_.size(); ++i) {
        SearchArc* sa = &eps_arcs_[i];
        //Pass null pointer as epsilons are never in the
        //active arcs lists
        sa->SetEntryToken(token_, threshold, -1, 0, opts);
        Token token = sa->GetExitToken();
        //TODO maybe not prune until adding a rescoring cost
        //in the state expansion
        if (token.Cost() < threshold) {
          pair<SearchState*, float> p = decoder->ExpandToFollowingState(sa, 
              threshold);
          SearchState* ss = p.first;
          if (ss) {
            ++num_activated;
            if (ss->NumEpsilons() && !ss->InEpsQueue()) {
              ss->AddToEpsQueue();
              q->push_back(ss);
            }
          } 
          best = min(best, p.second);
        }
        sa->ClearTokens();
        assert(!sa->HasActiveTokens());
      }
      return best;
    }

    //A state is active if it has an active token
    bool Active() const { return token_.Active(); }

    //Returns the number of epsilon like transitions
    //associated with the state
    int NumEpsilons() const { return eps_arcs_.size(); }

    //Returns the number of emitting transitions leaving
    //the state
    int NumEmitting() const { return arcs_.size(); }

    //The State Id from the underlying 
    int StateId() const { return state_id_; }

    //Position in the active state list
    int Index() const { return index_; }

    //Set the token in the state and activates if necessary
    //if active and lower cost, updates the cost
    //TODO: Maybe incorporating the best scoring arc and pruning on 
    //that could help
    float SetToken(const Token& token, int time,
        ActiveStateVector* active_states) {
      if (!Active()) {
        token_ = token;
        last_activated_ = time;
        index_ = active_states->size();
        active_states->push_back(this);
      } else {
        if (token_.Cost() > token.Cost())
          token_ = token;
        assert(last_activated_ == time);
      }
      last_activated_ = time;
      return Cost();
    }

    //TODO template the state on the lattice type?
    //template<class L>
    pair<float, float> SetToken(const SearchArc& arc, int time, float threshold, 
        ActiveStateVector* active_states, L* lattice, 
        const SearchOptions& opts) {
      Token token = arc.GetExitToken();
      pair<float, float> cost(kMaxCost, kMaxCost);
      if (token.Cost() < threshold) {
        cost = token_.Combine(token, arc, lattice, time, threshold, opts);
        if (index_ == -1) {
          last_activated_ = time;
          index_ = active_states->size();
          active_states->push_back(this);
        }
      }
      return cost;
    }

    //Resets the search state back to the default
    //uninitialized state. Called before returning
    //the search state back to a memory pool
    void Clear() {
      arcs_.clear();
      eps_arcs_.clear();
      final_cost_ = kMaxCost;
      in_eps_queue_ = false;
      index_ = - 1;
      last_activated_ = -1;
      ref_count_ = 0;
      state_id_ = -1;
      token_.Clear();
    }

    //Clear just the token
    void ClearToken() {
      token_.Clear();
    }

    const Token& GetToken() const { return token_; }

    float Cost() const { return token_.Cost(); }

    float AcceptCost() const { return token_.Cost() + final_cost_; }

    //Active the state and add it to the active state list
    void Activate(const Token& token, int time, 
        ActiveStateVector* active_states) {
      //Already active
      if (Active()) {
        //Sanity check this is in the list
        if ((*active_states)[index_] != this)
          logger(FATAL) << "Active state not in the correct position";
        //Update cost if token is better
        if (Cost() > token.Cost())
          token_ = token;
      } else {
        //Not active so put in the list and store the position
        token_ = token;
        index_ = active_states->size();
        active_states->push_back(this);
        ++num_activations_;
      }
      last_activated_ = time;
    }

    void Deactivate(ActiveStateVector* active_states) { 
      token_.Clear();
      (*active_states)[index_] = 0;
      index_ = -1;
    }

    bool HasActiveArcs() const {
      for (int i = 0; i != arcs_.size(); ++i) 
        if (arcs_[i].HasActiveTokens())
          return true;
      //Maybe cache if the arc is active when expanding
      //if (arcs_[i].Active()) 
      //  return true;
      return false;
    }

    //Returns the single best path through the lattice
    template<class Arc>
      void GetBestSequence(VectorFst<Arc>* best) const {
        best->DeleteStates();
        int s = token_.GetBestSequence(best);
        best->SetFinal(s, Arc::Weight::One());
      }

    template<class Arc> 
      void GetLattice(VectorFst<Arc>* ofst) const {
        token_.GetLattice(ofst);
      }

    //When caching a pointer in the SearchArcs
    //We need to know how many arcs are referencing
    //the state. Only when the ref_count_ is zero
    //is it safe to collect a search state
    int RefCount() const {
      return ref_count_;
    }

    int IncrRefCount() {
      return ++ref_count_;
    }

    int DecRefCount() {
      return --ref_count_;
    }

    void ClearRefCount() {
      ref_count_ = 0;
    }

    void AddToEpsQueue() {
      in_eps_queue_ = true;
    }

    void RemoveFromEpsQueue() {
      in_eps_queue_ = false;
    }

    void DumpInfo(Logger* logger) const {
      (*logger)(DEBUG) 
        << "SearchState : " << state_id_ << endl 
        << "\t# emitting arcs " << arcs_.size() << endl
        << "\t# epsilon arcs " << eps_arcs_.size() << endl
        << "\tIndex " << index_ << " last activated " << last_activated_;
    }

    bool InEpsQueue() const { return in_eps_queue_; }

    typename VectorHelper<SearchArc>::Vector arcs_;
    typename VectorHelper<SearchArc>::Vector eps_arcs_;
    //Best token associated with this state
    Token token_;
    //Time to indicate when the state was last activated
    int last_activated_; 
    int ref_count_;
    int index_;  //Where the state is stored in the active state list
    int state_id_;
    int num_activations_;
    float final_cost_;
    //Epsilon expansion uses a generic SSSP algorithm
    //This flag is used to indicate if the state is already 
    //in the queue in case we need to perform a 'decrease key'
    //operation
    bool in_eps_queue_;
  private:
    DISALLOW_COPY_AND_ASSIGN(SearchState);
  };

 public:
  CLevelDecoder(FST* fst, TransModel* trans_model, const SearchOptions& opts, 
      ostream* logstream = &std::cerr, L* lattice = 0) 
    : fst_(fst), trans_model_(trans_model), search_opts_(opts),
    lattice_(0), logger_("dcd-recog", *logstream, opts.colorize), 
    time_(-1), debug_(true), num_search_state_allocs_(0), 
    num_search_state_frees_(0) { 
      active_arcs_.reserve(kDefaultActiveListSize);
      active_states_.reserve(kDefaultActiveListSize);
      if (lattice) {
        lattice_ = lattice;
        owns_lattice_ = false;
      } else {
        lattice_ = new L(opts, logstream);
        owns_lattice_ = true;
      }
    }

  virtual ~CLevelDecoder() {
    lattice_->Clear();
    if (owns_lattice_)
      delete lattice_;
    ClearSearchHash();
    ClearSearchPool();
  }

  //Pretty terrible!
  void PrintFrameUsage() {
    if (FLAGS_v >= 1) {
      logger_(INFO) << "time " << time_ << " current memory usage " 
        << GetCurrentRSS() / (1024 * 1024) << "MB" << endl 
        << "\t\t  # active arcs " << num_active_arcs_ 
        << ", # arcs pruned " << num_arcs_pruned_ 
        << ", # arcs after expansion " 
        << num_active_arcs_after_prune_
        << ", best arc cost " << best_arc_cost_ 
        << ", worst arc cost " << worst_arc_cost_
        << ", unique ilabels " << num_unique_ilabels_ << endl 
        << "\t\t  # active states " << num_active_states_  
        << ", # of active states after eps expansion " 
        << num_epsilons_activated_
        << ", # epsilon cycles " << num_epsilon_cycles_ << endl
        << "\t\t  best state cost " << best_state_cost_  
        << ", worst state cost " << worst_state_cost_ << endl
        << "\t\t  search state requests " 
        <<  num_search_state_requests_ 
        << ", cache hits " << num_search_state_hits_ 
        << ", misses " << num_search_state_misses_;
    }
  }

  void ClearSearchStats() {
    num_active_arcs_ = 0;
    num_unique_ilabels_ = 0;
    num_arcs_pruned_ = 0;
    best_arc_cost_ = kMaxCost;
    worst_arc_cost_ = kMinCost;
    num_active_states_ = 0;
    num_epsilons_activated_ = 0;
    best_state_cost_ = kMaxCost;
    worst_arc_cost_ = kMinCost;
    num_search_state_misses_  = 0;
    num_search_state_requests_ = 0;
    num_search_state_hits_ = 0;

    total_num_arcs_pruned_ = 0;
    total_num_arc_expanded_ = 0;
    total_num_arcs_pruned_ = 0;
    total_num_arcs_lookahead_pruned_ = 0;
    total_num_arcs_hisogram_pruned_ = 0;

    total_num_states_expanded_ = 0;
    total_num_states_pruned_ = 0;
    total_num_epsilson_states_relaxed_ = 0;
    total_num_epsilson_states_pruned_ = 0;
    max_active_arcs_ = 0;
    max_active_states_ = 0;

    num_exit_tokens_ = 0;
    num_exit_tokens_pruned_ = 0;
  }

  //Write the output to ofst and optionally lattice to fst
  template<class ARC>
  float Decode(TransModel* trans_model, const SearchOptions& opts,
      VectorFst<ARC>* ofst, VectorFst<ARC>* lfst = 0) {
    PROFILE_FUNC(); 
    ClearSearchStats();
    logger_(INFO) << "Pool usage :" << endl
      << "\t\t  # in search hash " << search_hash_.size()
      << " # active arcs " << active_arcs_.size() 
      << " # active states " << active_states_.size()
      << " # of lattice states in pool " 
      << lattice_->FreeListSize() 
      << " # of allocs " << num_search_state_allocs_ 
      << " # of frees " << num_search_state_frees_
      << " # search states in pool " 
      << search_state_pool_.size();

    trans_model_ = trans_model;
    
    timer_.Reset();
    double timer_begin_decode_ = 0;
    double timer_expand_search_states_ = 0;
    double timer_expand_search_arcs_ = 0;
    double timer_expand_eps_arcs_ = 0;
    double timer_gc_ = 0;
    double timer_end_decode_ = 0;
    double timer_other_ = 0;
    double timer_next_frame_ = 0;
    double start_time = timer_.Elapsed();
    double time = timer_.Elapsed();
    
    if (!BeginDecode())
      logger_(FATAL) << "BeginDecode failed to activate any search states";
    timer_begin_decode_ = timer_.Elapsed() - time;
    PrintFrameUsage();
    for (; !trans_model->Done();) {
      ++time_;
      time = timer_.Elapsed();
      ExpandActiveStates();
      timer_expand_search_states_ += timer_.Elapsed() - time;
      if (search_opts_.gc_period > 0 && 
          (time_ + 1) % search_opts_.gc_period == 0) {
        time = timer_.Elapsed();
        VLOG(1) << "Performing GC at frame " << time_;
        Gc();
        timer_gc_ += timer_.Elapsed() - time;
      }
      time = timer_.Elapsed();
      ExpandActiveArcs();
      timer_expand_search_arcs_ += timer_.Elapsed() - time;
      
      time = timer_.Elapsed();
      ExpandEpsilonArcs();
      timer_expand_eps_arcs_ += timer_.Elapsed() - time;

      PrintFrameUsage();
      
      time = timer_.Elapsed();
      trans_model_->Next();
      timer_next_frame_ += timer_.Elapsed() - time;
    }
    time = timer_.Elapsed();
    float best_cost = EndDecode(ofst, lfst, search_opts_.nbest);
    timer_end_decode_ = timer_.Elapsed() - time;
    VLOG(1) << "End decode found best cost " << best_cost;
    //This slows things here if we destroy the decoder after each utterance
    CleanUp();
    double end_time = timer_.Elapsed() - start_time;
    timer_other_ = end_time - timer_end_decode_ - timer_expand_eps_arcs_ - 
      timer_expand_search_arcs_ - timer_expand_search_states_ - timer_gc_
      - timer_begin_decode_;

    lattice_->DumpInfo();
    logger_(INFO) << "Search usage summary :" << endl
      << "\t\t  # of arcs expanded " 
      << total_num_arc_expanded_ 
      << " # of arcs pruned " 
      << total_num_arcs_pruned_ 
      << " # of arcs lookahead pruned "
      << total_num_arcs_lookahead_pruned_
      << " # of arcs histogram pruned " 
      << total_num_arcs_hisogram_pruned_
      << " Max active arcs " 
      << max_active_arcs_ 
      << " Arc exit token prune ratio " 
      << num_exit_tokens_pruned_ * 100.0f / num_exit_tokens_ << endl
      << "\t\t  # of states expanded "
      << total_num_states_expanded_
      << " # of states pruned "
      << total_num_states_pruned_ 
      << " Max active states " 
      << max_active_states_ << endl
      << "\t\t  # of epsilons expanded "
      << total_num_epsilson_states_relaxed_ << endl
      << "\t\t  # of search state misses "
      << num_search_state_misses_;

    double timer_sum = timer_expand_search_arcs_ + timer_expand_search_states_ + 
      timer_expand_eps_arcs_ + timer_gc_  + timer_end_decode_  + 
      timer_next_frame_ + timer_begin_decode_ + timer_other_;

    logger_(INFO) << "Search profile : " << endl
      << "\t\t  Arcs " <<  timer_expand_search_arcs_ / end_time
      << ", States " << timer_expand_search_states_ / end_time        
      << ", Epsilons " << timer_expand_eps_arcs_ / end_time
      << ", GC " << timer_gc_ / end_time
      << ", End " << timer_end_decode_ / end_time 
      << ", Decodable " << timer_next_frame_ / end_time
      << ", Other " << (timer_begin_decode_ + timer_other_)  / end_time 
      << " (Sum " << timer_sum / end_time << ")";
    return best_cost;
  }

  void ClearSearchHash() {
    PROFILE_FUNC();
    for (typename SearchHash::iterator it = search_hash_.begin();
        it != search_hash_.end(); ++it)
      FreeSearchState(it->second);
    search_hash_.clear();
    if (num_search_state_allocs_ != num_search_state_frees_)
      logger_(FATAL) << "Search state allocation mismatch detected : " 
        << " # Allocs : " << num_search_state_allocs_
        << " # Frees : " << num_search_state_frees_;
  }

  void CleanUp() {
    PROFILE_FUNC();
    lattice_->Clear();
    ClearSearchHash();
    ClearSearch();
    time_ = -1;
  }

  //Lattice debugging feature to dump the current traceback
  //to an fst, graphviz can be used to then visualize the lattice
  bool DumpTraceBackToFst(const string& path) {
    StdVectorFst ofst;
    SymbolTable ssyms("state.syms");
    lattice_->DumpToFst(&ofst, &ssyms);
    ssyms.WriteText(path + ".ssyms");
    return ofst.Write(path);
  }

  int Gc() {
    PROFILE_FUNC();
    if (search_opts_.dump_traceback) {
      stringstream ss;
      ss << FLAGS_tmpdir << time_ <<  "_" << search_opts_.source 
        << "_pre.fst";
      DumpTraceBackToFst(ss.str());
    }

    lattice_->GcClearMarks();
    for (int i = 0; i != active_arcs_.size(); ++i) {
      SearchArc* search_arc = active_arcs_[i];
      search_arc->GcMark();
    }
    vector<int> early_mission;
    int lattice_num_reclaimed = 
      lattice_->GcSweep(search_opts_.early_mission ?  &early_mission : 0);
    
    if (early_mission.size() && search_opts_.wordsyms) {
      if (early_mission.size()) {
        stringstream ss;
        ss << "Partial : ";
        for (int i = 0; i != early_mission.size(); ++i) {
          ss << search_opts_.wordsyms->Find(early_mission[i]);
          if (i < early_mission.size() - 1)
            ss << " ";
        }
        logger_(INFO) << ss.str();
      }
    }
    
    if (search_opts_.dump_traceback) {
      stringstream ss;
      ss << FLAGS_tmpdir << time_ <<  "_" << search_opts_.source 
        << "_post.fst";
      DumpTraceBackToFst(ss.str());
    }
    
    if (search_opts_.gc_check)
      GcCheck();

    return lattice_num_reclaimed;
  }

  //Go through the tokens and check the lattice states are in
  //the used lists
  bool GcCheck() {
    lattice_->Check();
    lattice_->SortLists();
    for (int i = 0; i != active_arcs_.size(); ++i) {
      SearchArc* arc = active_arcs_[i];
      if (arc) {
        for (int j = 0; j != arc->NumStates(); ++j) {
          Token token = arc->GetToken(j);
          if (token.Active()) {
            if (!lattice_->IsActive(token.LatticeState()))
              logger_(FATAL) << "Lattice state is not in the active list";
          }
        }
      }
    }
    return true;
  }

  int SearchGc() {
    int num_reclaimed = 0;
    for (typename SearchHash::iterator it = search_hash_.begin();
        it != search_hash_.end();) {
      SearchState* ss = it->second;
      if (!ss->RefCount()) {
        FreeSearchState(ss);
        ++num_reclaimed;
        typename SearchHash::iterator jt = it++;
        search_hash_.erase(jt);
      } else {
        ++it;
      }
    }
    return num_reclaimed;
  }

  void DumpInfo() {
    return;
    if (!debug_) return;
    logger_(DEBUG) << "Decoder Info:" << endl
      << "\tCurrent time " << time_ << endl
      << "\t# Active states " << active_states_.size() << endl
      << "\t# Active arcs " << active_arcs_.size() << endl
      << "\t# Cached search states " << search_hash_.size();
    lattice_->DumpInfo();
  }

  //Prepare decoder for search and add the initial state to
  //active lists
  //Will return false if the fst doesn't have a valid start state
  //or the active state list is empty
  bool BeginDecode() {
    PROFILE_FUNC();
    int s = fst_->Start();
    if (s == kNoStateId)
      return false;
    LatticeState ls = lattice_->CreateStartState(s);
    Token token(ls, 0); //Create token associated with the start lattice state 
    //and set the cost to zero.
    SearchState* ss = FindSearchState(s);
    ss->Activate(token, time_, &active_states_);
    ExpandEpsilonArcs();
    return !active_states_.empty();
  }

  SearchState* FindBestState(int finalmode = kBackoff) {
    SearchState* best_state = 0;
    float best_cost = kMaxCost;
    if (finalmode != kAnyFinal) {
      for (int i = 0; i != active_states_.size(); ++i) {
        SearchState* ss = active_states_[i];
        if (ss->AcceptCost() < best_cost) {
          best_cost = ss->AcceptCost();
          best_state = ss;
        }
      }
      //Found a final state or we didn't but non-final aren't allowed
      if (best_state || finalmode == kRequireFinal)
        return best_state;
    }
    for (int i = 0; i != active_states_.size(); ++i) {
      SearchState* ss = active_states_[i];
      if (ss->Cost() < best_cost) {
        best_cost = ss->AcceptCost();
        best_state = ss;
      }
    }
    return best_state;
  }

  //Here ARC is the templated on the underlying semiring of the output lattice
  //which in most case will be different to the semiring of the search Fst
  template<class ARC>
  float EndDecode(VectorFst<ARC>* ofst, fst::MutableFst<ARC>* lattice = 0,
      int n = 0) {
    PROFILE_FUNC();
    SearchState* ss = FindBestState(); 
    if (ss) {
      ss->GetBestSequence(ofst);
      //optionally generate the lattice arcs
      if (lattice) {
        int numarcs = 
          lattice_->GetLattice(ss->GetToken().LatticeState(), lattice);

        logger_(INFO) << "Generated lattice with " 
          << numarcs << " arcs "
          << lattice->NumStates() << " states ";
        
        if (n != 0 ) {
          fst::Project(lattice, PROJECT_OUTPUT);
          fst::RmEpsilon(lattice);          
          vector<typename ARC::Weight> d;
          VectorFst<ARC> nbest;
          typedef AutoQueue<typename ARC::StateId> Q;
          AnyArcFilter<ARC> filter; 
          Q q(*lattice, &d, filter);
          ShortestPathOptions<ARC, Q, AnyArcFilter<ARC> > opts(&q, filter);
          opts.nshortest = abs(n);
          opts.unique = n > 0;
          fst::ShortestPath(*lattice, &nbest, &d, opts);
          logger_(INFO) << "NBest Generated with " << nbest.NumStates()
            << " states";
          *lattice = nbest;
        }
      }
      return ss->Cost();
    }
    return kMaxCost;
  }

  float ComputeBand() {
    PROFILE_FUNC();
    vector<float> costs;
    for (int i = 0; i != active_arcs_.size(); ++i) {
      SearchArc* search_arc = active_arcs_[i];
      if (search_arc && search_arc->Active())
        costs.push_back(search_arc->GetBestCost());
    }
    if (costs.size() > search_opts_.band) {
      std::nth_element(costs.begin(), costs.begin() + search_opts_.band, 
          costs.end());
      return costs[search_opts_.band];
    }
    return kMaxCost;
  }

  void SwapArcs(int i, int j) {
    PROFILE_FUNC();
    SearchArc* a = active_arcs_[i];
    active_arcs_[i] = active_arcs_[j];
    active_arcs_[j] = a;
  }

  struct ArcExpandResults2 {
    ArcExpandResults2() 
      : best_arc_cost(kMaxCost), worst_arc_cost(kMinCost),
      threshold(kMaxCost), best_arc_index(-1), num_expanded(0),
      num_pruned(0), best_lookahead_cost(kMaxCost),
      lookahead_threshold(kMaxCost), num_lookahead_pruned(0) { }
    float best_arc_cost;
    float worst_arc_cost;
    float threshold;
    int best_arc_index;
    int num_expanded;
    int num_pruned;
    float best_lookahead_cost;
    float lookahead_threshold;
    int num_lookahead_pruned;
  };


  typedef Triple<float, float, int> ArcExpandResults;

  ArcExpandResults ExpandActiveArcs_ArcExpansion(int begin, int end) {
    PROFILE_FUNC();
    float best_arc_cost = kMaxCost;
    float best_lookahead_cost = kMaxCost;
    float lookahead_threshold = kMaxCost;
    float worst_arc_cost = -kMaxCost;
    int best_index = -1;
    total_num_arc_expanded_ += active_arcs_.size();
    Token scratch[kMaxTokensPerArc];
    ArcExpandOptions opts (kMaxCost, kMaxCost, kMaxCost, kMaxCost, 
        0, scratch, search_opts_, lattice_);

    for (int i = begin; i != end; ++i) {
      SearchArc* search_arc = active_arcs_[i];
      if (!search_arc) {
        continue;
      }

      //Expansion returns the cost of the best
      //scoring token in the arc
      pair<float, float> arc_cost_lookahead = 
        search_arc->Expand(trans_model_, &opts);
      float arc_cost = arc_cost_lookahead.first;

      if (arc_cost >= threshold_) {
        search_arc->Deactivate(&active_arcs_, time_);
        active_arcs_[i] = 0;
        ++num_arcs_pruned_;
        ++total_num_arcs_pruned_;
        continue;
      }


      if (search_opts_.acoustic_lookahead > kDelta) {
        float lookahead_cost =  arc_cost_lookahead.second;
        if (lookahead_cost > lookahead_threshold) {
          search_arc->Deactivate(&active_arcs_, time_);
          active_arcs_[i] = 0;
          ++num_arcs_pruned_;
          ++total_num_arcs_lookahead_pruned_;
          continue;
        }

        if (lookahead_cost < best_lookahead_cost) {
          best_lookahead_cost = lookahead_cost;
          lookahead_threshold = best_lookahead_cost + search_opts_.beam;
          opts.lbest_ = best_lookahead_cost;
          opts.lthreshold_ = lookahead_threshold;
        }
      }

      //Move the best arc to the front of the list
      //TODO how close is the best arc in the next round
      //TODO what do the arc best cost trajectories look like during decoding
      if (arc_cost < best_arc_cost) {
        best_arc_cost = arc_cost;
        threshold_ = best_arc_cost + search_opts_.beam;
        opts.threshold_ = threshold_;
        best_index = i;
      }
      arc_costs_[i] = arc_cost;
      worst_arc_cost = max(worst_arc_cost, arc_cost);
    }
    return ArcExpandResults(best_arc_cost, worst_arc_cost, best_index);
  }

  void ExpandActiveArcs_BandPruning() {
    PROFILE_FUNC();
    if (arc_costs_.size() > search_opts_.band) {
      float band = ComputeBand();
      threshold_ = band;
    }
  }

  void ExpandActiveArcs_ListCompaction() {
    PROFILE_FUNC();
    int j = 0;
    for (int i = 0; i != active_arcs_.size(); ++i) {
      if (!active_arcs_[i]) continue;
      SearchArc* arc = active_arcs_[i];
      if (arc->GetBestCost() <= threshold_) {
        active_arcs_[i] = 0;
        active_arcs_[j++] = arc;
        float exit_cost = arc->GetExitCost();
        if (exit_cost < kMaxCost) {
          ++num_exit_tokens_;
          if (exit_cost < threshold_) {
            //Return a pointer the next state and the cost arriving via
            //active_arcs[i] not the best cost of the state
            pair<SearchState*, float> statecost = ExpandToFollowingState(arc, 
                threshold_);
            //Some lookahead or re-scoring might actually give us a even better
            //threshold than the band pruning
            if (statecost.second + search_opts_.beam < threshold_) {
              threshold_ = statecost.second + search_opts_.beam;
              //Could use a branchless min once we know
              //this actually helps out
            }
          } else {
            ++num_exit_tokens_pruned_;
          }
        }
      } else {
        ++total_num_arcs_hisogram_pruned_;
        active_arcs_[i]->Deactivate(&active_arcs_, time_);
        active_arcs_[i] = 0;
      }
    }
    //TODO check j to active_arcs_.size() - 1 are all nulled out
    active_arcs_.resize(j);
    num_active_arcs_after_prune_ = active_arcs_.size();
  }

  void ExpandActiveArcs() { 
    PROFILE_FUNC();
    num_arcs_pruned_ = 0;
    best_arc_cost_ = kMaxCost;
    worst_arc_cost_ = -kMaxCost;
    threshold_ = kMaxCost;
    arc_costs_.resize(active_arcs_.size());
    max_active_arcs_ = max(max_active_arcs_, int(active_arcs_.size()));
    ArcExpandResults best_worst_arcs
      = ExpandActiveArcs_ArcExpansion(0, active_arcs_.size());
    best_arc_cost_ = best_worst_arcs.first;
    worst_arc_cost_ = best_worst_arcs.second;
    SwapArcs(0, best_worst_arcs.third);
    ExpandActiveArcs_BandPruning();
    ExpandActiveArcs_ListCompaction();
    arc_costs_.clear();
    num_active_arcs_ = active_arcs_.size();
    num_unique_ilabels_ = 0;
  }

  //Returns a pair, pointer to the destination search state
  //and the cost of the arc arriving in the state, may be bigger than
  //SearchState's cost in lattice generation mode but lower(or higher) 
  //than the arc's exit cost in on-the-fly rescoring mode of if some other
  //heuristic is added returns null, kMaxCost if no state was activated
  pair<SearchState*, float> ExpandToFollowingState(SearchArc* arc,
      float threshold) {
    PROFILE_FUNC();
    /* Token token = arc->GetExitToken();
       Trying to enforce a minimum duration doesn't seem to help
       int duration =  time_ - token.LatticeState()->Time();
       if (duration && (time_ - token.LatticeState()->Time() < 5) 
       && arc->NumStates() == 3) {
       return pair<SearchState*, float>(0, kMaxCost);
       }
    */
    SearchState* ss = arc->FindNextState(this);
    pair<float, float> costs = ss->SetToken(*arc, time_, threshold, 
        &active_states_, lattice_, search_opts_);
    return pair<SearchState*, float>(ss, costs.first);
  }

  void ExpandActiveStates() {
    PROFILE_FUNC();
    float threshold = best_state_cost_ + search_opts_.beam;
    max_active_states_ = max(max_active_states_, int(active_states_.size()));
    for (int i = 0; i != active_states_.size(); ++i ) {
      SearchState* ss = active_states_[i];
      if (ss->NumEmitting()) {
        if (ss->Cost() > threshold) {
          ++total_num_states_pruned_;
        } else {
          ++total_num_states_expanded_;
          float cost = ss->ExpandIntoArcs(&active_arcs_, threshold, time_, 
              search_opts_);
          //Update the pruning threshold
          threshold = min(threshold, cost + search_opts_.beam);
        }
      }
      ss->Deactivate(&active_states_);
    }
    active_states_.clear();
  }

  //Expand the epsilon like transitions using  a generic single
  //source shortest path algorithm. Heap for Dijkstra, queue for
  //Bellman-Fords or stack for runtime disaster
  //Optionally control the maximum number of pops from the queue
  //TODO: try implement some sort of epsilon closure cache for 
  //frequently traversed states. Results show that the 
  //TODO: could some sort of epsilon synchronization help here?

  void ExpandEpsilonArcs(int max_cycles = std::numeric_limits<int>::max()) {
    PROFILE_FUNC();
    //TODO: Use the std::pair lexicographic ordering to implement a heap with 
    //decrease key
    //set<SearchState*> q;
    EpsQueue q;
    float best = kMaxCost;
    float threshold = kMaxCost;
    float worst = kMinCost;
    int num_before = active_states_.size();
    //Add all states with outgoing epsilons to the queue
    for (int i = 0; i != active_states_.size(); ++i) {
      SearchState* ss = active_states_[i];
      best = min(best, ss->Cost());
      if (ss->NumEpsilons()) {
        //logger_(DEBUG) << "Adding state " <<  ss->StateId();
        assert(!ss->InEpsQueue());
        ss->AddToEpsQueue();
        q.push_back(ss);
      }
    }
    //Generic SSSP algorithm, however care needs to be taken here due 
    //to the presence of negative weights

    num_active_states_ = active_states_.size();
    num_epsilon_cycles_ = 0;
    while (!q.empty()) {
      SearchState* ss = q.front();
      q.pop_front();
      assert(ss->InEpsQueue());
      ss->RemoveFromEpsQueue();
      if (ss->Cost() <  threshold) { 
        search_stats_.EpsilonExpanded(ss->StateId());
        float f = ss->ExpandEpsilonArcs(&active_states_, &q, this,
            search_opts_.prune_eps ? best + search_opts_.beam : kMaxCost, 
            search_opts_);
        if (f < best) {
          best = f;
          threshold = best + search_opts_.beam;
        }
        ++num_epsilon_cycles_;
      } else {
        ++total_num_states_pruned_;
      }
    }
    num_epsilons_activated_ = active_states_.size() - num_before;
    best_state_cost_ = best;
    worst_state_cost_ = worst;
    total_num_epsilson_states_relaxed_ += num_epsilon_cycles_;
  }

  void ExpandEpsilonArcs2(int max_cycles = std::numeric_limits<int>::max()) {
    PROFILE_FUNC();
    //TODO: Use the std::pair lexicographic ordering to implement a heap with 
    //decrease key
    //set<SearchState*> q;
    EpsQueue q;
    float best = kMaxCost;
    float threshold = kMaxCost;
    float worst = kMinCost;
    int num_before = active_states_.size();
    //Add all states with outgoing epsilons to the queue
    for (int i = 0; i != active_states_.size(); ++i) {
      SearchState* ss = active_states_[i];
      best = min(best, ss->Cost());
      if (ss->NumEpsilons()) {
        //logger_(DEBUG) << "Adding state " <<  ss->StateId();
        assert(!ss->InEpsQueue());
        ss->AddToEpsQueue();
        q.push_back(ss);
      }
    }
    //Generic SSSP algorithm, however care needs to be taken here due 
    //to the presence of negative weights

    num_active_states_ = active_states_.size();
    num_epsilon_cycles_ = 0;
    while (!q.empty()) {
      SearchState* ss = q.front();
      q.pop_front();
      assert(ss->InEpsQueue());
      ss->RemoveFromEpsQueue();
      if (ss->Cost() <  threshold) { 
        search_stats_.EpsilonExpanded(ss->StateId());
        float f = ss->ExpandEpsilonArcs(&active_states_, &q, this,
            search_opts_.prune_eps ? best + search_opts_.beam : kMaxCost, 
            search_opts_);
        if (f < best) {
          best = f;
          threshold = best + search_opts_.beam;
        }
        ++num_epsilon_cycles_;
      } else {
        ++total_num_states_pruned_;
      }
    }
    num_epsilons_activated_ = active_states_.size() - num_before;
    best_state_cost_ = best;
    worst_state_cost_ = worst;
    total_num_epsilson_states_relaxed_ += num_epsilon_cycles_;
  }

  void DecodeFrame() {
    ExpandEpsilonArcs();
    ExpandActiveStates();
    ExpandActiveArcs();
  }

  void ClearSearch() {
    active_states_.clear();
    active_arcs_.clear();
    for (typename SearchHash::iterator it = search_hash_.begin(); 
        it != search_hash_.end(); ++it) {
      FreeSearchState(it->second);
    }
    search_hash_.clear();
  }

  void Clear() {
    lattice_->Clear();
    ClearSearchStats();
  }


  void FreeSearchState(SearchState* searchstate) {
    assert(searchstate);
    if (search_opts_.use_search_pool)
      search_state_pool_.push_back(searchstate);
    else
      delete searchstate;
    ++num_search_state_frees_;
  }

  SearchState* AllocSearchState() {
    ++num_search_state_allocs_;
    if (search_state_pool_.empty())
      return new SearchState;
    SearchState* ss = search_state_pool_.back();
    search_state_pool_.pop_back();
    return ss;
  }

  void ClearSearchPool() {
    for (typename vector<SearchState*>::iterator it 
        = search_state_pool_.begin(); it != search_state_pool_.end(); ++it)
      delete *it;
    search_state_pool_.clear();
  }

  //If each state is pair based on the state in the WFST
  //and some key e.g. word history arriving in the state
  //then we can easily generate full lattices be creating
  //multiple word conditioned search states
  virtual SearchState* FindSearchState(int state) {
    PROFILE_FUNC();
    ++num_search_state_requests_;
    if (search_hash_.find(state) == search_hash_.end()) {
      ++num_search_state_misses_;
      //Todo possible create a memory pool to store the states
      SearchState* ss = AllocSearchState();
      ss->Init(*fst_, state, *trans_model_, search_opts_);
      search_hash_[state] = ss;
      return ss;
    } else {
      ++num_search_state_hits_;
    }
    return search_hash_[state];
  }

  static const string &Type() {
    static string type = TransModel::Type() + "_" +  L::Type();
    return type;
  }

private:
  FST* fst_;
  TransModel* trans_model_;
  ActiveArcVector active_arcs_;
  ActiveStateVector active_states_;
  mutable SearchHash search_hash_;
  const SearchOptions &search_opts_;
  L *lattice_;
  bool owns_lattice_;
  Logger logger_;
  int time_;
  bool debug_;
  vector<float> arc_costs_;   
  vector<SearchState*> search_state_pool_;
  float threshold_;
  float best_arc_cost_; //Best arc after token expansion
  float worst_arc_cost_; //Worst surviing arc after token expansion and 
  //pruning and on-the-fly rescoring

  //Arc and state usage
  int num_active_arcs_;
  int num_active_arcs_after_prune_;
  int num_unique_ilabels_;
  int num_arcs_pruned_;
  int num_active_states_;
  float best_state_cost_; //Best state cost after epsilon expansion
  float worst_state_cost_; //Worst state cost after epsilon expansion
  int num_epsilons_activated_;
  int num_epsilon_cycles_;
  int num_states_pruned_;

  //Tokens exiting an arc
  int num_exit_tokens_;
  int num_exit_tokens_pruned_;

  //Search state creation and destruction
  int num_search_state_allocs_;
  int num_search_state_frees_;
  int num_search_state_requests_;
  int num_search_state_hits_;
  int num_search_state_misses_;

  typedef long long int bigint;
  int total_num_arc_expanded_;
  int total_num_arcs_pruned_;
  int total_num_arcs_lookahead_pruned_;
  int total_num_arcs_hisogram_pruned_;

  int total_num_states_expanded_;
  int total_num_states_pruned_;
  int total_num_epsilson_states_pruned_;
  int total_num_epsilson_states_relaxed_;
  int max_active_arcs_;
  int max_active_states_;

  Statistics search_stats_;
  Timer timer_;
  DISALLOW_COPY_AND_ASSIGN(CLevelDecoder);
};

} //namespace dcd
#endif
