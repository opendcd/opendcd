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
