/// far-to-lattice.cc
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
//

#include <fst/script/arg-packs.h>
#include <fst/script/script-impl.h>
#include <fst/extensions/far/far.h>
#include <fst/extensions/far/main.h>
#include <fst/extensions/far/farscript.h>

using namespace std;

namespace fst {
namespace script {
typedef args::Package<const string&, const string&, const string&>
  ToKaldiLatticeArgs;

template<class Arc>
void ToKaldiLattice(ToKaldiLatticeArgs* args) {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;
  LOG(INFO) << args->arg2;
  FarReader<Arc>* reader = FarReader<Arc>::Open(args->arg1);
  if (!reader) return;

  for (; !reader->Done(); reader->Next()) {
    string key = reader->GetKey();
    const Fst<Arc> &fst = reader->GetFst();
  }
  delete reader;
}

void ToKaldiLattice(const string& ifilename, const string& arc_type, 
    const string& ofilename) { 
  ToKaldiLatticeArgs args(ifilename, arc_type, ofilename);
  Apply<Operation<ToKaldiLatticeArgs> >("ToKaldiLattice", arc_type, &args);
}

REGISTER_FST_OPERATION(ToKaldiLattice, StdArc, ToKaldiLatticeArgs);
REGISTER_FST_OPERATION(ToKaldiLattice, LogArc, ToKaldiLatticeArgs);

}  // namespace script
}  // namespace fst


using namespace fst;
int main(int argc, char **argv) {
  namespace s = fst::script;

  string usage = "Convert OpenFst far file to Kaldi Lattice table.\n\n  Usage: ";
  usage += argv[0];
  usage += " in.fst [out.text]\n";

  std::set_new_handler(FailedNewHandler);
  SetFlags(usage.c_str(), &argc, &argv, true);
  if (argc > 3) {
    ShowUsage();
    return 1;
  }

  string ifilename = argv[1];
  string ofilename = argv[2];
  s::ToKaldiLattice(ifilename, fst::LoadArcTypeFromFar(ifilename), ofilename);
  return 0;
}
