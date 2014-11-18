// config.cc
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
// Configuration and version utilties

#include <unistd.h>
#include <iostream>
#include <dcd/config.h>

using namespace std;

namespace dcd {
void PrintVersionInfo() {
#ifndef OS_WIN
  cerr << "Version information : " << endl;
  cerr << "\tgit revision\t\t: " << g_dcd_gitrevision << endl;
  cerr << "\tCompiled on\t\t: " << __DATE__ << " " << __TIME__ << endl;
  cerr << "\tCompiler flags\t\t: " << g_dcd_cflags << endl;
  cerr << "\tLinker flags\t\t: " << g_dcd_lflags << endl;
  cerr << "\tCompiler version\t: " << g_dcd_compiler_version << endl << endl;
#endif
}


// Functions to queury to the total system memory take from
// http://stackoverflow.com/questions/2513505/how-to-get-available-memory-c-g
#ifdef MSVCVER
size_t GetTotalSystemMemory() {
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullTotalPhys;
}
#else
size_t GetTotalSystemMemory() {
    /*
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size; */
  return 0;
}
#endif

#ifdef _MSCVER
string GetHostname() {
  return "";
}
#else
string GetHostname() {
  char str[1024];
  memset(str, 0, 1024);
  gethostname(str, 1024);
  return string(str);
}
#endif

void PrintMachineInfo() {
  cerr << "Machine information : " << endl;
  cerr << "\tNumber of cores\t\t: " << endl;
  cerr << "\tNumber of threads\t: " << endl;
  cerr << "\tMachine load\t\t: " << endl;
  cerr << "\tMachine is virtual\t: " << endl;
  cerr << "\tAmount of memory\t: " << GetTotalSystemMemory()  << endl;
  cerr << "\tOS version\t\t: " << endl;
  cerr << "\tHostname\t\t: " << GetHostname() << endl;
}

// Code take from https://www.hackerzvoice.net/ouah/Red_%20Pill.html
// TODO(Paul) doesn't seem to work on 64bit
int SwallowRedpill() {
  unsigned char m[2+4], rpill[] = "\x0f\x01\x0d\x00\x00\x00\x00\xc3";
  *((unsigned long*)&rpill[3]) = (unsigned long)m;
  ((void(*)())&rpill)();
  return (m[5] > 0xd0) ? 1 : 0;
}
}  // namespace dcd
