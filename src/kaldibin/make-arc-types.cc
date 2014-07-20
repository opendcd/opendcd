// make-arc-types.cc
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
#include <iomanip>
#include "hmm/transition-model.h"
#include "hmm/hmm-utils.h"
#include "tree/context-dep.h"
#include "util/common-utils.h"
#include "fst/fstlib.h"
#include "fstext/table-matcher.h"
#include "fstext/fstext-utils.h"
#include "fstext/context-fst.h"


using namespace fst;
using namespace std;

namespace kaldi {

typedef unordered_map<pair<int32, vector<int32> >, int32,
                      HmmCacheHash> Log2PhysMapType;

struct MakeArcTypeOpts {
  MakeArcTypeOpts () 
    : prob_scale(1.0f), logical2physical(false), use_trans_ids(false) { }
  BaseFloat prob_scale;
  bool logical2physical;
  bool use_trans_ids;
};

fst::VectorFst<fst::StdArc>*
GetHmmAsFstWithLoops(std::vector<int32> phone_window,
                  const ContextDependencyInterface& ctx_dep,
                  const TransitionModel& trans_model,
                  const MakeArcTypeOpts& opts) {
  using namespace fst;

  if (static_cast<int32>(phone_window.size()) != ctx_dep.ContextWidth())
    KALDI_ERR <<"Context size mismatch, ilabel-info [from context FST is "
              <<(phone_window.size())<<", context-dependency object "
        "expects "<<(ctx_dep.ContextWidth());

  int P = ctx_dep.CentralPosition();
  int32 phone = phone_window[P];
  KALDI_ASSERT(phone != 0);

  const HmmTopology &topo = trans_model.GetTopo();
  const HmmTopology::TopologyEntry &entry  = topo.TopologyForPhone(phone);

  VectorFst<StdArc> *ans = new VectorFst<StdArc>;

  // Create a mini-FST with a superfinal state [in case we have emitting
  // final-states, which we usually will.]
  typedef StdArc Arc;
  typedef Arc::Weight Weight;
  typedef Arc::StateId StateId;
  typedef Arc::Label Label;

  std::vector<StateId> state_ids;
  for (size_t i = 0; i < entry.size(); i++)
    state_ids.push_back(ans->AddState());
  assert(state_ids.size() > 1);  // Or invalid topology entry.
  ans->SetStart(state_ids[0]);
  StateId final = state_ids.back();
  ans->SetFinal(final, Weight::One());

  for (int32 hmm_state = 0;
       hmm_state < static_cast<int32>(entry.size());
       hmm_state++) {
    int32 pdf_class = entry[hmm_state].pdf_class, pdf;
    if (pdf_class == kNoPdf) 
      pdf = kNoPdf;  // nonemitting state; 
    //not generally used.
    else {
      bool ans = ctx_dep.Compute(phone_window, pdf_class, &pdf);
      KALDI_ASSERT(ans && "Context-dependency computation failed.");
    }
    int32 trans_idx;
    for (trans_idx = 0;
        trans_idx < static_cast<int32>(entry[hmm_state].transitions.size());
        trans_idx++) {
      BaseFloat log_prob;
      Label label = kNoLabel;
      int32 dest_state = entry[hmm_state].transitions[trans_idx].first;
      bool is_self_loop = (dest_state == hmm_state);
      if (pdf_class == kNoPdf) {
        // no pdf, hence non-estimated probability.  very unusual case.
        // [would not happen with normal topology] .  There is 
        // no transition-state involved in this case.
        KALDI_ASSERT(!is_self_loop);
        log_prob = log(entry[hmm_state].transitions[trans_idx].second);
        label = 0;
      } else {  // normal probability.
        int32 trans_state =
            trans_model.TripleToTransitionState(phone, hmm_state, pdf);
        int32 trans_id =
            trans_model.PairToTransitionId(trans_state, trans_idx);
        log_prob = opts.prob_scale * trans_model.GetTransitionLogProb(trans_id);
        // log_prob is a negative number (or zero)...
        //int32 hmm_state = trans_model.TransitionIdToHmmState(trans_id);
        
        label = opts.use_trans_ids ? trans_id : pdf + 1;
      }
      ans->AddArc(state_ids[hmm_state],
                  Arc(label, label, Weight(-log_prob), state_ids[dest_state]));
    }
  }
  return ans;
}

template<class Arc>
void MakeArcTypes(const std::vector<std::vector<int32> > &ilabel_info,
                                   const ContextDependencyInterface &ctx_dep,
                                   const TransitionModel &trans_model,
                                   std::vector<int32> *disambig_syms_left,
                                   vector<VectorFst<Arc>* >* fsts,
                                   vector<int>* log2phys,
                                   const MakeArcTypeOpts& opts) { 
  typedef typename Arc::Weight Weight;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  assert(ilabel_info.size() >= 1 && ilabel_info[0].size() == 0);
  // make sure that eps == eps.
  // "cache" is an optimization that prevents GetHmmAsFst repeating work
  // unnecessarily.
  Log2PhysMapType log2physmap;
  fsts->clear();
  fsts->reserve(ilabel_info.size());
  std::vector<int32> phones = trans_model.GetPhones();

  VectorFst<Arc>* epsfst = new VectorFst<Arc>;
  epsfst->AddState();
  epsfst->AddState();
  epsfst->SetStart(0);
  epsfst->SetFinal(1, Weight::One());
  epsfst->AddArc(0, Arc(0, 0, 0, 1));
  fsts->push_back(epsfst);
  assert(disambig_syms_left != 0);
  disambig_syms_left->clear();
  log2phys->push_back(0);
  int32 first_disambig_sym = trans_model.NumTransitionIds() + 1;  
  // First disambig symbol we can have on the input side.
  int32 next_disambig_sym = first_disambig_sym;

  if (ilabel_info.size() > 0)
    assert(ilabel_info[0].size() == 0);  // make sure epsilon is epsilon...

  for (int32 j = 1; j < static_cast<int32>(ilabel_info.size()); j++) {  
    // zero is eps.
    assert(!ilabel_info[j].empty());
    if (ilabel_info[j].size() == 1 &&
      ilabel_info[j][0] <= 0) {  // disambig symbol

      // disambiguation symbol.
      int32 disambig_sym_left = next_disambig_sym++;
      disambig_syms_left->push_back(disambig_sym_left);
      // get acceptor with one path with "disambig_sym" on it.
      VectorFst<Arc> *fst = new VectorFst<Arc>;
      fst->AddState();
      fst->AddState();
      fst->SetStart(0);
      fst->SetFinal(1, Weight::One());
      fst->AddArc(0, Arc(disambig_sym_left, disambig_sym_left, 
            Weight::One(), 1));
      //Map the disam to an epsilon transition
      log2phys->push_back(0);
      fsts->push_back(fst);
      KALDI_VLOG(2) << "disambiguation symbol " <<  disambig_sym_left;
    } else {  // Real phone-in-context.
      int P = ctx_dep.CentralPosition();
      std::vector<int32> phone_window = ilabel_info[j];
      int phone = phone_window[P];
      const HmmTopology &topo = trans_model.GetTopo();
      // vector of the pdfs, indexed by pdf-class (pdf-classes must start 
      //from zero  and be contiguous).
      std::vector<int32> pdfs(topo.NumPdfClasses(phone));
      for (int32 pdf_class = 0;
           pdf_class < static_cast<int32>(pdfs.size());
           pdf_class++) {
        if (! ctx_dep.Compute(phone_window, pdf_class, &(pdfs[pdf_class]))) {
          std::ostringstream ctx_ss;
          for (size_t i = 0; i < phone_window.size(); i++)
            ctx_ss << phone_window[i] << ' ';
          KALDI_ERR << "GetHmmAsFst: context-dependency object could not produce "
                    << "an answer: pdf-class = " << pdf_class << " ctx-window = "
                    << ctx_ss.str() << ".  This probably points "
              "to either a coding error in some graph-building process, "
              "a mismatch of topology with context-dependency object, the "
              "wrong FST being passed on a command-line, or something of "
              " that general nature.";
        }
      }

      pair<int32, vector<int32> > cache_index(phone, pdfs);
      if (log2physmap.find(cache_index) != log2physmap.end()) {
        log2phys->push_back(log2physmap[cache_index]);
      } else {
        VectorFst<Arc>* fst =  GetHmmAsFstWithLoops(phone_window,
                                         ctx_dep,
                                         trans_model,
                                         opts);
        if (opts.logical2physical)
          log2physmap[cache_index] = fsts->size();
        log2phys->push_back(fsts->size());
        fsts->push_back(fst);
      }
    }
  }
    
  KALDI_VLOG(2) << "Number of input labels " <<  ilabel_info.size();
  KALDI_VLOG(2) << "Number of logical phones " << log2phys->size();
  KALDI_VLOG(2) << "Number of physical phones " << fsts->size();
  KALDI_VLOG(2) << "Number of disamb symbols " << disambig_syms_left->size();
  vector<int> uniq = *log2phys;
  SortAndUniq(&uniq);
  KALDI_VLOG(2) << "Unique phone ids " << uniq.size();
  
  //return ans;
}
}

template<class Arc>
bool WriteArcTypesAsFsts(const string& path,
                         const vector<VectorFst<Arc>*>& fsts) {
  ofstream ofs(path.c_str(), ostream::binary);
  if (!ofs.is_open())
    return false;
  int numfsts = fsts.size();
  WriteType(ofs, numfsts);
  for (int i = 0; i != fsts.size(); ++i)
    fsts[i]->Write(ofs, FstWriteOptions(path));
  return true;
}

template<class Arc>
bool WriteArcTypesAsFar(const string& path,
                        const vector<VectorFst<Arc>*>& fsts) {
  return false;
}

template<class Arc>
bool WriteArcTypesAsTxt(const string& path,
                        const vector<VectorFst<Arc>*>& fsts) {
  ofstream ofs(path.c_str());
  if (!ofs.is_open())
    return false;
  int numfsts = fsts.size();
  ofs << "# " << numfsts << endl;
  for (int i = 0; i != fsts.size(); ++i) {
    const Fst<Arc> &fst = *fsts[i];
    for (StateIterator<Fst<Arc> > siter(fst); !siter.Done(); siter.Next()) {
    }
  }
  return true;
}



DECLARE_int32(v);

int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    typedef kaldi::int32 int32;
    using fst::SymbolTable;
    using fst::VectorFst;
    using fst::StdArc;

    const char *usage =
        "Make arc types for the dynamic expansion inside the decoder phones, \n"
        " without self-loops [use add-self-loops to add them]\n"
        "Usage:   make-arc-types ilabel-info-file tree-file"
        "transition-gmm/acoustic-model [arcs-far-out]\n"
        "e.g.: \n"
        " make-arc-types ilabels 1.tree 1.mdl > arcs.far\n";
    ParseOptions po(usage);

    MakeArcTypeOpts opts;
    string disambig_out_filename;
    string type = "fsts";
    po.Register("disambig-syms-out", &disambig_out_filename, "List of" 
        "disambiguation symbols on input of H [to be output from this program]");
    po.Register("use_trans_ids", &opts.use_trans_ids, "Use transition ids"
        "for HMM input arc labels.  Required for native Kaldi compatibility.");
    po.Register("type", &type, "Output format when writing the arcs");
    po.Read(argc, argv);

    if (po.NumArgs() < 3 || po.NumArgs() > 5) {
      po.PrintUsage();
      exit(1);
    }
    
    std::string ilabel_info_filename = po.GetArg(1);
    std::string tree_filename = po.GetArg(2);
    std::string model_filename = po.GetArg(3);
    std::string arcs_out_filename;
    string log2phys_out_filename;
    
    if (po.NumArgs() >= 4) 
      arcs_out_filename = po.GetArg(4);
    
    if (po.NumArgs() >= 5) {
      log2phys_out_filename = po.GetArg(5);
      opts.logical2physical = true;
    }

    if (arcs_out_filename == "-") 
      arcs_out_filename = "/dev/stdout";

    if (log2phys_out_filename == "-") {
      if (arcs_out_filename == "/dev/stdout") 
        KALDI_ERR << "Attempting to write arcs and mapping table to stdout";
      else
        log2phys_out_filename = "/dev/stdout";
    }

    std::vector<std::vector<int32> > ilabel_info;
    {
      bool binary_in;
      Input ki(ilabel_info_filename, &binary_in);
      fst::ReadILabelInfo(ki.Stream(), binary_in, &ilabel_info);
    }

    ContextDependency ctx_dep;
    ReadKaldiObject(tree_filename, &ctx_dep);

    TransitionModel trans_model;
    ReadKaldiObject(model_filename, &trans_model);

    std::vector<int32> disambig_syms_out;

    vector<StdVectorFst*> fsts;
    vector<int> log2phys;
    MakeArcTypes(ilabel_info, ctx_dep, trans_model, &disambig_syms_out,
                 &fsts, &log2phys, opts);

    if (type == "fsts")
      WriteArcTypesAsFsts(arcs_out_filename, fsts);
    else if (type == "far")
      WriteArcTypesAsFar(arcs_out_filename, fsts);
    else if (type == "txt")
      WriteArcTypesAsTxt(arcs_out_filename, fsts);
    else
      KALDI_ERR << "Unknown type " << type;

    if (!log2phys_out_filename.empty()) {
      ofstream ofs(log2phys_out_filename.c_str());
      if (ofs.is_open()) {
        for (int i = 0; i != log2phys.size(); ++i)
          ofs << i << " " << log2phys[i] << endl;
      }
    }
    if (disambig_out_filename != "") {  // if option specified..
      if (disambig_out_filename == "-")
        disambig_out_filename = "";
      if (! WriteIntegerVectorSimple(disambig_out_filename, disambig_syms_out))
        KALDI_ERR << "Could not write disambiguation symbols to "
                   << (disambig_out_filename == "" ?
                       "standard output" : disambig_out_filename);
    }
    return 0;
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}

