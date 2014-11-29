// search-statisitcs.h
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
// Author: Paul R. Dixon
// \file
// This is the decoder statistics. The decoder
// is templated on this class. Certain implementation
// will not t perform anything and will be optimized
// out. Faster, neater and safer than using ifdef-else macros
//
#ifndef DCD_SEARCH_STATISTICS_H__
#define DCD_SEARCH_STATISTICS_H__

#include <limits>
#include <vector>
#include <dcd/utils.h>

namespace dcd {
struct RunnningStatistic {
  void Clear() {
    num_ = 0;
    max_ = kMaxCost;
    min_ = kMinCost;
  }

  void Push(float value) {
  }

  float Max() const { return max_; }
  float Min() const { return min_; }
  float Ave() const { return ave_; }
  float Variance() const { return var_; }

  int num_;
  float max_;
  float min_;
  float ave_;
  float var_;
};

enum Statistic {
    kActiveArcCosts = 0,
    kNumActiveArcs,
    kNumActiveState,
    kNumArcsPruned,
    kNumArcsHistogramPruned,
    kActiveStateCosts,
    kNumActiveArc,
    kNumOfStats
};

struct UtteranceSearchStatstics {
};



class NullStatistics {
 public:
  void EpsilonExpanded(int state) { }

  void StateExpanded(int state) { }

  void ArcExpanded(int ilabel) { }

  void Clear() { }

  void Push(const NullStatistics) { }

  void PushStatistic(int statistic) { }

  const RunnningStatistic& GetStatistic(int stat) {
    return running_stat_;
  }

  RunnningStatistic running_stat_;
};

class SimpleStatistics {
 public:
  void EpsilonExpanded(int state) {
    eps_hits_.Insert(state);
  }

  void StateExpanded(int state) {
    state_hits_.Insert(state);
  }

  void ArcExpanded(int ilabel) {
    if (ilabel_hits_.size() < ilabel)
      ilabel_hits_.resize(ilabel);
    ++ilabel_hits_[ilabel];
  }

  void PushStatistic(int statistic, float value) {
    running_stats_[statistic].Push(value);
  }

  void Clear() {
    eps_hits_.Clear();
    state_hits_.Clear();
    ilabel_hits_.clear();
    num_ = 1;
  }
  /*
  void PrintEpsHits(const string& path) {
     ofstream ofs(path.c_str());
     for (CountSet<int>::const_iterator it = eps_hits_.Begin();
         it != eps_hits_.End(); ++it) {
       ofs << it->first << " " << it->second << endl;
     }
   }
*/

 private:
  CountSet<int> eps_hits_;
  CountSet<int> state_hits_;
  vector<int> ilabel_hits_;
  int num_;
  RunnningStatistic running_stats_[kNumOfStats];
};


}  // namespace dcd
#endif  // DCD_SEARCH_STATISTICS_H__
