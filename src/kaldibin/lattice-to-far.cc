// latbin/lattice-to-fst.cc
 
// Copyright 2009-2011  Microsoft Corporation
 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.
 
#pragma GCC diagnostic ignored "-Wuninitialized"

#include <fst/extensions/far/far.h>

#include "base/kaldi-common.h"
#include "fstext/fstext-lib.h"
#include "fstext/lattice-utils.h"
#include "fstext/lattice-weight.h"
#include "lat/kaldi-lattice.h"
#include "util/common-utils.h"
 

using namespace fst;
using namespace kaldi;

//Compact case
void ConvertToFst(const CompactLattice& lat, 
  MutableFst<CompactLatticeArc>* fst) {
  *fst = lat;
}

//Non-compact weight 
template<class Arc>
void ConvertToFst(const CompactLattice& clat, MutableFst<Arc>* fst) {
  //RemoveAlignmentsFromCompactLattice(&clat)
  Lattice lat;
  ConvertLattice(clat, &lat);
  ConvertLattice(lat, fst);
  //Project(&fst, fst::PROJECT_OUTPUT); 
}


struct LatticeToFarOpts {
  float acoustic_scale;
  float lm_scale;
  bool rm_eps;
};


template<class Arc>
int ConvertLattices(ParseOptions* po, const LatticeToFarOpts& opts) {
  vector<vector<double> > scale = fst::LatticeScale(opts.lm_scale, opts.acoustic_scale);   
  std::string lats_rspecifier = po->GetArg(1);
  std::string fsts_wspecifier = po->GetArg(2);
 
  SequentialCompactLatticeReader lattice_reader(lats_rspecifier);
  FarWriter<Arc> *far_writer =
      FarWriter<Arc>::Create(fsts_wspecifier, fst::FAR_DEFAULT);
  if (!far_writer) 
    return -1;
   
  int n_done = 0; // there is no failure mode, barring a crash.
   
  string last_key;
  for (; !lattice_reader.Done(); lattice_reader.Next()) {
    std::string key = lattice_reader.Key();
    CompactLattice clat = lattice_reader.Value();
    lattice_reader.FreeCurrent();
    ScaleLattice(scale, &clat); 
    fst::VectorFst<Arc> fst;
    ConvertToFst(clat, &fst); 
    if (opts.rm_eps) 
      RemoveEpsLocal(&fst);
    LOG(INFO) << "Last key " << last_key << " key = " << key;
    far_writer->Add(key, fst);
    n_done++;
  }
  delete far_writer;
  KALDI_LOG << "Done converting " << n_done << " lattices to word-level FSTs";
  return (n_done != 0 ? 0 : 1);
}


int main(int argc, char *argv[]) {
  try {
    LatticeToFarOpts opts = { 1.0f, 1.0f, false };
    string arc_type = "std";
    const char *usage =
        "Turn lattices into a FAR archieve containing FSTs with lattice, compact lattice, std or log weights \n"
        "By default, removes all weights and also epsilons (configure with\n"
        "with --acoustic-scale, --lm-scale and --rm-eps)\n"
        "Usage: lattice-to-fst [options] lattice-rspecifier fsts-wspecifier\n"
        " e.g.: lattice-to-fst  ark:1.lats far.fsts\n";
       
    ParseOptions po(usage);
    po.Register("acoustic-scale", &opts.acoustic_scale, "Scaling factor for acoustic likelihoods");
    po.Register("lm-scale", &opts.lm_scale, "Scaling factor for graph/lm costs");
    po.Register("rm-eps", &opts.rm_eps, "Remove epsilons in resulting FSTs (in lazy way; may not remove all)");
    //po.Register("arc-type", &arc_type, "Semiring for the FSTs in the output far files");
    po.Read(argc, argv);
 
    if (po.NumArgs() != 2) {
      po.PrintUsage();
      exit(1);
    }
    if (arc_type == "std") 
      ConvertLattices<StdArc>(&po, opts);
    //else if (arc_type == "log")
    //  ConvertLattices<LogArc>(&po, opts);
    else if (arc_type == "lattice")
      ConvertLattices<LatticeArc>(&po, opts);
    else if (arc_type == "compactlattice")
      ConvertLattices<CompactLatticeArc>(&po, opts);
    else
      KALDI_ERR << "Unknown arc type : " << arc_type;
  } catch(const std::exception& e) {
  std::cerr << e.what();
  return -1;
  }
}
