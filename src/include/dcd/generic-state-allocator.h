#ifndef DCD_GENERIC_STATE_ALLOCATER_H__
#define DCD_GENERIC_STATE_ALLOCATER_H__

namespace dcd {

// A simple allocator class, can be use for lattice or search states
template <class T>
struct DefaultCacheStateAllocator {

  DefaultCacheStateAllocator() 
    : num_allocated_(0) { }

  ~DefaultCacheStateAllocator() { }

  T* Allocate(StateId s) {
    ++num_allocated_;
    return new T;
  }

  void Free(T* state, int s) {
    --num_allocated_;
    delete T;
  }

  int NumAllocated() const { return num_allocated_; }

 private:
  int num_allocated_;
  DISALLOW_COPY_AND_ASSIGN(DefaultCacheStateAllocator);
};

}//namespace dcd

#endif //DCD_GENERIC_STATE_ALLOCATER_H__
