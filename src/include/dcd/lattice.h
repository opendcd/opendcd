#ifndef DCD_LATTICE_H__
#define DCD_LATTICE_H__

#include <fst/vector-fst.h>
#include <dcd/lattice-arc.h>
#include <dcd/log.h>
#include <dcd/search-opts.h>
#include <dcd/search-statistics.h>
#include <dcd/stl.h>
#include <dcd/constants.h>

using fst::VectorFst;

namespace dcd {
class Lattice {
 public:
  struct LatticeState;
  
  struct LatticeArc {
    
    LatticeArc() { Clear(); }

    LatticeArc(const LatticeArc& other)
      : prevstate_(other.prevstate_), ilabel_(other.ilabel_), 
      olabel_(other.olabel_), dur_(other.dur_), am_weight_(other.am_weight_), 
      lm_weight_(other.lm_weight_), dur_weight_(other.dur_weight_) { }

    LatticeArc(LatticeState* prevstate, int ilabel, int olabel, float am_weight,
        float lm_weight, float dur_weight)
      : prevstate_(prevstate), ilabel_(ilabel), olabel_(olabel), 
      am_weight_(am_weight), lm_weight_(lm_weight), 
      dur_weight_(dur_weight) { }

    LatticeState* prevstate_;
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

    LatticeState* PrevState() const { return prevstate_; }

    int ILabel() const { return ilabel_; }

    int OLabel() const { return olabel_; }

    //We need function for each weight type to convert from 
    //the LatticeArc to the OpenFst Arc type 
    //This is done using overloads

    void ConvertWeight(StdArc::Weight* w) const {
      *w = am_weight_ + lm_weight_;
    }

    void ConvertWeight(KaldiLatticeWeight* w) const {
      w->SetValue1(am_weight_);
      w->SetValue2(lm_weight_);
    }
  };

  struct LatticeState {
   public:
    void Init(int time, int state, int id, int index,
        float forwards_cost = kMaxCost) {
      Clear();
      time_ = time;
      state_ = state;
      id_ = id;
      index_ = index;
      forwards_cost_ = forwards_cost;
    }

    //Create an OpenFst version of the best sequence from this state
    template<class Arc>
    int GetBestSequence(VectorFst<Arc>* ofst) const {
      //PROFILE_FUNC();
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

    LatticeState* PrevState() const {
      return best_arc_.prevstate_;
    }

    int PrevStateId() const {
      return  best_arc_.prevstate_ ? best_arc_.prevstate_->Id() : -1;
    }


    int PrevStateIndex() const {
      return  best_arc_.prevstate_ ? best_arc_.prevstate_->index_ : -1;
    }
    //Add a new arc to lattice state
    template<class T>
    pair<float, float> AddArc(LatticeState* src, float cost,  const T& arc,
        float threshold, const SearchOptions& opts) {
      //TODO add rescoring here

      //Total LM/AM/Trn cost accumulated in the arc
      float arc_cost = cost - src->ForwardsCost();
      float am_cost = arc_cost - arc.Weight();
      LatticeArc lattice_arc(src, arc.ILabel(), arc.OLabel(), am_cost,
          arc.Weight(), 0.0f);
      if (cost < forwards_cost_) {
        //New best token arriving
        best_arc_ = lattice_arc;
        forwards_cost_ = cost;
      }

      //Generating a lattice
      //Here we use a potentially tigher beam
      if (opts.gen_lattice && best_arc_.prevstate_) {
        float lat_threshold = forwards_cost_ + opts.lattice_beam;
        if (cost < lat_threshold) {
          //TODO: check the another arcs in the isn't the same with 
          //just a different cost
          arcs_.push_back(lattice_arc);
        }
      }
      return pair<float, float>(cost, forwards_cost_);
    }

    //Here we assume the start state has a null best_previous_ pointer
    bool IsStart() const { return !best_arc_.prevstate_; }

    void GcClearMark() { marked_ = false; } 

    bool GcMarked() const { return marked_; }

    void GcMark() {
      if (marked_) {
        ++marked_;
        return;
      }
      if (best_arc_.prevstate_)
        best_arc_.prevstate_->GcMark(); 
      for (int i = 0; i != arcs_.size(); ++i) {
        LatticeState* prev = arcs_[i].prevstate_;
        prev->GcMark();
      }
      marked_ = true;
    }

    void Clear() {
      state_ = kNoStateId;
      time_ = -1;
      id_ = -1;
      marked_ = false;
      index_ = -1;
      forwards_cost_ = kMaxCost;
      backwards_cost_ = kMaxCost;
      best_arc_.Clear();
      if (arcs_.size())
        ClearVector(&arcs_);
    }

    void Check() {
    }

    int Index() const { return index_; }

    void SetIndex(int index) { index_ = index; }

    float ForwardsCost() const { return forwards_cost_; }

    float BackwardsCost() const { return backwards_cost_; }

    int Id() const { return id_; }

    int OLabel() const { return best_arc_.OLabel(); }

    int Time() const { return time_; }

   protected:
    int state_; //the state in the search fst
    int time_; //Time the lattice state was created
    int id_; //Unique id assigned to this state
    int marked_; //Integer useful for storing other values, e.g. refs count
    //GC marks, id for value into VectorFsts when generating
    int index_; //Where the lattice state is in the vector of lattice states;
    float forwards_cost_;
    float backwards_cost_;

    LatticeArc best_arc_; //The best lattice arc arriving in this state

    typedef  VectorHelper<LatticeArc>::Vector LatticeArcVector;
    LatticeArcVector arcs_;  //lattice arcs within the lattice
    //beam arriving in this state
    friend class Lattice;
  };


  explicit Lattice(const SearchOptions& opts, ostream* logstream = &std::cerr) 
    : logger_("Lattice", *logstream),
    next_id_(0), num_allocs_(0), num_frees_(0), 
    use_pool_(opts.use_lattice_pool) { }

  virtual ~Lattice() { 
    Clear();
    for (int i = 0; i != free_list_.size(); ++i)
      delete free_list_[i];
    free_list_.clear();
  }

  void Check() {
    unordered_set<LatticeState*> lss;
    for (int i = 0; i < used_list_.size(); ++i) {
      LatticeState* ls = used_list_[i];
      lss.insert(ls);
      if (ls->Index() != i) 
        LOG(FATAL) << "Lattice::Check : Index problem in lattice ls->Index() "
          << ls->Index() << " i " << i;
    }

    unordered_set<LatticeState*> fss;
    for (int i = 0; i < free_list_.size(); ++i) {
      LatticeState* ls = free_list_[i];
      fss.insert(ls);
    }

    for (int i = 0; i < used_list_.size(); ++i) {
      const LatticeState& ls = *used_list_[i];
      const LatticeState::LatticeArcVector& arcs = ls.arcs_;
      for (int j = 0; j != arcs.size(); ++j) {
        const LatticeArc& arc = arcs[j];
        if (lss.find(arc.prevstate_) == lss.end()) {
          if (fss.find(arc.prevstate_) == fss.end()) {
            LOG(ERROR) << "Lattice::Check : bad arc back pointer int lattice " <<
              arc.prevstate_->Index();
          } else {
            LOG(ERROR) << "Lattice::Check : arc back pointer into freelist ";
          }
        }
      }
    }

    for (int i = 0; i != free_list_.size(); ++i ) {
      LatticeState* lattice_state = free_list_[i];
      lattice_state->Check();
    }
  }

  LatticeState* CreateStartState(int state) {
    LatticeState* ls =  NewLatticeState(-1, state);
    ls->forwards_cost_ = 0.0f;
    return ls;
  }

  void DumpInfo() {
    if (FLAGS_v > 2)
      logger_(DEBUG) << "Lattice Info:" << endl
        << "\tNext Id " << next_id_ << endl
        << "\t# of allocs " << num_allocs_ << endl
        << "\t# of frees " << num_frees_ << endl
        << "\t# in used list " << used_list_.size() << endl
        << "\t# in free list " << free_list_.size();
  }

  LatticeState* NewLatticeState(int time, int state) { 
    PROFILE_FUNC();
    LatticeState* lattice_state = 0;
    if (use_pool_ && free_list_.size()) {
      lattice_state = free_list_.back();
      free_list_.pop_back();
    } else { 
      lattice_state = new LatticeState;
    }
    lattice_state->Init(time, state, next_id_++, used_list_.size());       
    used_list_.push_back(lattice_state);
    ++num_allocs_;
    return lattice_state;
  }

  void FreeLatticeState(LatticeState* lattice_state) { 
    PROFILE_FUNC();
    if (use_pool_)
      free_list_.push_back(lattice_state);
    else
      delete lattice_state;
    ++num_frees_;
  }

  LatticeState* AddState(int time, int state) {
    return NewLatticeState(time, state);
  }

  void DeleteState(LatticeState* ls) {
    if (ls->Index())
      used_list_[ls->Index()] = 0;
    delete ls;
  }

  //First field is the best cost arriving in the lattice state (forward cost)
  //Second field is the the cost of the SearchArc arrvining in the lattice state
  template<class SearchArc>
  pair<float, float> AddArc(LatticeState* src, LatticeState* dest, float cost, 
      const SearchArc& arc, float threshold, 
      const SearchOptions & opts) {
    //Add the lattice arc in the reverse direction
    return dest->AddArc(src, cost, arc, threshold, opts);
  }

  int NumStates() const { return used_list_.size(); }

  int FreeListSize() const { return free_list_.size(); }

  void Clear() {
    next_id_ = 0;
    GcClearMarks();
    GcSweep();
    DumpInfo();
    if (num_frees_ != num_allocs_)
      LOG(FATAL) << "Lattice state allocator mismatch detected " << endl 
        << " # Allocs " << num_allocs_ << endl
        << " # Frees " << num_frees_;
    num_frees_ = 0;
    num_allocs_ = 0;
  }

  //Go through all the lattice states in the used_list_ and
  //unmark them. In the next phase we mark the reachable states
  void GcClearMarks() {
    PROFILE_FUNC();
    for (int i = 0; i != used_list_.size(); ++i)
      used_list_[i]->GcClearMark();
  }

  //Free all the lattice states that are not used
  int GcSweep(vector<int>* early_mission = 0) {
    PROFILE_FUNC();
    int j = 0;
    for (int i = 0; i != used_list_.size(); ++i) {
      LatticeState* lattice_state = used_list_[i];
      if (used_list_[i]->GcMarked()) {
        //Check j is null or the same as i
        lattice_state->SetIndex(j);
        used_list_[j++] = lattice_state;
      } else { 
        lattice_state->SetIndex(-1);
        FreeLatticeState(used_list_[i]);
        used_list_[i] = 0;
      }
    }

    //Sanity check to make sure all the end of the used_list is all
    //null probably can be removed at the end
    for (int i = 0; i < j; ++i) {
      assert(used_list_[i]);
      assert(used_list_[i]->Index() == i);
    }
    //Resize down the end of the used_list
    int num_reclaimed = used_list_.size() - j;
    used_list_.resize(j);
    VLOG(1) << "Lattice Gc reclaimed " << num_reclaimed
      << " states # in used_list " 
      << used_list_.size() << " # in free_list " << free_list_.size();

    if (early_mission) {
      //Work backward from a frontier node.
      vector<LatticeState*> stack;
      for (LatticeState* ls = used_list_.back(); !ls->IsStart(); 
          ls = ls->PrevState())
        stack.push_back(ls);

      while (!stack.empty()) {
        LatticeState *ls = stack.back();
        stack.pop_back();
        if (ls->OLabel())
          early_mission->push_back(ls->OLabel());
        if (ls->marked_ > 1) {
          VLOG(2) << "Common prefix found index = " << ls->Index() 
            << " id = " << ls->Id();
          break;
        }  
      }
    }
    return num_reclaimed;
  }

  template<class Arc>
  int GetLattice(LatticeState* state, MutableFst<Arc>* ofst) {
    GcClearMarks();
    state->GcMark();
    //After GcSweep the used_list indexes should be contigous
    //therefore we don't need to do any relabelling or convert
    //pointer to indexes
    GcSweep();

    while (ofst->NumStates() < used_list_.size())
      ofst->AddState();
    int numarcs = 0;
    for (int i = 0; i != used_list_.size(); ++i) {
      const LatticeState& ls = *used_list_[i];
      if (ls.IsStart()) {
        ofst->SetStart(ls.index_);
      } else {
        for (int j = 0; j != ls.arcs_.size(); ++j) {
          const LatticeArc& arc = ls.arcs_[j];
          typename Arc::Weight w;
          arc.ConvertWeight(&w);
          int s = arc.prevstate_->Index();
          int d = ls.Index();
          ofst->AddArc(s, Arc(arc.ilabel_, arc.olabel_, w, d));
          ++numarcs;
        }
      }
    }
    ofst->SetFinal(state->Index(), Arc::Weight::One());
    return numarcs;
  }


  template<class T>
  string ToString(const T &t) {
    stringstream ss;
    ss << t;
    return ss.str();
  }

  //Debug function to build a tranducers from the partial traceback
  template<class Arc>
  void DumpToFst(VectorFst<Arc>* ofst, fst::SymbolTable* ssyms = 0) {
    typedef typename Arc::Weight W;
    while(ofst->NumStates() < used_list_.size())
      ofst->AddState();
    ofst->SetStart(0);
    for (int i = 0; i != used_list_.size(); ++i) {
      const LatticeState &state = *used_list_[i];
      if (!state.marked_)
        ofst->SetFinal(i, W::One());
      if (ssyms) {
        stringstream ss;
        ss << state.id_ << "," << state.marked_;
        ssyms->AddSymbol(ss.str(), i);
      }
      if (!i)
        continue;
      if (state.arcs_.size()) {
        for (int j = 0; j != state.arcs_.size(); ++j) {
          const LatticeArc &arc = state.arcs_[j];
          W w;
          arc.ConvertWeight(&w);
          ofst->AddArc(arc.PrevState()->Index(), 
              StdArc(arc.ilabel_, arc.olabel_, w, i)); 
        }
      } else {
        const LatticeArc &arc = used_list_[i]->best_arc_;
        W w;
        arc.ConvertWeight(&w);
        ofst->AddArc(used_list_[i]->PrevStateIndex(), StdArc(arc.ilabel_, 
              arc.olabel_, w, i));
      }
    }
  }

  void GetUsageStatistics(UtteranceSearchStatstics *search_statistics) {
  }

  static const string& Type() {
    static string type = "GenericLattice";
    return type;
  }
  //Debugging and check functions
  void SortLists() {
    if (!use_pool_)
      LOG(FATAL) << "Lattice allocator pooling must be enable "
        "to check the lattice consistency";
    std::sort(used_list_.begin(), used_list_.end());
    std::sort(free_list_.begin(), free_list_.end());
  }

  bool IsActive(LatticeState* state) const {
    return std::binary_search(used_list_.begin(), used_list_.end(), state);
  }

  bool IsFree(LatticeState* state) const { 
    return std::binary_search(free_list_.begin(), free_list_.end(), state);
  }

 protected:
  vector<LatticeState*> used_list_;
  vector<LatticeState*> free_list_;
  Logger logger_;
  int next_id_;
  //Sanity check, num_allocs_ should equal num_frees_ after decoding
  int num_allocs_;
  int num_frees_;
  bool use_pool_;
 private:
  DISALLOW_COPY_AND_ASSIGN(Lattice);
};
} //namespace dcd

#endif //DCD_LATTICE_H__
