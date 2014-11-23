// stl.h
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
// Helper classes for wrapping different STL implementations.

#ifndef DCD_STL_H__
#define DCD_STL_H__

#if __cplusplus==199711L
  #include <tr1/unordered_map>
  #include <tr1/unordered_set>
  using std::tr1::unordered_map;
  using std::tr1::unordered_set;
#else
  #include <unordered_map>
  #include <unordered_set>
  using std::unordered_map;
  using std::unordered_set;
#endif

#ifdef HAVE_RDESTL
// Optionally use the RDE hash_map and vector
// Perhaps add EASTL or Google's Sparsehash These maybe be faster and allow for
// better memory control

#include <rdestl/hash_map.h>
#include <rdestl/vector.h>

namespace dcd {
template<class T>
struct VectorHelper {
  typedef rde::vector<T> Vector;
};

template<class K, class V>
struct HashMapHelper {
  typedef rde::hash_map<K, V> HashMap;
};

template<class T>
void ClearVector(rde::vector<T> *vec) {
  vec->clear();
}
}  // namespace dcd
#else
#include <vector>
namespace dcd {

template<class T>
struct VectorHelper {
  typedef std::vector<T> Vector;
};

template<class T>
void ClearVector(std::vector<T> *vec) {
  std::vector<T> decoy;
  vec->swap(decoy);
}

template<class K, class V>
struct HashMapHelper {
  typedef unordered_map<K, V> HashMap;
};

template<class T, int N>
struct SmallVectorHelper {
  typedef std::vector<T> SmallVector;
};

/*
template<class T, int N>
struct TokenVectorHelper {
  typedef std::array<T, N> TokenVector;
};
*/
}  // namespace dcd
#endif
#endif  // DCD_STL_H__
