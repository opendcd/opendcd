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

#include <iostream>
#include <dcd/config.h>

using namespace std;

namespace dcd {
void PrintVersionInfo() {
#ifndef OS_WIN
  cerr << "Version information : " << endl;
  cerr << "\t\tgit revision : " << g_dcd_gitrevision << endl;
  cerr << "\t\tCompiled on : " << __DATE__ << " " << __TIME__ << endl;
  cerr << "\t\tCompiler flags : " << g_dcd_cflags << endl;
  cerr << "\t\tLinker flags : " << g_dcd_lflags << endl;
  cerr << "\t\tCompiler version : " << g_dcd_compiler_version << endl << endl;
#endif
}
} //namespace dcd
