// constants.h
// \file

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
