// far-filter.cc
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
// Copyright 2013-2014 Paul R. Dixon
// \file
// Apply a pipeline of commangs to every in FST in the FAR archive
// Modified version of lattice-filter


#include <exec-stream/exec-stream.h>
#include <fst/vector-fst.h>
#include <fst/extensions/far/far.h>

using namespace std;
using namespace fst;

//From kaldi text-utils
void SplitStringToVector(const std::string &full, const char *delim,
                         bool omit_empty_strings,
                         std::vector<std::string> *out) {
  size_t start = 0, found = 0, end = full.size();
  out->clear();
  while (found != std::string::npos) {
    found = full.find_first_of(delim, start);
    // start != end condition is for when the delimiter is at the end
    if (!omit_empty_strings || (found != start && start != end))
      out->push_back(full.substr(start, found - start));
    start = found + 1;
  }
}

template<class Arc>
void ProcessFinal(exec_stream_t* es, fst::VectorFst<Arc>* out) {
  fst::VectorFst<Arc>* tmp = 
    fst::VectorFst<Arc>::Read(es->out(), fst::FstReadOptions());
  *out = *tmp;
  delete tmp;
}


void ProcessFinal(exec_stream_t* es, ostream* out) {
  for (string s; getline(es->out(), s); ) 
    (*out) << s << endl;
}

template <class Arc, class Out>
void ProcessFstImpl(const fst::Fst<Arc>& fst, const vector<string>& cmds, 
                    int i, exec_stream_t* est, Out* out) {
  
  //Break the command and arguments apart and create
  //a new string of just the arguments
  vector<string> args;
  SplitStringToVector(cmds[i], " ", true, &args);
  stringstream ss;
  for (int j = 1; j < args.size(); ++j) {
    ss << args[j];
    if (j < args.size() - 1)
      ss << " ";
  }

  VLOG(1) << "Launching command " << args[0] << " : " << ss.str(); 
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
  //Final
  if (i == cmds.size() - 1) {
    ProcessFinal(&es, out);
 } else {
    ProcessFstImpl(fst, cmds, i + 1, &es, out);
  }
}


template <class Arc>
void ProcessFstImplOld(const fst::Fst<Arc>& fst, const vector<string>& cmds, 
                    int i, exec_stream_t* est, fst::VectorFst<Arc>* out) {
  
  //Break the command and arguments apart and create
  //a new string of just the arguments
  vector<string> args;
  SplitStringToVector(cmds[i], " ", true, &args);
  stringstream ss;
  for (int j = 1; j < args.size(); ++j) {
    ss << args[j];
    if (j < args.size() - 1)
      ss << " ";
  }

  VLOG(1) << "Launching command " << args[0] << " : " << ss.str(); 
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
    fst::VectorFst<Arc>* tmp = fst::VectorFst<Arc>::Read(es.out(),
        fst::FstReadOptions());
    *out = *tmp;
    delete tmp;
  } else {
    ProcessFstImpl(fst, cmds, i + 1, &es, out);
  }
}

template <class Arc>
void ProcessFstOld(const string& cmd,
    const fst::Fst<Arc>& fst, fst::VectorFst<Arc>* out) {
  vector<string> cmds;
  SplitStringToVector(cmd, "|", true, &cmds);
  ProcessFstImpl(fst, cmds, 0, 0, out);
}

template <class Arc, class Out>
void ProcessFst(const string& cmd,
    const fst::Fst<Arc>& fst, Out* out) {
  vector<string> cmds;
  SplitStringToVector(cmd, "|", true, &cmds);
  ProcessFstImpl(fst, cmds, 0, 0, out);
}



DEFINE_bool(text, false, "Last command in the pipeline write text output");
int main(int argc, char **argv) {
  string usage = "Composes two FSTs.\n\n  Usage: ";
  usage += argv[0];
  usage += " in.far [out.far]\n";

  std::set_new_handler(FailedNewHandler);
  SET_FLAGS(usage.c_str(), &argc, &argv, true);
  if (argc < 2 && argc > 4) {
    ShowUsage();
    return 1;
  }

  string cmd(argv[1]);
  FarReader<StdArc> *reader = FarReader<StdArc>::Open((argc > 2 &&
      (strcmp(argv[2], "-") !=0 )) ? argv[2] : "");

  if (FLAGS_text) {
    ofstream strm((argc > 3 &&
        (strcmp(argv[3], "-") !=0 )) ? argv[3] : "/dev/stdout");

    for (; !reader->Done(); reader->Next()) {
      const StdFst& fst = reader->GetFst();
      strm << "[" << reader->GetKey() << "]" << endl;
      ProcessFst(cmd, fst, &strm);
    }
  } else {
    FarWriter<StdArc> *writer = FarWriter<StdArc>::Create((argc > 2 &&
        (strcmp(argv[3], "-") !=0 )) ? argv[3] : "");
    for (; !reader->Done(); reader->Next()) {
      StdVectorFst ofst;
      const StdFst& fst = reader->GetFst();
      ProcessFst(cmd, fst, &ofst);
      writer->Add(reader->GetKey(), ofst);
    }
    delete writer;
  }
  delete reader;
  return 0;
}

