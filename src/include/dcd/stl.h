#ifndef DCD_STL_H__
#define DCD_STL_H__

#ifdef USERDESTL 
//Optionally use the RDE hash_map 
//Perhaps add EASTL. These should be faster and allow for
//better memory tweaking
//
#include "../../../3rdparty/rdestl/vector.h"
#include "../../../3rdparty/rdestl/hash_map.h"

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
}

#else
#include <vector>
#include <unordered_map>
#include <array>
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
  typedef std::unordered_map<K, V> HashMap;
};

template<class T, int N>
struct SmallVectorHelper {
  typedef std::vector<T> SmallVector;
};

template<class T, int N>
struct TokenVectorHelper {
  typedef std::array<T, N> TokenVector;
};
}
#endif
#endif 
