// arc-expand-fst.h
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
// arc-expand-fst.h

// Implementation of delayed ArcExpandFst.

#ifndef FST_LIB_ARCEXPAND_H___
#define FST_LIB_ARCEXPAND_H__

#include <vector>

#include <fst/cache.h>
#include <fst/log.h>

namespace fst {

struct ArcExpandFstOptions : public CacheOptions {
};

template <class A>
class ArcExpandFstImpl : public CacheImpl<A> {
 public:
  using FstImpl<A>::SetType;
  using FstImpl<A>::SetProperties;
  using FstImpl<A>::SetInputSymbols;
  using FstImpl<A>::SetOutputSymbols;

  using CacheBaseImpl< CacheState<A> >::PushArc;
  using CacheBaseImpl< CacheState<A> >::HasArcs;
  using CacheBaseImpl< CacheState<A> >::HasFinal;
  using CacheBaseImpl< CacheState<A> >::HasStart;
  using CacheBaseImpl< CacheState<A> >::SetArcs;
  using CacheBaseImpl< CacheState<A> >::SetFinal;
  using CacheBaseImpl< CacheState<A> >::SetStart;


  typedef A Arc;
  typedef Fst<A> FST;
  typedef typename A::Label Label;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;
  typedef CacheState<A> State;

  static const size_t kPrime0 = 7853;
  static const size_t kPrime1 = 7867;

  struct Element {
    StateId state;
    StateId arcstate;
    int index;  // size_t might not be signed

    Element(const Element& other)
      : state(other.state), arcstate(other.arcstate), index(other.index)  { }

    Element()
      : state(kNoStateId), arcstate(kNoStateId), index(-1) { }

    explicit Element(StateId s)
      : state(s), arcstate(kNoStateId), index(-1)  { }

    Element(StateId s, StateId a, size_t i)
      : state(s), arcstate(a), index(i) { }
  };

  struct ElementKey {
    size_t operator()(const Element& e) const {
      return static_cast<size_t>(e.state + e.arcstate * kPrime0 +
          e.index * kPrime1);
    }
  };

  struct ElementEqual {
    bool operator()(const Element &e1, const Element &e2) const {
      return (e1.state == e2.state) && (e1.arcstate == e2.arcstate)
        && (e1.index == e2.index);
    }
  };

  ArcExpandFstImpl(const Fst<A>& fst, const vector<const Fst<A>*>& arcs,
    const ArcExpandFstOptions &opts)
    : CacheImpl<A>(opts), fst_(fst.Copy()), arcs_(arcs) {
      SetType("arcexpand");
      // uint64 props = fst.Properties(kFstProperties, false);
      // SetProperties(MyProperties(props, true), kCopyProperties);
      // SetProperties(kFstProperties, kCopyProperties);
      SetInputSymbols(fst.InputSymbols());
      SetOutputSymbols(fst.OutputSymbols());
      for (int i = 0; i != arcs.size(); ++i) {
        const Fst<A>& fst = *arcs[i];
        // Assume the epsilon/disambiguation has 2 states
        int numstates = CountStates(fst);
        if (numstates == 2)
          epsilons_.insert(i);
        numstates_.push_back(numstates);
      }
    }

  ArcExpandFstImpl(const ArcExpandFstImpl &impl)
    : CacheImpl<A>(impl),
    fst_(impl.fst_->Copy(true)), arcs_(impl.arcs_) {
      SetType("arcexpand");
      SetProperties(impl.Properties(), kCopyProperties);
      SetInputSymbols(impl.InputSymbols());
      SetOutputSymbols(impl.OutputSymbols());
    }

  ~ArcExpandFstImpl() {
    delete fst_;
  }

  StateId Start() {
    if (!HasStart()) {
      Element e(fst_->Start());
      StateId start = AddElement(e);
      SetStart(start);
    }
    return CacheImpl<A>::Start();
  }

  Weight Final(StateId s) {
    if (!HasFinal(s)) {
      Expand(s);
      const Element element = elements_[s];
      SetFinal(s, element.arcstate == - 1 ? fst_->Final(element.state) :
          Weight::Zero());
    }
    return CacheImpl<A>::Final(s);
  }

  size_t NumArcs(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<A>::NumArcs(s);
  }

  size_t NumInputEpsilons(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<A>::NumInputEpsilons(s);
  }

  size_t NumOutputEpsilons(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<A>::NumOutputEpsilons(s);
  }

  uint64 Properties() const { return Properties(kFstProperties); }

  // Set error if found; return FST impl properties.
  uint64 Properties(uint64 mask) const {
    if ((mask & kError) &&
      (fst_->Properties(kError, false)))
      SetProperties(kError, kError);
    return FstImpl<A>::Properties(mask);
  }

  void InitArcIterator(StateId s, ArcIteratorData<A> *data) {
    if (!HasArcs(s))
      Expand(s);
    CacheImpl<A>::InitArcIterator(s, data);
  }


  Label FindTypeLabel(const Element& e) {
    if (e.index == -1)
      return kNoLabel;
    ArcIterator<FST> aiter(*fst_, e.state);
    aiter.Seek(e.index);
    return aiter.Value().ilabel;
  }

  void Expand(StateId s) {
    if (s >= elements_.size())
      FSTERROR() << "Expand : out of bounds state access";
    const Element element = elements_[s];
    if (element.arcstate == -1) {
      // Expanding a state in the CLG and the first element is the source state
      int i = 0;
      for (ArcIterator<Fst<A> > aiter(*fst_, element.state); !aiter.Done();
        aiter.Next()) {
        const Arc& arc = aiter.Value();
        Element delement = epsilons_.find(arc.ilabel) == epsilons_.end() ?
          Element(element.state, 0, i++) :
          Element(arc.nextstate, kNoStateId, -1);
        StateId d = AddElement(delement);
        PushArc(s, Arc(0, arc.olabel, arc.weight, d));
      }
    } else {
      // Expanding a state inside a HMM arc
      // Element.state is the source state of the underlying transition
      // Element.index indexes into arcs leaving element.state
      // Element.arcstate tell us the internal state of arc
      Label ilabel = FindTypeLabel(element);
      const FST* type =  arcs_[ilabel];
      int laststate = numstates_[ilabel] - 1;
      if (laststate <= 0)
        FSTERROR() << "Expand : Last state numbering problem " << laststate;

      for (ArcIterator<FST> aiter(*type, element.arcstate); !aiter.Done();
           aiter.Next()) {
        const Arc& arc = aiter.Value();
        Element delement;
        if (arc.nextstate == laststate) {
          const Arc& fstarc = FindArc(element);
          delement.state = fstarc.nextstate;
          delement.arcstate = kNoStateId;
          delement.index = -1;
        } else {
          delement.state = element.state;
          delement.arcstate = arc.nextstate;
          delement.index = element.index;
        }
        StateId d = AddElement(delement);
        PushArc(s, Arc(arc.ilabel, 0, arc.weight, d));
      }
    }
    SetArcs(s);
  }

  StateId AddElement(const Element& element) {
    if (element.state == -1)
      FSTERROR() << "Attempting to add bad element " <<
        element.state << " " << element.arcstate << " " << element.index;
    StateId d = kNoStateId;
    typename ElementMap::iterator it = element2state_.find(element);
    if (it == element2state_.end()) {
      // State doesn't exist yet
      d = elements_.size();
      elements_.push_back(element);
      element2state_[element] = d;
    } else {
      d = it->second;
    }
    if (elements_.size() != element2state_.size())
      FSTERROR() << "ArcExpandFstImpl::AddElement : element hashing error";
    return d;
  }

  const Arc& FindArc(StateId s) {
    return FindArc(elements_[s]);
  }

  const Arc& FindArc(const Element& e)  {
    if (e.index == -1) {
      return arc_;
    } else {
      ArcIterator<FST> aiter(*fst_, e.state);
      aiter.Seek(e.index);
      return aiter.Value();
    }
  }

  StateId FstState(StateId s) {
    if (s >= elements_.size())
      FSTERROR() << "FstState : Out of bounds state access " << s;
    return elements_[s].state;
  }

  Arc FstArc(StateId s) {
    const Element& e = elements_[s];
    return e.index == -1 ? Arc() : FindArc(e);
  }

  Label ArcLabel(StateId s) {
    const Arc& arc = FindArc(s);
    return arc.ilabel;
  }

  StateId ArcState(StateId s) {
    if (s >= elements_.size())
      FSTERROR() << "FstState : Out of bounds state access " << s;
    return elements_[s].arcstate;
  }

  const Fst<A>& GetFst() const {
    return *fst_;
  }

 private:
  const Fst<A> *fst_;
  const Arc arc_;
  // Maps from a state in the expanded fst to pair corresponding
  // to the state in the CLG and a state in an arc
  vector<Element> elements_;
  typedef unordered_map<Element, StateId, ElementKey, ElementEqual> ElementMap;
  ElementMap element2state_;
  // Number of states associated with the nth arc type
  vector<int> numstates_;
  unordered_set<Label> epsilons_;
  const vector<const Fst<A>*>& arcs_;
  void operator=(const ArcExpandFstImpl<A> &);  // disallow
};

template <class A>
class ArcExpandFst : public ImplToFst< ArcExpandFstImpl<A> > {
 public:
  friend class ArcIterator< ArcExpandFst<A> >;
  friend class StateIterator< ArcExpandFst<A> >;

  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef CacheState<A> State;
  typedef ArcExpandFstImpl<A> Impl;

  ArcExpandFst(const Fst<A> &fst, const vector<const Fst<A>*>& arcs)
    : ImplToFst<Impl>(new Impl(fst, arcs, ArcExpandFstOptions())) {}

  ArcExpandFst(const Fst<A> &fst, const vector<const Fst<A>*>& arcs,
    const ArcExpandFstOptions &opts)
    : ImplToFst<Impl>(new Impl(fst, arcs, opts)) {}

  // See Fst<>::Copy() for doc.
  ArcExpandFst(const ArcExpandFst<A> &fst, bool safe = false)
    : ImplToFst<Impl>(fst, safe) {}

  // Get a copy of this ArcExpandFst. See Fst<>::Copy() for further doc.
  virtual ArcExpandFst<A> *Copy(bool safe = false) const {
    return new ArcExpandFst<A>(*this, safe);
  }

  virtual inline void InitStateIterator(StateIteratorData<A> *data) const;

  virtual void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) const {
    GetImpl()->InitArcIterator(s, data);
  }

  // Get the underlying state in the 'CLG' search network
  StateId FstState(StateId s) const {
    return GetImpl()->FstState(s);
  }

  // Add a state could correspond to a arc in the underlying network
  Arc FstArc(StateId s) const {
    return GetImpl()->FstArc(s);
  }

  StateId ArcState(StateId s) const  {
    return GetImpl()->ArcState(s);
  }

  Label ArcLabel(StateId s) const  {
    return GetImpl()->ArcLabel(s);
  }

  const Fst<Arc>& GetFst() const {
    return GetImpl()->GetFst();
  }

 private:
  // Makes visible to friends.
  Impl *GetImpl() const { return ImplToFst<Impl>::GetImpl(); }

  void operator=(const ArcExpandFst<A> &fst);  // disallow
};

// Specialization for ArcExpandFst.
template<class A>
class StateIterator< ArcExpandFst<A> >
  : public CacheStateIterator< ArcExpandFst<A> >{
 public:
  explicit StateIterator(const ArcExpandFst<A> &fst)
    : CacheStateIterator< ArcExpandFst<A> >(fst, fst.GetImpl()) {}
};


// Specialization for ArcExpandFst.
template <class A>
class ArcIterator< ArcExpandFst<A> >
  : public CacheArcIterator< ArcExpandFst<A> >{
 public:
  typedef typename A::StateId StateId;

  ArcIterator(const ArcExpandFst<A> &fst, StateId s)
    : CacheArcIterator< ArcExpandFst<A> >(fst.GetImpl(), s) {
      if (!fst.GetImpl()->HasArcs(s))
        fst.GetImpl()->Expand(s);
    }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcIterator);
};

template <class A> inline
void ArcExpandFst<A>::InitStateIterator(StateIteratorData<A> *data) const {
    data->base = new StateIterator< ArcExpandFst<A> >(*this);
}

typedef ArcExpandFst<StdArc> StdArcExpandFst;

template <class Arc>
void ArcExpand(const Fst<Arc> &ifst, const vector<const Fst<Arc>* >& arcs,
               MutableFst<Arc> *ofst) {
  ArcExpandFstOptions nopts;
  nopts.gc_limit = 0;  // Cache only the last state for fastest copy.
  *ofst = ArcExpandFst<Arc>(ifst, arcs, nopts);
}
}  // namespace fst
#endif  // FST_LIB_ARCEXPAND_H___
