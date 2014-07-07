// kaldi-lattice-arc-reg.cc
// Based on registration code from OpenFst
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
// Copyright 2013-2014
// /file

#include <fst/const-fst.h>
#include <fst/edit-fst.h>
#include <fst/vector-fst.h>
#include <fst/script/register.h>
#include <fst/script/fstscript.h>
#include <fst/extensions/far/farscript.h>
#include <dcd/lattice-arc.h>

using namespace fst;

namespace fst {
namespace script {
  REGISTER_FST(VectorFst, KaldiLatticeArc);
  REGISTER_FST(ConstFst, KaldiLatticeArc);
  REGISTER_FST(EditFst, KaldiLatticeArc);
  REGISTER_FST_CLASSES(KaldiLatticeArc);
  REGISTER_FST_OPERATIONS(KaldiLatticeArc);
  REGISTER_FST_FAR_OPERATIONS(KaldiLatticeArc);
  }
}

