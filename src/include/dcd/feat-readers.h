// feat-readers.h
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
/// Basic standalone feature reader for Kaldi .ark
/// files. Supports text and binary format, but
/// currently limited to matrix float readers.

#ifndef DCD_FEATREADERS_H__
#define DCD_FEATREADERS_H__

#include <vector>
#include <fstream>
#include <limits>
#include <string>
#include <sstream>
#include <cstdlib>
#include <fst/util.h>

#include <dcd/utils.h>

namespace dcd {

template<class T>
class SequentialMatrixReader;

template <class T>
struct SubVector {
  int Size() const  { return data.size(); }
  T operator () (int index) { return data[index]; }
  const std::vector<T>& data;
};

template <class T>
class Matrix {
 public:
  Matrix() { }

  explicit Matrix(const std::vector<std::vector<T> >& data)
  : data_(data) { }

  int NumRows() const { return data_.size(); }

  const std::vector<T>& Row(int i) const { return data_[i]; }

  void ReserveRows(int i) { data_.reserve(i); }

  void PushRow(const vector<T>& row) { data_.push_back(row); }

  void Clear() { data_.clear(); }

  const std::vector<std::vector<T> >& Data() const { return data_; }

  const T& operator() (int row, int column) const { 
    return data_[row][column]; 
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Matrix);
  std::vector<std::vector<T> > data_;
};


template<class T>
class SequentialMatrixReader {

  struct UtteranceHeader {

    UtteranceHeader() : numframes(0), vecsize(0) { }
  
    bool Read(std::istream& strm) {
      using fst::ReadType;
      strm >> id;
      strm.read(things, sizeof(things));
      ReadType(strm, &numframes);
      char c;
      ReadType(strm, &c);
      ReadType(strm, &vecsize);
      return true;
    }

    void Print() {
      LOG(INFO) << id << " " << numframes << " " << vecsize;
    }

    void Clear() { 
      numframes = 0;
      vecsize = 0;
      id.clear();
    }

    char things[7];
    std::string id;
    int numframes;
    int vecsize;
    
  };

 public:
  explicit SequentialMatrixReader(const std::string& path, bool cacheall = false)
      : src_(path), done_(false) {
    vector<string> fields;
    SplitStringToVector(src_, ":", true, &fields);
    if (fields.size() != 2)
      FSTERROR() << "SequentialMatrixReader::SequentialMatrixReader : "
                    "Incorrect read specifier : " << src_ <<
                    " Num fields " << fields.size();
    
    if (fields[1] == "-") {
      fields[1] = "/dev/stdin";
      cacheall = false;
    }

    if (cacheall) {
      std::ifstream ifs(fields[1].c_str(), std::fstream::binary);
      if (!ifs.is_open())
        FSTERROR() << "SequentialMatrixReader : Failed to open stream " <<
          fields[1];

      ifs.seekg(0, std::ios::end);
      std::streampos filesize = ifs.tellg();
      ifs.seekg(0, std::ios::beg);
      buffer_.resize(filesize);
      ifs.read(&buffer_[0], filesize);
      featstream_ = new std::istringstream(buffer_);
    } else {
      std::ifstream* featstream = new std::ifstream(fields[1].c_str(), std::fstream::binary);
      if (!featstream->is_open()) {
        delete featstream;
        FSTERROR() << "SequentialMatrixReader::SequentialMatrixReader : "
                      "Failed to open file : " << src_;
      } else {
        featstream_ = featstream;
      }
    }
    ReadNextUtterance();
  }
 
  ~SequentialMatrixReader() { 
    if (featstream_)
      delete featstream_;
  }

  bool ReadNextUtterance() {
    if (featstream_->peek() == EOF) {
      done_ = true;
      return false;
    }
    using fst::ReadType;
    hdr_.Clear();
    features_.Clear();
    if (!hdr_.Read(*featstream_)) {
      FSTERROR() << "ReadNextUtterance :  Failed to read header";
      return false;
    }
    features_.Clear();
    features_.ReserveRows(hdr_.numframes);
    for (int i = 0; i != hdr_.numframes; ++i) {
      vector<T> frame;
      frame.resize(hdr_.vecsize);
      for (int j = 0; j != hdr_.vecsize; ++j)
        ReadType(*featstream_, &frame[j]);
      features_.PushRow(frame);
    }
    return true;
  }

  void FreeCurrent() { 
    hdr_.Clear(); 
    features_.Clear();
  } 
  
  void Next() { ReadNextUtterance(); }

  bool Done() const { return done_; }

  void Reset() { } //featstream_.seekg(0); }

  const std::string& Key() const { return hdr_.id; }

  const Matrix<T>& Value() const { return features_; }

 private:
  std::istream* featstream_;
  std::string src_;
  std::string buffer_;
  Matrix<T> features_;
  UtteranceHeader hdr_;
  bool done_;
  DISALLOW_COPY_AND_ASSIGN(SequentialMatrixReader);
};

typedef SequentialMatrixReader<float> SequentialBaseFloatMatrixReader;  

class SimpleDecodable {
 public:
  SimpleDecodable(const Matrix<float> &matrix, float scale)
    : matrix_(matrix) { }

  float LogLikelihood(int frame, int index) const {
    return matrix_(frame, index);
  }

  int NumFrames() const { return matrix_.NumRows(); }

  bool IsLastFrame(int frame) const { return (matrix_.NumRows() - 1 == frame); }

  const Matrix<float> &matrix_;
 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleDecodable);
};

class SimpleDecodableHitStats {
 public:
  SimpleDecodableHitStats(const Matrix<float> &matrix, float scale)
    : matrix_(matrix), max_index_(0) { 
      hit_stats_.resize(matrix.NumRows());
    }

  ~SimpleDecodableHitStats() {
    if (hit_dump_.is_open()) {
      hit_dump_ << "# of frames = " << hit_stats_.size() 
        << ", # of states = " << max_index_ << endl;
      std::stringstream ss;
      for (int i = 0; i != hit_stats_.size(); ++i) {
        vector<int>& frame_stats = hit_stats_[i];
        frame_stats.resize(max_index_ + 1);
        float sum = 0.0f;
        int num_hit = 0;
        for (int j = 0 ; j != frame_stats.size(); ++j) {
          sum += frame_stats[j];
          if (frame_stats[j])
            ++num_hit;
        }
        ss << "# states hits " << num_hit << " : ";
        for (int j = 0 ; j != frame_stats.size(); ++j) {
          ss << frame_stats[j] << ((j + 1) < frame_stats.size() ? " " : "\n");
        }
      }
      hit_dump_ << ss.str();
    }
    hit_stats_.clear();
  }

  float LogLikelihood(int frame, int index) const {
    vector<int>& frame_stats = hit_stats_[frame];
    if (index >= frame_stats.size())
      frame_stats.resize(index + 1);
    if (frame_stats[index] >= std::numeric_limits<int>::max()) {
      cerr << "Out of space" << endl;
      exit(-1);
    }
    ++frame_stats[index];
    max_index_ = std::max(index, max_index_);
    return matrix_(frame, index);
  }

  int NumFrames() const { return matrix_.NumRows(); }

  bool IsLastFrame(int frame) const { return (matrix_.NumRows() - 1 == frame); }

  const Matrix<float>& matrix_;

  mutable vector<vector<int> > hit_stats_;

  mutable int max_index_;

  static std::ofstream hit_dump_;

  static bool OpenDumpFile(const std::string& path);
 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleDecodableHitStats);
};
}

#endif // DCD_FEATREADERS_H__
