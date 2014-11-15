// util.h
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
// Author : Paul R. Dixon (info@opendcd.org)
// \file
// Various helper function and ultities

#ifndef DCD_UTILS_H__
#define DCD_UTILS_H__

#include <string>

#include <fst/extensions/far/far.h>
#include <fst/fst.h>
#include <fst/vector-fst.h>
#include <fst/verify.h>

namespace dcd {

template<class A, class B>
struct Pair {
  Pair(const Pair& other) :
    first(other.first), second(other.second) { }
  Pair() { }
  Pair(const A &a, const B &b) : first(a), second(b) { }
  A first;
  B second;
};


template<class A, class B, class C>
struct Triple {
  Triple(const Triple& other) :
    first(other.first), second(other.second), third(other.third) { }

  Triple() { }

  Triple(const A& a, const  B& b, const C& c)
    : first(a), second(b),  third(c) { }

  A first;
  B second;
  C third;
};


template<class T>
class CountSet {
 public:
  CountSet() { }

  int Insert(const T& v) {
    // If the key is not present will return the default value;
    int count = hash_map_[v] + 1;
    hash_map_[v] = count;
    return count;
  }

  void Clear() { hash_map_.clear(); }

  int NumKeys() const { return hash_map_.size(); }

  typedef typename unordered_map<T, int>::const_iterator const_iterator;

  const_iterator Begin() const { return hash_map_.begin(); }

  const_iterator End() const { return hash_map_.end(); }

 private:
  unordered_map<T, int> hash_map_;
  DISALLOW_COPY_AND_ASSIGN(CountSet);
};

/// Taken from Kaldi
/// Split a string using any of the single character delimiters.
/// If omit_empty_strings == true, the output will contain any
/// nonempty strings after splitting on any of the
/// characters in the delimiter.  If omit_empty_strings == false,
/// the output will contain n+1 strings if there are n characters
/// in the set "delim" within the input string.  In this case
/// the empty string is split to a single empty string.
void SplitStringToVector(const std::string &full, const char *delim,
                         bool omit_empty_strings,
                         std::vector<std::string> *out);

} // namespace dcd

namespace fst {
template<class Arc>
bool ReadArcTypes(const string& path, vector<fst::Fst<Arc>*>* arcs) {
  fst::FarReader<Arc>* reader = fst::FarReader<Arc>::Open(path);
  if (!reader) {
    FSTERROR() << "Failed to open arc types file : " << path;
    return false;
  }

  for (; !reader->Done(); reader->Next()) {
    const fst::Fst<Arc>& fst = reader->GetFst();
    fst::VectorFst<Arc>* arc = new fst::VectorFst<Arc>(fst);
    arcs->push_back(arc);
  }
  return true;
}

template<class Arc>
bool ReadFstArcTypes(const string& path, vector<const fst::Fst<Arc>*>* arcs,
                     float scale, bool verify) {
  typedef MutableFst<Arc> FST;
  ifstream ifs(path.c_str(), ifstream::binary);
  if (!ifs.is_open())
    return false;
  int n = 0;
  fst::ReadType(ifs, &n);
  for (int i = 0; i != n; ++i) {
  fst::VectorFst<Arc>* fst = fst::VectorFst<Arc>::Read(ifs, fst::FstReadOptions());
  for (fst::StateIterator<FST> siter(*fst); !siter.Done(); siter.Next()) {
     for (fst::MutableArcIterator<FST> aiter(fst, siter.Value()); !aiter.Done();
         aiter.Next()) {
       Arc arc = aiter.Value();
       arc.weight = arc.weight.Value() *  scale;
       aiter.SetValue(arc);
     }
  }
  if (!fst) {
     FSTERROR() << "ReadArcTypes : Failed to read arctype " << i;
     return false;
  }
  if (verify && !Verify(*fst))
    FSTERROR() << "Invalid FST in arcs files at index " << i;
  arcs->push_back(fst);
  }
  return true;
}
} // namespace fst
#endif // DCD_UTILS_H__
