// online-nnet-loglike-tranf-estimator.cc
// LICENSE-GOES-HERE
//
// Copyright 2013- Yandex LLC
// Authors: ilya@yandex-team.ru (Ilya Edrenkin),
//          jorono@yandex-team.ru (Josef Novak),
//          javi@yandex-team.ru (Javier Dieguez)
//
// \file
// The class NnetLoglikeTransfEstimator wraps something of type
// OnlineFeatInputItf, and applies loglikelihood computation.
// This version does not do splicing, as this is implicit in our
// feature chain.

#include "dcd/frontend/online-nnet-loglike-transf-estimator.h"

namespace kaldi {

void NnetLoglikeTransfEstimator::ComputeLoglikelihoods() {
  Matrix<BaseFloat> feats;
  CuMatrix<BaseFloat> cu_feats, cu_feats_transf, nnet_out;

  const uint32 num_frames = raw_feat_matrix_.size();
  feats.Resize(num_frames, raw_feat_dim_);

  feat_offset_ = first_feature_in_raw_matrix_;

  const uint32 off = 0;
  int32 r_off = 0;
  int feats_to_remove = feats.NumRows() - 2 * context_frames_;
  if (feats_to_remove < 0) feats_to_remove = 0;
  for(uint32 r = 0; r < feats.NumRows(); ++r) {
    memcpy(feats.RowData(r)+off*raw_feat_dim_, raw_feat_matrix_[r_off].Data(), sizeof(BaseFloat)*raw_feat_dim_);
    if (r<feats_to_remove) {
      first_feature_in_raw_matrix_++;
      raw_feat_matrix_.pop_front();
    }
    else {
      r_off++;
    }
  }

  cu_feats = feats;

  nnet_transf_.Feedforward(cu_feats, &cu_feats_transf);

  nnet_.Feedforward(cu_feats_transf, &nnet_out);
  pdf_prior_.SubtractOnLogpost(&nnet_out);

  //preserve old loglikes
  Matrix<float> oldlikes;
  oldlikes.Resize(loglikes_.NumRows(), loglikes_.NumCols());
  for(int i=0;i<loglikes_.NumRows();i++) {
    oldlikes.CopyRowFromVec(loglikes_.Row(i),i);
  }

  //get new ones in tmp matrix
  Matrix<float> newlikes;
  newlikes.Resize(nnet_out.NumRows(), nnet_out.NumCols());
  nnet_out.CopyToMat(&newlikes);

  //construct ALL GOOD loglikes matrix
  int bad_rows = finished_ ? 0 : context_frames_ ;
  if (bad_rows >= newlikes.NumRows()) return;
  loglikes_.Resize(newlikes.NumRows() - bad_rows, newlikes.NumCols());
  if(oldlikes.NumRows() < context_frames_) {
    for(int i=0; i<loglikes_.NumRows() ; i++) {
      loglikes_.CopyRowFromVec(newlikes.Row(i),i);
    }
  }
  else {
    for(int i=0; i<context_frames_ ; i++) {
      loglikes_.CopyRowFromVec(oldlikes.Row(oldlikes.NumRows()-context_frames_+i),i);
    }
    for(int i=context_frames_; i<loglikes_.NumRows() ; i++) {
      loglikes_.CopyRowFromVec(newlikes.Row(i),i);
    }
  }

}

void NnetLoglikeTransfEstimator::GetNextFeatures() {
  if (finished_) return; // Nothing to do.

  Matrix<BaseFloat> next_features(1, input_->Dim());

  finished_ = ! input_->Compute(&next_features);
  uint32 num_frames_fetched = next_features.NumRows();

  if(num_frames_fetched > 0) {

    for(uint32 i = 0; i < num_frames_fetched; ++i, ++current_frame_) {
      raw_feat_matrix_.push_back(Vector<BaseFloat>(next_features.Row(i)));
    }

  }
}


bool NnetLoglikeTransfEstimator::IsValidFrame (int32 frame) {
  KALDI_ASSERT(frame >= feat_offset_ &&
               "You are attempting to get expired frames.");
  if (frame < feat_offset_ + loglikes_.NumRows()) return true;
  else {
    do {
      GetNextFeatures();
    } while(!finished_ && (current_frame_ < context_frames_ + 1)); // for the first frame to be successfully computed

    if(raw_feat_matrix_.size() > 0) ComputeLoglikelihoods();

    if (frame < feat_offset_ + loglikes_.NumRows()) return true;
    else {
      if (finished_ && frame < current_frame_) return true;
      else if(finished_ && frame == current_frame_) return false;
      else {
        KALDI_WARN << "Unexpected point reached in code: "
                   << "possibly you are skipping frames?";
        return false;
      }
    }
  }
}

} // namespace kaldi


