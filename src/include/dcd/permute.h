// permute.h

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
// Copyright 2014 Paul R.  Dixon
// Author: paul.r.dixon@gmail.com
//
// \file
// Class to pemute a linearFst.

#ifndef FSTX_PERMUTE_H__
#define FSTX_PERMUTE_H__

#include <algorithm>
#include <string>
#include <vector>
using std::vector;

#include <fst/fst.h>
#include <fst/test-properties.h>


namespace fst {

template <class A> class PermuteFst;

template <class A>
class PermuteFstImpl : public FstImpl<A> {
 public:
  using FstImpl<A>::SetType;
  using FstImpl<A>::SetProperties;
  using FstImpl<A>::SetInputSymbols;
  using FstImpl<A>::SetOutputSymbols;

  friend class StateIterator< PermuteFst<A> >;
  friend class ArcIterator< PermuteFst<A> >;

  typedef A Arc;
  typedef typename A::Label Label;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;

  explicit PermuteFstImpl(const Fst<A> &fst) {
    StateIterator<Fst<A> > siter(fst); 
    for (;!siter.Done(); siter.Next()) {
      StateId s = siter.Value();
      if (fst.NumArcs() > 1)
        LOG(FATAL) << "Fst must be a linear chain";
      ArcIterator<Fst<A> > aiter(fst, s);
      auto arc = aiter.Value();
      arcs_.push_back(arc);
    }
  }

  StateId Start() const {
    return 0;
  }

  Weight Final(StateId s) const {
    return Weight::One();
  }

  size_t NumArcs(StateId s) const {
    return 1;
  }

  size_t NumInputEpsilons(StateId s) const {
    return 0;
  }

  size_t NumOutputEpsilons(StateId s) const {
    return 0;
  }

  uint64 Properties() const { return Properties(kFstProperties); }

  uint64 Properties(uint64 mask) const {
    if ((mask & kError) && fst_->Properties(kError, false))
      SetProperties(kError, kError);
    return FstImpl<Arc>::Properties(mask);
  }


 private:
  vector<A> arcs_;

  void operator=(const PermuteFstImpl<A> &fst);  // Disallow

};


template <class A>
class PermuteFst : public ImplToFst< PermuteFstImpl<A> > {
 public:
  friend class StateIterator< PermuteFst<A> >;
  friend class ArcIterator< PermuteFst<A> >;

  using ImplToFst< PermuteFstImpl<A> >::GetImpl;

  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef PermuteFstImpl<A> Impl;

  explicit PermuteFst(const Fst<A> &fst)
      : ImplToFst<Impl>(new Impl(fst)) {
    uint64 props = kUnweighted | kNoEpsilons | kIDeterministic | kAcceptor;
    if (fst.Properties(props, true) != props) {
      FSTERROR() << "PermuteFst: argument not an unweighted "
                 << "epsilon-free deterministic acceptor";
      GetImpl()->SetProperties(kError, kError);
    }
  }

  // See Fst<>::Copy() for doc.
  PermuteFst(const PermuteFst<A> &fst, bool safe = false)
      : ImplToFst<Impl>(fst, safe) {}

  // Get a copy of this PermuteFst. See Fst<>::Copy() for further doc.
  virtual PermuteFst<A> *Copy(bool safe = false) const {
    return new PermuteFst<A>(*this, safe);
  }

  virtual inline void InitStateIterator(StateIteratorData<A> *data) const;

  virtual inline void InitArcIterator(StateId s,
                                      ArcIteratorData<A> *data) const;

  // Label that represents the rho transition.
  // We use a negative value, which is thus private to the library and
  // which will preserve FST label sort order.
  static const Label kRhoLabel = -2;
 private:
  // Makes visible to friends.
  Impl *GetImpl() const { return ImplToFst<Impl>::GetImpl(); }

  void operator=(const PermuteFst<A> &fst);  // disallow
};

template <class A> const typename A::Label PermuteFst<A>::kRhoLabel;


// Specialization for PermuteFst.
template <class A>
class StateIterator< PermuteFst<A> > : public StateIteratorBase<A> {
 public:
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;

  explicit StateIterator(StateId nstates)
      : nstates_(nstates), s_(0) {
  }

  bool Done() const { return s_ == nstates_; }

  StateId Value() const { return s_; }

  void Next() {
    ++s_;
  }

  void Reset() {
    s_ = 0;
  }

 private:
  // This allows base class virtual access to non-virtual derived-
  // class members of the same name. It makes the derived class more
  // efficient to use but unsafe to further derive.
  virtual bool Done_() const { return Done(); }
  virtual StateId Value_() const { return Value(); }
  virtual void Next_() { Next(); }
  virtual void Reset_() { Reset(); }

  StateId s_;
  StateId nstates_;
  DISALLOW_COPY_AND_ASSIGN(StateIterator);
};


// Specialization for PermuteFst.
template <class A>
class ArcIterator< PermuteFst<A> > : public ArcIteratorBase<A> {
 public:
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef typename A::Weight Weight;

  ArcIterator(const vector<A> &arc, StateId s)
      : aiter_(0), s_(s), pos_(0) {
  }

  virtual ~ArcIterator() { }

  bool Done() const {
  }

  const A& Value() const {
    return arc_;
  }

  void Next() {
    ++pos_;
  }

  size_t Position() const {
    return pos_;
  }

  void Reset() {
    pos_ = 0;
  }

  void Seek(size_t a) {
    pos_ = a;
  }

  uint32 Flags() const {
    return kArcValueFlags;
  }

  void SetFlags(uint32 f, uint32 m) {}

 private:
  // This allows base class virtual access to non-virtual derived-
  // class members of the same name. It makes the derived class more
  // efficient to use but unsafe to further derive.
  virtual bool Done_() const { return Done(); }
  virtual const A& Value_() const { return Value(); }
  virtual void Next_() { Next(); }
  virtual size_t Position_() const { return Position(); }
  virtual void Reset_() { Reset(); }
  virtual void Seek_(size_t a) { Seek(a); }
  uint32 Flags_() const { return Flags(); }
  void SetFlags_(uint32 f, uint32 m) { SetFlags(f, m); }

  const vector<A> arcs_;
  StateId s_;
  size_t pos_;
  mutable A arc_;
  DISALLOW_COPY_AND_ASSIGN(ArcIterator);
};


template <class A> inline void
PermuteFst<A>::InitStateIterator(StateIteratorData<A> *data) const {
  data->base = new StateIterator< PermuteFst<A> >(*this);
}

template <class A> inline void
PermuteFst<A>::InitArcIterator(StateId s, ArcIteratorData<A> *data) const {
  data->base = new ArcIterator< PermuteFst<A> >(*this, s);
}

// Useful alias when using StdArc.
typedef PermuteFst<StdArc> StdPermuteFst;
}


#endif  // FSTX_PERMUTE_H__
FSTX_PERMUTE_H___LIB_COMPLEMENT_H__
