//lattice-filter.cc
// Copyright 2013 Paul R. Dixon

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
#include "fstext/fstext-lib.h"
#include "lat/kaldi-lattice.h"
#include "lat/lattice-functions.h"
#include "exec-stream.h"

using namespace std;
using kaldi::Lattice;

template <class Arc>
void ProcessLatticeImpl(const fst::VectorFst<Arc>& fst,  const vector<string>& cmds, int i, 
    exec_stream_t* est, fst::VectorFst<Arc>* out) {
  
  //Break the command and arguments apart and create
  //a new string of just the arguments
  vector<string> args;
  kaldi::SplitStringToVector(cmds[i], " ", true, &args);
  stringstream ss;
  for (int j = 1; j < args.size(); ++j) {
    ss << args[j];
    if (j < args.size() - 1)
      ss << " ";
  }

  KALDI_VLOG(2) << "Launching command " << args[0] << " : " << ss.str(); 
  //Launch the command
  exec_stream_t es(args[0], ss.str()); 
  es.set_binary_mode(exec_stream_t::s_all);

  if (i == 0) {
    //This is the very first command in th pipeline so
    //write in the input Fst
    fst.Write(es.in(), fst::FstWriteOptions()); 
  } else {
    //Not sure why this doesn work?
    //es.in() << est->out().rdbuf();
    for (char c; est->out().get(c);)
      es.in().write(&c, 1);
  }
  
  for (string s; getline(es.err(), s); )
    cerr << s << endl;
  if (i == cmds.size() - 1) {
    fst::VectorFst<Arc>* tmp = fst::VectorFst<Arc>::Read(es.out(), fst::FstReadOptions());
    *out = *tmp;
    delete tmp;
  } else {
    ProcessLatticeImpl(fst, cmds, i + 1, &es, out);
  }
}

template <class Arc>
void ProcessLattice(const string& cmd,
    const fst::VectorFst<Arc>& fst, fst::VectorFst<Arc>* out) {
  vector<string> cmds;
  kaldi::SplitStringToVector(cmd, "|", true, &cmds);
  ProcessLatticeImpl(fst, cmds, 0, 0, out);
}



int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    typedef kaldi::int32 int32;
    typedef kaldi::int64 int64;
    
    using fst::VectorFst;
    using fst::StdArc;
    typedef StdArc::StateId StateId;
    
    const char *usage = "Filter a set of lattices\n";
    
    ParseOptions po(usage);
    bool compact = false;
    po.Register("compact", &compact, "Gather information as a compact lattice");
    po.Read(argc, argv);
    if (po.NumArgs() != 3) {
      po.PrintUsage();
      exit(1);
    }

    std::string cmd = po.GetArg(1);
    std::string lats_rspecifier = po.GetArg(2);
    std::string lats_wspecifier = po.GetArg(3);

    SequentialCompactLatticeReader clat_reader(lats_rspecifier);

    if (compact) { 
      CompactLatticeWriter clat_writer(lats_wspecifier);
      for (; !clat_reader.Done(); clat_reader.Next()) {
        std::string key = clat_reader.Key();
        KALDI_VLOG(1) << "Processing lattice : " << key;
        CompactLattice out;
        ProcessLattice(cmd, clat_reader.Value(), &out);
        clat_writer.Write(key, out);
      }
    } else {
      LatticeWriter lat_writer(lats_wspecifier);
      for (; !clat_reader.Done(); clat_reader.Next()) {
        std::string key = clat_reader.Key();
        KALDI_VLOG(1) << "Processing lattice : " << key;
        Lattice lat;
        ConvertLattice(clat_reader.Value(), &lat);
        Lattice out;
        ProcessLattice(cmd, lat, &out);
        lat_writer.Write(key, out);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}
