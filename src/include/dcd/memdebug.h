// memdebug.h
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
// Globabl declarations for memory debugging variable and functions
//
#ifndef _DCD_MEM_DEBUG_H__ 
#define _DCD_MEM_DEBUG_H__

#include <cstring>

extern unsigned long long g_dcd_num_bytes_allocated;
extern unsigned long long g_dcd_num_bytes_freed;
extern size_t g_dcd_current_num_allocated;
extern size_t g_dcd_peak_bytes_allocated;
extern size_t g_dcd_num_allocs;
extern size_t g_dcd_num_frees;
extern size_t g_dcd_global_allocated;
extern bool g_dcd_memdebug_enabled;
void PrintMemorySummary();

#endif 
