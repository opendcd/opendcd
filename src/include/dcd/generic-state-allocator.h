// generic-state-allocator.h
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
// Author: Paul R. Dixon
// \file
// Simple allocator class for memory pools in the decoder
// maybe change to the new allocators in OpenFst 1.4.1

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

}  // namespace dcd

#endif // DCD_GENERIC_STATE_ALLOCATER_H__
