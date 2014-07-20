// gmmbin/gmm-decode-faster.cc

// Copyright 2009-2011  Microsoft Corporation
//                2013  Johns Hopkins University (author: Daniel Povey)

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
#include "hmm/transition-model.h"
#include "fstext/fstext-lib.h"
#include "decoder/faster-decoder.h"
#include "gmm/decodable-am-diag-gmm.h"
#include "util/timer.h"
#include "lat/kaldi-lattice.h" // for CompactLatticeArc
#include "fst/matcher-fst.h" //lookahead
using namespace kaldi;
typedef kaldi::int32 int32;
using fst::SymbolTable;
using fst::VectorFst;
using fst::ConstFst;
using fst::StdArc;
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


int really_decode( 
    vector<Fst<StdArc>* > components,
    bool allow_partial, BaseFloat acoustic_scale,
    string word_syms_filename, FasterDecoderOptions decoder_opts,
    string model_rxfilename, string feature_rspecifier,
    string words_wspecifier, string alignment_wspecifier, 
    string lattice_wspecifier, bool clg_use_gc, int clg_gc_limit,
    bool hclg_use_gc, int hclg_gc_limit, int mod_reset
		   ){

  TransitionModel trans_model;
  AmDiagGmm am_gmm;
  {
    bool binary;
    Input ki(model_rxfilename, &binary);
    trans_model.Read(ki.Stream(), binary);
    am_gmm.Read(ki.Stream(), binary);
  }

  Int32VectorWriter words_writer(words_wspecifier);

  Int32VectorWriter alignment_writer(alignment_wspecifier);

  CompactLatticeWriter clat_writer(lattice_wspecifier);

  SymbolTable *word_syms = NULL;
  if (word_syms_filename != "") 
    if (!(word_syms = fst::SymbolTable::ReadText(word_syms_filename)))
      KALDI_ERR << "Could not read symbol table from file "
		<< word_syms_filename;

  SequentialBaseFloatMatrixReader feature_reader(feature_rspecifier);

  BaseFloat tot_like            = 0.0;
  kaldi::int64 frame_count      = 0;
  int num_success = 0, num_fail = 0;

  Timer timer;
  //Only used *if* we actually have three components...
  //What is the 'right' way to do this in C++???
  CacheOptions clg_opts;
  clg_opts.gc       = clg_use_gc;
  clg_opts.gc_limit = clg_gc_limit;
  CacheOptions hclg_opts;
  hclg_opts.gc       = hclg_use_gc;
  hclg_opts.gc_limit = hclg_gc_limit;
  FasterDecoder* decoder       = 0;
  Fst<StdArc>* clg_fst  = 0;
  Fst<StdArc>* hclg_fst = 0;
  if( components.size()==1 ){
    hclg_fst = components.at(0);
    decoder  = new FasterDecoder(*hclg_fst, decoder_opts);
  }
  
  for (; !feature_reader.Done(); feature_reader.Next()) {
    if( components.size()==3 && num_success % mod_reset == 0 ){
      KALDI_LOG << "Resetting network...";
      delete decoder;
      delete hclg_fst;
      delete clg_fst;
      clg_fst  = new ComposeFst<StdArc>(*components.at(1), *components.at(2), clg_opts);
      hclg_fst = new ComposeFst<StdArc>(*components.at(0), *clg_fst, hclg_opts);
      decoder  = new FasterDecoder(*hclg_fst, decoder_opts);
      KALDI_LOG << "Network reset complete...";
    }else if( components.size()==2 && num_success % mod_reset == 0 ){
      KALDI_LOG << "Resetting network...";
      delete decoder;
      delete hclg_fst;
      hclg_fst = new ComposeFst<StdArc>(*components.at(0), *components.at(1), clg_opts);
      decoder  = new FasterDecoder(*hclg_fst, decoder_opts);
      KALDI_LOG << "Network reset complete...";
    }
    
    string key = feature_reader.Key();
    Matrix<BaseFloat> features (feature_reader.Value());
    feature_reader.FreeCurrent();
    if (features.NumRows() == 0) {
      KALDI_WARN << "Zero-length utterance: " << key;
      num_fail++;
      continue;
    }

    DecodableAmDiagGmmScaled gmm_decodable(am_gmm, trans_model, features,
					   acoustic_scale);
    decoder->Decode(&gmm_decodable);

    VectorFst<LatticeArc> decoded;  // linear FST.

    if ( (allow_partial || decoder->ReachedFinal())
	 && decoder->GetBestPath(&decoded) ) {
      if (!decoder->ReachedFinal())
	KALDI_WARN << "Decoder did not reach end-state, "
		   << "outputting partial traceback since --allow-partial=true";
      num_success++;
      if (!decoder->ReachedFinal())
	KALDI_WARN << "Decoder did not reach end-state, outputting partial traceback.";
      vector<int> alignment;
      vector<int> words;
      LatticeWeight weight;
      frame_count += features.NumRows();

      GetLinearSymbolSequence(decoded, &alignment, &words, &weight);

      words_writer.Write(key, words);
      if (alignment_writer.IsOpen())
	alignment_writer.Write(key, alignment);
          
      if (lattice_wspecifier != "") {
	if (acoustic_scale != 0.0) // We'll write the lattice without acoustic scaling
	  fst::ScaleLattice(fst::AcousticLatticeScale(1.0 / acoustic_scale), &decoded);
	VectorFst<CompactLatticeArc> clat;
	ConvertLattice(decoded, &clat, true);
	clat_writer.Write(key, clat);
      }
          
      if (word_syms != NULL) {
	std::cerr << key << ' ';
	for (size_t i = 0; i < words.size(); i++) {
	  string s = word_syms->Find(words[i]);
	  if (s == "")
	    KALDI_ERR << "Word-id " << words[i] <<" not in symbol table.";
	  std::cerr << s << ' ';
	}
	std::cerr << '\n';
      }
      BaseFloat like = -weight.Value1() -weight.Value2();
      tot_like += like;
      KALDI_LOG << "Log-like per frame for utterance " << key << " is "
		<< (like / features.NumRows()) << " over "
		<< features.NumRows() << " frames.";
      KALDI_VLOG(2) << "Cost for utterance " << key << " is "
		    << weight.Value1() << " + " << weight.Value2();
    } else {
      num_fail++;
      KALDI_WARN << "Did not successfully decode utterance " << key
		 << ", len = " << features.NumRows();
    }
  }

  double elapsed = timer.Elapsed();
  KALDI_LOG << "Time taken [excluding initialization] "<< elapsed
	    << "s: real-time factor assuming 100 frames/sec is "
	    << (elapsed*100.0/frame_count);
  KALDI_LOG << "Done " << num_success << " utterances, failed for "
	    << num_fail;
  KALDI_LOG << "Overall log-likelihood per frame is " << (tot_like/frame_count) << " over "
	    << frame_count<<" frames.";

  return (num_success != 0 ? 0 : 1);
}


int main(int argc, char *argv[]) {
  try {
    const char *usage =
        "Decode features using GMM-based model.\n"
        "Usage:  gmm-decode-faster [options] model-in fst-in features-rspecifier words-wspecifier [alignments-wspecifier [lattice-wspecifier]]\n"
        "Note: lattices, if output, will just be linear sequences; use gmm-latgen-faster\n"
        "  if you want \"real\" lattices.\n";
    ParseOptions po(usage);
    bool allow_partial = true;
    BaseFloat acoustic_scale = 0.1;
    
    string word_syms_filename;
    FasterDecoderOptions decoder_opts;

    bool   clg_use_gc  = false;
    int clg_gc_limit   = 1<<20LL;
    
    bool   hclg_use_gc = false;
    int hclg_gc_limit  = 1<<20LL;

    int mod_reset      = 50;
    decoder_opts.Register(&po, true);  // true == include obscure settings.
    po.Register("acoustic-scale", &acoustic_scale,
                "Scaling factor for acoustic likelihoods");
    po.Register("word-symbol-table", &word_syms_filename,
                "Symbol table for words [for debug output]");
    po.Register("allow-partial", &allow_partial,
                "Produce output even when final state was not reached");
    po.Register("clg_use_gc", &clg_use_gc,
                "Toggle garbage collection for lazy CLG.");
    po.Register("clg_gc_limit", &clg_gc_limit,
                "Set cache limit for lazy CLG.");
    po.Register("hclg_use_gc", &hclg_use_gc,
                "Toggle garbage collection for lazy HCLG.");
    po.Register("hclg_gc_limit", &hclg_gc_limit,
                "Set cache limit for lazy HCLG.");
    po.Register("mod_reset", &mod_reset,
                "Set an interval to reset the lazy FSTs." );
    po.Read(argc, argv);

    if (po.NumArgs() < 4 || po.NumArgs() > 6) {
      po.PrintUsage();
      exit(1);
    }

    string 
      model_rxfilename     = po.GetArg(1),
      fst_rxfilename       = po.GetArg(2),
      feature_rspecifier   = po.GetArg(3),
      words_wspecifier     = po.GetArg(4),
      alignment_wspecifier = po.GetOptArg(5),
      lattice_wspecifier   = po.GetOptArg(6);

    vector<string> fsts = split(fst_rxfilename, ',');
    vector<Fst<StdArc>* > components;
    switch( fsts.size() ){
      case 1:
        {
          Fst<StdArc>* hclg_fst = Fst<StdArc>::Read(fsts.at(0));
          components.push_back(hclg_fst);
          return really_decode(
              components, allow_partial, acoustic_scale,
              word_syms_filename, decoder_opts,
              model_rxfilename, feature_rspecifier,   
              words_wspecifier, alignment_wspecifier, lattice_wspecifier,
              clg_use_gc, clg_gc_limit, hclg_use_gc, hclg_gc_limit,
              mod_reset
                        );
        }
      case 2:
        {
	  /*
	  CacheAccStdOLabelLookAheadFst* hcl_fst = CacheAccStdOLabelLookAheadFst::Read(fsts.at(0));
	  Fst<StdArc>*                     g_fst = Fst<StdArc>::Read(fsts.at(1));
	  components.push_back(hcl_fst);
	  components.push_back(g_fst);

          return really_decode(
	      components, allow_partial, acoustic_scale,
              word_syms_filename, decoder_opts,
              model_rxfilename, feature_rspecifier,   
              words_wspecifier, alignment_wspecifier, lattice_wspecifier,
              clg_use_gc, clg_gc_limit, hclg_use_gc, hclg_gc_limit,
              mod_reset
		       );
	  */
	  Fst<StdArc>* h_fst   = Fst<StdArc>::Read(fsts.at(0));
	  Fst<StdArc>* clg_fst = Fst<StdArc>::Read(fsts.at(1));
	  components.push_back(h_fst);
	  components.push_back(clg_fst);

	  return really_decode(
	      components, allow_partial, acoustic_scale,
              word_syms_filename, decoder_opts,
              model_rxfilename, feature_rspecifier,   
              words_wspecifier, alignment_wspecifier, lattice_wspecifier,
              clg_use_gc, clg_gc_limit, hclg_use_gc, hclg_gc_limit,
              mod_reset
		       );
        }
      case 3:
        {
          Fst<StdArc>*     h_fst                = Fst<StdArc>::Read(fsts.at(0));
          CacheAccStdOLabelLookAheadFst* cl_fst = CacheAccStdOLabelLookAheadFst::Read(fsts.at(1));
          Fst<StdArc>*     g_fst                = Fst<StdArc>::Read(fsts.at(2));
          components.push_back(h_fst);
          components.push_back(cl_fst);
          components.push_back(g_fst);
          return really_decode(
              components, allow_partial, acoustic_scale,
              word_syms_filename, decoder_opts,
              model_rxfilename, feature_rspecifier,   
              words_wspecifier, alignment_wspecifier, lattice_wspecifier,
              clg_use_gc, clg_gc_limit, hclg_use_gc, hclg_gc_limit,
              mod_reset
                        );
        }
      default:
        {
          KALDI_ERR << "Number of component FSTs should be 1 or 3.";
          return 1;
        }
    }
    return 1;
    
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}
