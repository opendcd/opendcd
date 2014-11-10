// config.h
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
// Wrapper and utlilties for cross platform compilation and specifying
// external libraries. Try and put all the cross platform and
// build specific things inside this file

#ifndef DCD_CONFIG_H__
#define DCD_CONFIG_H__

#ifdef _MSC_VER
  // Probably don't need thread safety on Windows anyway
  #define thread_local
#else
  // Actually __thread is gcc verion specific
  #define thread_local
#endif

#ifdef HAVE_SHINY
  #include <Shiny.h>
#else
  #define PROFILE_FUNC();
  #define PROFILE_UPDATE();
  #define PROFILE_OUTPUT(NULL);
  #define PROFILE_END();
  #define PROFILE_BEGIN(Param);
#endif




#if defined(_WIN32)
#define NOMINMAX
# include <windows.h>
#else
# include <sys/time.h>
# include <unistd.h>
#endif


#ifdef HAVE_KALDI
  #include <util/table-types.h>
  #include <util/parse-options.h>
  #include <itf/decodable-itf.h>
  #include <decoder/decodable-matrix.h>
  using kaldi::SequentialBaseFloatMatrixReader;
  using kaldi::Matrix;
  using kaldi::ParseOptions;
  using kaldi::Trim;
  typedef kaldi::DecodableMatrixScaled Decodable;
#else
  #include <dcd/feat-readers.h>
  #include <dcd/parse-options.h>
  namespace dcd {
    typedef SimpleDecodable Decodable;
  }
#endif

extern const char* g_dcd_gitrevision;
extern const char* g_dcd_cflags;
extern const char* g_dcd_lflags;
extern const char* g_dcd_compiler_version;

namespace dcd {

size_t GetPeakRSS();

size_t GetCurrentRSS();

void PrintVersionInfo();

void PrintMachineInfo();

int SwallowRedpill();

// Based on the timer class from Kaldi
#if defined(_WIN32)
class Timer {
 public:
  Timer() { Reset(); }
  void Reset() {
    QueryPerformanceCounter(&time_start_);
  }
  double Elapsed() {
    LARGE_INTEGER time_end;
    LARGE_INTEGER freq;
    QueryPerformanceCounter(&time_end);
    if (QueryPerformanceFrequency(&freq) == 0)
      return 0.0;  // Hardware does not support this.
    return ((double)time_end.QuadPart - (double)time_start_.QuadPart) /
        ((double)freq.QuadPart);
  }
 private:
  LARGE_INTEGER time_start_;
};
#else
class Timer {
 public:
  Timer() { Reset(); }

  void Reset() { gettimeofday(&this->time_start_, &time_zone_); }

  // Returns time in seconds.
  double Elapsed() {
    struct timeval time_end;
    gettimeofday(&time_end, &time_zone_);
    double t1, t2;
    t1 =  (double)time_start_.tv_sec +
        (double)time_start_.tv_usec / (1000 * 1000);
    t2 =  (double)time_end.tv_sec + (double)time_end.tv_usec / (1000 * 1000);
    return t2 - t1;
  }

 private:
  struct timeval time_start_;
  struct timezone time_zone_;
};
#endif

}  // namespace dcd

#endif  // DCD_CONFIG_H__
