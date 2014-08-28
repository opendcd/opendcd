// cascade.h
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
// Helper class to construct the recongnition cascade
// from individual transducers

#ifndef DCD_CASCADE_H__
#define DCD_CASCADE_H__
#include <fst/compose.h>

namespace dcd {
template<class Arc>
class Cascade {
 typedef Fst<Arc> FST;
 public:
  
  ~Cascade() {
    DestroyFsts(&tmp_fsts_);
    DestroyFsts(&fsts_);
  }

  static Cascade *Read(const string &path) {
    Logger logger("ocdrecog", std::cerr, true);
    char* str = new char[path.size() + 1];
    strcpy(str, path.c_str());
    vector<char*> paths;
    SplitToVector(str, ",", &paths, true);
    Cascade* cascade  = new Cascade;
    for (int i = 0; i != paths.size(); ++i) {
      logger(INFO) << "Reading fst from : " << paths[i];
      Fst<Arc>* fst = Fst<Arc>::Read(paths[i]);
      if (!fst)
        LOG(FATAL) << "Failed to read fst from : " << paths[i];
      logger(INFO) << "Fst type = " << fst->Type() << 
        ", # of states = " << CountStates(*fst);
      cascade->fsts_.push_back(fst);
    }
    delete str;
    return cascade;
  }
  
  static Cascade *Read(istream& strm, const string& src = "") {
    vector<Fst<Arc>*> fsts;
    FstHeader hdr;
    while (hdr.Read(strm, src, true)) {
      Fst<Arc>* fst = Fst<Arc>::Read(strm);
      if (fst)
        fsts.push_back(fst);
      else
        logger(FATAL) << "Failed to read Fst from lingware";
    }
    if (fsts.size() == 0)
      return 0;
    Cascade* cascade  = new Cascade;
    cascade->fsts_ = fsts;
    return cascade;
   }

  FST* Rebuild() {
    if (fsts_.size() == 1)
      return fsts_[0];
    else if (fsts_.size() == 2) {
      if (tmp_fsts_.size()) 
        DestroyFsts(&tmp_fsts_);
      CacheOptions opts;
      opts.gc = false;
      Fst<Arc> *fst = new ComposeFst<Arc>(*fsts_[0], *fsts_[1], opts);
      tmp_fsts_.push_back(fst);
      return fst;
    }
    return 0;
  }

  void DestroyFsts(vector<Fst<Arc>*>* fsts) {
    for (int i = fsts->size() - 1; i >= 0; --i) {
      Fst<Arc>* fst = fsts->at(i);
      if (fst)
        delete fst;
      else
        LOG(WARN) << "Trying to destoy a null pointer!";
    }
    fsts->clear();
  }
  //Build the recognition cascade storing the individual compose
  //fsts in the cacade vector - we will need these when we
  //delete the cascade later on
  Fst<Arc>* BuildCascade(const vector<Fst<Arc>*>& fsts,
                       vector<Fst<Arc>*>* cascade) {
   if (fsts.size() == 0)
     return 0;
   if (fsts.size() == 1)
     return fsts.back();
   Fst<Arc>* last = fsts.back();
   for (int i = fsts.size() - 2; i >= 0; --i) {
     Fst<Arc>* composed = new ComposeFst<Arc>(*fsts[i], *last);
     if (!composed) {
       LOG(ERROR) << "Failed to construct composition Fst";
     } else {
       cascade->push_back(composed);
       last = composed;
     }
   }
   return last;
  }

  int NumFsts() const { return fsts_.size(); }

 private:
  vector<FST*> fsts_;
  vector<FST*> tmp_fsts_;
  FST* cascade_;
};
}

#endif
