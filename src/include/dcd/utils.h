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


template<class T> 
class CountSet {
 public:
  CountSet() { } 
  
  int Insert(const T& v) {
    //If the key is not present will return the
    //default value;
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
  using namespace fst;
  typedef MutableFst<Arc> FST;
  ifstream ifs(path.c_str(), ifstream::binary);
  if (!ifs.is_open())
    return false;
  int n = 0;
  ReadType(ifs, &n);
  for (int i = 0; i != n; ++i) {
   VectorFst<Arc>* fst = VectorFst<Arc>::Read(ifs, FstReadOptions());
   for (StateIterator<FST> siter(*fst); !siter.Done(); siter.Next()) {
     for (MutableArcIterator<FST> aiter(fst, siter.Value()); !aiter.Done(); 
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

} //namespace dcd

#endif //DCD_UTILS_H__
