// online-nnet-tranf-decodable.cc
// LICENSE-GOES-HERE
//
// Copyright 2013- Yandex LLC
// Authors: ilya@yandex-team.ru (Ilya Edrenkin),
//          jorono@yandex-team.ru (Josef Novak)
//
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




