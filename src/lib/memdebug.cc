// memdebug.cc
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
// Memory debugging and statistics functions

#include <cstdlib>
#include <iostream> 
#include <new>

#include <dcd/constants.h>
#include <dcd/memdebug.h>

#ifdef HAVEJEMALLOC
#include <jemalloc/jemalloc.h>
#endif

using namespace std;
using namespace dcd;

unsigned long long g_dcd_num_bytes_allocated = 0;
unsigned long long g_dcd_num_bytes_freed = 0;
size_t g_dcd_current_num_allocated = 0;
size_t g_dcd_peak_bytes_allocated = 0;
size_t g_dcd_num_allocs = 0;
size_t g_dcd_num_frees = 0;
size_t g_dcd_global_allocated = 0;
bool g_dcd_memdebug_enabled = false;

/*
void* caller() {
  const int target = 3;
  void* returnaddresses[target];
  if (backtrace(returnaddresses, target) < target) {
      return NULL;
  }
  return returnaddresses[target-1];
}*/

#ifdef MEMDEBUG
class MembugSetup {
  public: MembugSetup() { g_dcd_memdebug_enabled = true; }
};
MembugSetup membug_setup;

void* operator new(size_t size) throw(bad_alloc) {
  size_t* ret = (size_t*)malloc(size + sizeof(size));
  g_dcd_num_bytes_allocated += size;
  g_dcd_current_num_allocated += size;
  g_dcd_peak_bytes_allocated = 
    max(g_dcd_peak_bytes_allocated, g_dcd_current_num_allocated);
  ret[0] = size;
  if (!ret)
    throw bad_alloc();
  ++g_dcd_num_allocs;
  return (void*)(ret + 1);
}

void* operator new[] (size_t size) throw(bad_alloc) {
  size_t* ret = (size_t*)malloc(size + sizeof(size));
  g_dcd_num_bytes_allocated += size;
  g_dcd_current_num_allocated += size;
  g_dcd_peak_bytes_allocated = 
    max(g_dcd_peak_bytes_allocated, g_dcd_current_num_allocated);
  ret[0] = size;
  if (!ret)
    throw bad_alloc();
  ++g_dcd_num_allocs;
  return (void*)(ret + 1);
}

void operator delete (void* data) throw() {
  size_t* size = (size_t*)(data) - 1;
  g_dcd_num_bytes_freed += *size;
  g_dcd_current_num_allocated -= *size;
  free(size);
  ++g_dcd_num_frees;
}

void operator delete [] (void* data) throw() {
  size_t* size = (size_t*)(data) - 1;
  g_dcd_num_bytes_freed += *size;
  g_dcd_current_num_allocated -= *size;
  free(size);
  ++g_dcd_num_frees;
}
#endif

void PrintMemorySummary() {
#ifdef HAVEJEMALLOC
  malloc_stats_print(NULL, NULL, NULL);
#endif
  if (g_dcd_memdebug_enabled) {
    cerr << "Memory debug summary :" << endl;
    cerr << "\t\tTotal  # bytes allocated : " << g_dcd_num_bytes_allocated 
         << " (" << g_dcd_num_bytes_allocated / kMegaByte <<  "MB)" << endl;
    cerr << "\t\tTotal  # bytes freed : " << g_dcd_num_bytes_freed 
         << " (" << g_dcd_num_bytes_allocated / kMegaByte <<  "MB)" << endl;
    cerr << "\t\tPeak   # bytes allocated : " << g_dcd_peak_bytes_allocated
         << " (" << g_dcd_peak_bytes_allocated / kMegaByte <<  "MB)" << endl;
    cerr << "\t\tGlobal # bytes allocated : " << g_dcd_global_allocated  
         << " (" << g_dcd_global_allocated / kKiloByte <<  "KB)" << endl;
    size_t lost = g_dcd_current_num_allocated - g_dcd_global_allocated; 
    cerr << "\t\tLost   # of bytes (potentially) : " <<  lost 
         << " (" << lost / kKiloByte <<  "KB)" << endl;
    cerr << "\t\tTotal  # of allocs " << g_dcd_num_allocs << endl;
    /*
    //Some profiling code to get an allocation histogram
    ofstream ofs("alocs.txt");
    for (CountSet<size_t>::const_iterator it =  g_dcd_alloc_sizes.Begin();
        it != g_dcd_alloc_sizes.End(); ++it) 
      ofs << it->first << " " << it->second << endl;
    */
    //ArcDecoder<KaldiLatticeFst, GenericTransitionModel, NNLattice> 
    //  latticerescore(&l, trans_model, opts);
  }

} //namespace dcd
