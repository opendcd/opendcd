
#include <sstream>

#include <dcd/feat-readers.h>

using namespace std;

namespace dcd {

std::ofstream SimpleDecodableHitStats::hit_dump_;

bool SimpleDecodableHitStats::OpenDumpFile(const string& path) {
  hit_dump_.open(path.c_str());
  return hit_dump_.is_open();
}
}
