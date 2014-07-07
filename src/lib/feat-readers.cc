// feat-readers.cc
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
// Concrete function for the decodable that support state hit statitics

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
