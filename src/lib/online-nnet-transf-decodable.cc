// online-nnet-tranf-decodable.cc
// 
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
// The class OnlineDecodableTransfNnet wraps something of type
// OnlineLoglikes*Estimator.  It also applies an optional acoustic
// scale factor.
// This version does not do splicing, as this is implicit in our
// feature chain.

#include "dcd/frontend/online-nnet-transf-decodable.h"

namespace kaldi {

BaseFloat OnlineDecodableTransfNnet::LogLikelihood(int32 frame, int32 index) {
  // For the new decoder we just need the tid.  It has already been mapped.
  // TODO: Should probably be an option connected to the constructor...
  return ac_scale_ * input_loglikes_->GetLogLikelihood(frame, index);
}
  
bool OnlineDecodableTransfNnet::IsLastFrame(int32 frame) {
  return !input_loglikes_->IsValidFrame(frame+1);
}
  
int32 OnlineDecodableTransfNnet::NumIndices() {
  return trans_model_.NumTransitionIds();
}


} // namespace kaldi




