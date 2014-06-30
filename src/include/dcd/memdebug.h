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
