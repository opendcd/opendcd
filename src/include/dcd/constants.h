// constants.h
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
// Global constants and definitions

#ifndef DCD_CONSTANTS_H__
#define DCD_CONSTANTS_H__

#include <limits>

namespace dcd {

const int kMaxTokensPerArc = 15;
const float kMaxCost = std::numeric_limits<int>::max();
const float kMinCost = std::numeric_limits<int>::min();
const float kDefaultBeam = kMaxCost;
const int kMaxArcs = std::numeric_limits<int>::max();
const float kDefaultAcousticScale = 0.1;
const float kDefaultTranScale = 0.1;
const int kDefaultGcPeriod = 25;
const int kDefaultActiveListSize = 10000;

const int kMegaByte = 1024 * 1024;
const int kKiloByte = 1024;

//Flags for final state mode. After, decoding we
//can require final states, backoff to non-final
//or always allow non-final
const int kRequireFinal = 1;
const int kBackoff = 2;
const int kAnyFinal = 3;

//Visual Studio sometimes doesn't have a definiton for this
#ifndef M_LOG_2PI
  #define M_LOG_2PI 1.8378770664093454835606594728112
#endif

} //namespace dcd

#endif 
