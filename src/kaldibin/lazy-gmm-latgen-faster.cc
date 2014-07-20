// gmmbin/gmm-latgen-faster.cc

// Copyright 2009-2012  Microsoft Corporation
//                      Johns Hopkins University (author: Daniel Povey)

// See ../../COPYING for clarification regarding multiple authors
//
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


#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "gmm/am-diag-gmm.h"
#include "tree/context-dep.h"
#include "hmm/transition-model.h"
#include "fstext/fstext-lib.h"
#include "decoder/lattice-faster-decoder.h"
#include "gmm/decodable-am-diag-gmm.h"
#include "util/timer.h"
#include "feat/feature-functions.h"  // feature reversal
#include "fst/matcher-fst.h" //lookahead
using namespace kaldi;
typedef kaldi::int32 int32;
using fst::SymbolTable;
using fst::VectorFst;
using fst::StdArc;
using fst::ConstFst;

using fst::Fst;
using fst::StdOLabelLookAheadFst;
using fst::ComposeFst;
using fst::CacheOptions;
using std::string;

using fst::MatcherFst;
using fst::ConstFst;
using fst::SortedMatcher;
using fst::LabelLookAheadRelabeler;
using fst::LabelLookAheadMatcher;
using fst::CacheLogAccumulator;
using fst::FastLogAccumulator;
using fst::olabel_lookahead_flags;
using fst::olabel_lookahead_fst_type;
typedef MatcherFst<ConstFst<StdArc>,
                   LabelLookAheadMatcher<SortedMatcher<ConstFst<StdArc> >,
                                         olabel_lookahead_flags,
                                         FastLogAccumulator<StdArc> >,
                   olabel_lookahead_fst_type,
                   LabelLookAheadRelabeler<StdArc> > CacheAccStdOLabelLookAheadFst;

//http://stackoverflow.com/questions/236129/how-to-split-a-string-in-c/236803#236803
vector<string> &split(const string &s, char delim, vector<string> &elems) {
  std::stringstream ss(s);
  string item;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

vector<string> split(const string &s, char delim) {
  vector<string> elems;
  split(s, delim, elems);
  return elems;
}


int build_decoder( vector<Fst<StdArc>* > components, bool allow_partial, 
                   BaseFloat acoustic_scale, string word_syms_filename,
                   LatticeFasterDecoderConfig config, 
                   string model_in_filename,
                   string feature_rspecifier,
                   string words_wspecifier,
                   string alignment_wspecifier,
                   string lattice_wspecifier
                   ) {
  Timer timer;
  
  TransitionModel trans_model;
  AmDiagGmm am_gmm;
  {
    bool binary;
    Input ki(model_in_filename, &binary);
    trans_model.Read(ki.Stream(), binary);
    am_gmm.Read(ki.Stream(), binary);
  }

  bool determinize = config.determinize_lattice;
  CompactLatticeWriter compact_lattice_writer;
  LatticeWriter lattice_writer;
  if (! (determinize ? compact_lattice_writer.Open(lattice_wspecifier)
         : lattice_writer.Open(lattice_wspecifier)))
    KALDI_ERR << "Could not open table for writing lattices: "
              << lattice_wspecifier;
  
  Int32VectorWriter words_writer(words_wspecifier);
  
  Int32VectorWriter alignment_writer(alignment_wspecifier);
  
  fst::SymbolTable *word_syms = NULL;
  if (word_syms_filename != "") 
    if (!(word_syms = fst::SymbolTable::ReadText(word_syms_filename)))
      KALDI_ERR << "Could not read symbol table from file "
                << word_syms_filename;

  double tot_like = 0.0;
  kaldi::int64 frame_count = 0;
  int num_done = 0, num_err = 0;
  SequentialBaseFloatMatrixReader feature_reader(feature_rspecifier);

  CacheOptions clg_opts;
  clg_opts.gc       = false;
  clg_opts.gc_limit = 1<<20LL;
  LatticeFasterDecoder* decoder = 0;
  Fst<StdArc>* hclg_fst = 0;

  {
    if (components.size() == 1) {
      hclg_fst = components[0];
      decoder = new LatticeFasterDecoder(*hclg_fst, config);
    }
    
    for (; !feature_reader.Done(); feature_reader.Next()) {
      if (components.size() == 2 && num_done % 10 == 0) {
        KALDI_LOG << "Resetting network";
        delete decoder;
        delete hclg_fst;
        hclg_fst = new ComposeFst<StdArc>(*components[0], 
                                          *components[1],
                                          clg_opts);
        decoder = new LatticeFasterDecoder(*hclg_fst, config);
        KALDI_LOG << "Network reset complete";
      }

      std::string utt = feature_reader.Key();
      Matrix<BaseFloat> features (feature_reader.Value());
      feature_reader.FreeCurrent();
      if (features.NumRows() == 0) {
        KALDI_WARN << "Zero-length utterance: " << utt;
        num_err++;
        continue;
      }
          
      DecodableAmDiagGmmScaled gmm_decodable(am_gmm, trans_model, features,
                                             acoustic_scale);

      double like;
      if (DecodeUtteranceLatticeFaster(
            *decoder, gmm_decodable, word_syms, utt, acoustic_scale,
            determinize, allow_partial, &alignment_writer, &words_writer,
            &compact_lattice_writer, &lattice_writer, &like)) {
        tot_like += like;
        frame_count += features.NumRows();
        num_done++;
      } else num_err++;
    }
  }
  delete decoder; // delete this only after decoder goes out of scope.
      
  double elapsed = timer.Elapsed();
  KALDI_LOG << "Time taken "<< elapsed
            << "s: real-time factor assuming 100 frames/sec is "
            << (elapsed*100.0/frame_count);
  KALDI_LOG << "Done " << num_done << " utterances, failed for "
            << num_err;
  KALDI_LOG << "Overall log-likelihood per frame is " << (tot_like/frame_count) << " over "
            << frame_count << " frames.";

  if (word_syms) delete word_syms;
  if (num_done != 0) return 0;
  else return 1;
}


int main (int argc, char *argv[]) {
  try {
    const char *usage = 
        "Generate lattices using GMM-based model.\n"
        "Usage: gmm-latgen-faster [options] model-in (fst-in|fsts-rspecifier)"
        " features-rspecifier lattice-wspecifier" 
        " [ words-wspecifier [alignments-wspecifier] ]\n";
    ParseOptions po(usage);
    bool allow_partial = false;
    BaseFloat acoustic_scale = 0.1;
    LatticeFasterDecoderConfig config;
    
    std::string word_syms_filename;
    config.Register(&po);
    po.Register("acoustic-scale", &acoustic_scale,
                "Scaling factor for acoustic likelihoods");
    po.Register("word-symbol-table", &word_syms_filename,
                "Symbol table for words [for debug output]");
    po.Register("allow-partial", &allow_partial,
                "If true, produce output even if end state was not reached.");
    po.Read(argc, argv);
    
    if (po.NumArgs() < 4 || po.NumArgs() > 6) {
      po.PrintUsage();
      exit(1);
    }
    
    std::string model_in_filename = po.GetArg(1),
        fst_in_str = po.GetArg(2),
        feature_rspecifier   = po.GetArg(3),
        lattice_wspecifier   = po.GetArg(4),
        words_wspecifier     = po.GetOptArg(5),
        alignment_wspecifier = po.GetOptArg(6);
    
    vector<string> fsts = split(fst_in_str, ',');
    vector<Fst<StdArc>* > components;
    Fst<StdArc>* hclg_fst = (fsts.size() == 1) ? Fst<StdArc>::Read(fsts.at(0)) : 0;
    Fst<StdArc>* h_fst    = (fsts.size() == 2) ? Fst<StdArc>::Read(fsts.at(0)) : 0;
    Fst<StdArc>* clg_fst  = (fsts.size() == 2) ? Fst<StdArc>::Read(fsts.at(1)) : 0;

    switch (fsts.size()) {
      case 1:
      {
        components.push_back(hclg_fst);
        break;
      }
      case 2:
      {
        components.push_back(h_fst);
        components.push_back(clg_fst);
        break;
      }
      default:
      { 
        KALDI_ERR << "Number of component FSTs should be 1.";
        return -1;
      }
    }

    return build_decoder( 
                    components, 
                    allow_partial, 
                    acoustic_scale, 
                    word_syms_filename, 
                    config, 
                    model_in_filename, 
                    feature_rspecifier,
                    words_wspecifier, 
                    alignment_wspecifier, 
                    lattice_wspecifier
                    );


   } catch (const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}
