// bin/add-self-loops.cc
// Copyright 2009-2011 Microsoft Corporation

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

#include "hmm/transition-model.h"
#include "hmm/hmm-utils.h"
#include "tree/context-dep.h"
#include "util/common-utils.h"
#include "fst/fstlib.h"
#include "fstext/table-matcher.h"
#include "fstext/fstext-utils.h"
#include "fstext/context-fst.h"

/** @brief Add self-loops and transition probabilities to transducer, expanding to transition-ids.
*/

namespace kaldi {
  class TidToTstateMapper {
  public:
    // Function object used in MakePrecedingInputSymbolsSameClass and
    // MakeFollowingInputSymbolsSameClass (as called by AddSelfLoopsBefore
    // and AddSelfLoopsAfter).  It maps transition-ids to transition-states
    // (and -1 to -1, 0 to 0 and disambiguation symbols to 0).  It also
    // checks that there are no self-loops in the graph (i.e. in the labels
    // it is called with).  This is just a convenient place to put this check.

    // This maps valid transition-ids to transition states, maps kNoLabel to -1, and
    // maps all other symbols (i.e. epsilon symbols and disambig symbols) to zero.
    // Its point is to provide an equivalence class on labels that's relevant to what
    // the self-loop will be on the following (or preceding) state.
    TidToTstateMapper(const TransitionModel &trans_model,
                      const std::vector<int32> &disambig_syms):
        trans_model_(trans_model),
        disambig_syms_(disambig_syms) { }

    typedef int32 Result;
    int32 operator() (int32 label) const {
      if (label == static_cast<int32>(fst::kNoLabel)) return -1;  // -1 -> -1
      else if (label >= 1 && label <= trans_model_.NumTransitionIds()) {
        if (trans_model_.IsSelfLoop(label))
          KALDI_ERR << "AddSelfLoops: graph already has self-loops.";
        return trans_model_.TransitionIdToTransitionState(label);
      } else {  // 0 or (presumably) disambiguation symbol.  Map to zero
        if (label != 0)
          assert(std::binary_search(disambig_syms_.begin(), disambig_syms_.end(), label));  // or invalid label
        return 0;
      }
    }

  private:
    const TransitionModel &trans_model_;
    const std::vector<int32> &disambig_syms_;  // sorted.
  };
}


int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    typedef kaldi::int32 int32;
    using fst::SymbolTable;
    using fst::VectorFst;
    using fst::StdArc;
    const char *usage =
        "Add self-loops and transition probabilities to transducer, expanding to transition-ids\n"
        "Usage:   add-self-loops [options] transition-gmm/acoustic-model [fst-in] [fst-out]\n"
        "e.g.: \n"
        " add-self-loops --self-loop-scale=0.1 1.mdl < HCLG_noloops.fst > HCLG_full.fst\n";

    BaseFloat self_loop_scale = 1.0;
    bool reorder = true;
    std::string disambig_in_filename;

    ParseOptions po(usage);
    po.Register("self-loop-scale", &self_loop_scale, "Scale for self-loop probabilities relative to LM.");
    po.Register("disambig-syms", &disambig_in_filename, "List of disambiguation symbols on input of fst-in [input file]");
    po.Register("reorder", &reorder, "If true, reorder symbols for more decoding efficiency");
    po.Read(argc, argv);

    if (po.NumArgs() < 1 || po.NumArgs() > 3) {
      po.PrintUsage();
      exit(1);
    }

    std::string model_in_filename = po.GetArg(1);
    std::string fst_in_filename = po.GetOptArg(2);
    if (fst_in_filename == "-") fst_in_filename = "";
    std::string fst_out_filename = po.GetOptArg(3);
    if (fst_out_filename == "-") fst_out_filename = "";


    std::vector<int32> disambig_syms_in;
    if (disambig_in_filename != "") {
      if (disambig_in_filename == "-") disambig_in_filename = "";
      if (!ReadIntegerVectorSimple(disambig_in_filename, &disambig_syms_in))
        KALDI_ERR << "add-self-loops: could not read disambig symbols from "
                   <<(disambig_in_filename == "" ?
                      "standard input" : disambig_in_filename);
    }

    TransitionModel trans_model;
    ReadKaldiObject(model_in_filename, &trans_model);


    fst::VectorFst<fst::StdArc> *fst =
        fst::VectorFst<fst::StdArc>::Read(fst_in_filename);
    if (!fst)
      KALDI_ERR << "add-self-loops: error reading input FST.";
    // Duplicate states as necessary so that each state has at most one self-loop
    // on it.
    TidToTstateMapper f(trans_model, disambig_syms_in);
    MakeFollowingInputSymbolsSameClass(true, fst, f);

    if (! fst->Write(fst_out_filename) )
      KALDI_ERR << "add-self-loops: error writing FST to "
                 << (fst_out_filename == "" ?
                     "standard output" : fst_out_filename);

    delete fst;
    return 0;
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}

