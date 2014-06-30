// lat/sausages.h

// Copyright 2012  Johns Hopkins University (Author: Daniel Povey)

// See ../../COPYING for clarification regarding multiple authors
//
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
#ifndef DCD_LAT_SAUSAGES_NOTIME_H__
#define DCD_LAT_SAUSAGES_NOTIME_H__

#include <vector>
#include <map>
#include <fst/fst.h>
#include <fst/map.h>
#include <fst/topsort.h>
#include <fst/vector-fst.h>

#include <dcd/lattice-arc.h>

template <class T>
inline void hash_combine(std::size_t & seed, const T & v)
{
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
  template<typename S, typename T> struct hash<pair<S, T>>
  {
    inline size_t operator()(const pair<S, T> & v) const
    {
      size_t seed = 0;
      ::hash_combine(seed, v.first);
      ::hash_combine(seed, v.second);
      return seed;
    }
  };
}

namespace fst {

/// The implementation of the Minimum Bayes Risk decoding method described in
///  "Minimum Bayes Risk decoding and system combination based on a recursion for
///  edit distance", Haihua Xu, Daniel Povey, Lidia Mangu and Jie Zhu, Computer
///  Speech and Language, 2011
/// This is a slightly more principled way to do Minimum Bayes Risk (MBR) decoding
/// than the standard "Confusion Network" method.  Note: MBR decoding aims to
/// minimize the expected word error rate, assuming the lattice encodes the
/// true uncertainty about what was spoken; standard Viterbi decoding gives the
/// most likely utterance, which corresponds to minimizing the expected sentence
/// error rate.
///
/// In addition to giving the MBR output, we also provide a way to get a
/// "Confusion Network" or informally "sausage"-like structure.  This is a
/// linear sequence of bins, and in each bin, there is a distribution over
/// words (or epsilon, meaning no word).  This is useful for estimating
/// confidence.  Note: due to the way these sausages are made, typically there
/// will be, between each bin representing a high-confidence word, a bin
/// in which epsilon (no word) is the most likely word.  Inside these bins
/// is where we put possible insertions. 


/// This class does the word-level Minimum Bayes Risk computation, and gives you
/// either the 1-best MBR output together with the expected Bayes Risk,
/// or a sausage-like structure.


#ifndef DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131e-16
#endif
#ifndef FLT_EPSILON
#define FLT_EPSILON 1.19209290e-7f
#endif

static const double kMinLogDiffDouble = std::log(DBL_EPSILON);  // negative!
static const float kMinLogDiffFloat = std::log(FLT_EPSILON);  // negative!
const double kLogZeroDouble = -std::numeric_limits<double>::infinity();

inline double LogAdd(double x, double y) {
  double diff;
  if (x < y) {
    diff = x - y;
    x = y;
  } else {
    diff = y - x;
  }
  // diff is negative.  x is now the larger one.

  if (diff >= kMinLogDiffDouble) {
    double res;
#ifdef _MSC_VER
    res = x + log(1.0 + exp(diff));
#else
    res = x + log1p(exp(diff));
#endif
    return res;
  } else {
    return x;  // return the larger one.
  }
}


inline float LogAdd(float x, float y) {
  float diff;
  if (x < y) {
    diff = x - y;
    x = y;
  } else {
    diff = y - x;
  }
  // diff is negative.  x is now the larger one.

  if (diff >= kMinLogDiffFloat) {
    float res;
#ifdef _MSC_VER
    res = x + logf(1.0 + expf(diff));
#else
    res = x + log1pf(expf(diff));
#endif
    return res;
  } else {
    return x;  // return the larger one.
  }
}

DEFINE_double(am_scale, 1.0, "");
DEFINE_double(lm_scale, 1.0, "");
template<>
struct WeightConvert<KaldiLatticeWeight, TropicalWeight> {
  TropicalWeight operator()(const KaldiLatticeWeight& w) const {
    return w ==  KaldiLatticeWeight::Zero() ? TropicalWeight::Zero() :
      TropicalWeight(FLAGS_am_scale * w.Value1() + FLAGS_lm_scale * w.Value2());
  }
};

template<class Arc, class I>
bool GetLinearSymbolSequence(const Fst<Arc> &fst,
                             vector<I> *isymbols_out,
                             vector<I> *osymbols_out,
                             typename Arc::Weight *tot_weight_out) {
  typedef typename Arc::Label Label;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;

  Weight tot_weight = Weight::One();
  vector<I> ilabel_seq;
  vector<I> olabel_seq;

  StateId cur_state = fst.Start();
  if (cur_state == kNoStateId) {  // empty sequence.
    if (isymbols_out != NULL) isymbols_out->clear();
    if (osymbols_out != NULL) osymbols_out->clear();
    if (tot_weight_out != NULL) *tot_weight_out = Weight::Zero();
    return true;
  }
  while (1) {
    Weight w = fst.Final(cur_state);
    if (w != Weight::Zero()) {  // is final..
      tot_weight = Times(w, tot_weight);
      if (fst.NumArcs(cur_state) != 0) return false;
      if (isymbols_out != NULL) *isymbols_out = ilabel_seq;
      if (osymbols_out != NULL) *osymbols_out = olabel_seq;
      if (tot_weight_out != NULL) *tot_weight_out = tot_weight;
      return true;
    } else {
      if (fst.NumArcs(cur_state) != 1) return false;

      ArcIterator<Fst<Arc> > iter(fst, cur_state);  // get the only arc.
      const Arc &arc = iter.Value();
      tot_weight = Times(arc.weight, tot_weight);
      if (arc.ilabel != 0) ilabel_seq.push_back(arc.ilabel);
      if (arc.olabel != 0) olabel_seq.push_back(arc.olabel);
      cur_state = arc.nextstate;
    }
  }
}

template<class Arc>
class MinimumBayesRiskNoTime {
  typedef Fst<Arc> FST;
  
  struct MbrArc {
    int32 word;
    int32 start_node;
    int32 end_node;
    float loglike;
  };
 public:
  typedef std::unordered_map<std::pair<int, int>, float> EditMatrix; 
  const EditMatrix& edit_matrix_;
  /// Initialize with compact lattice-- any acoustic scaling etc., is assumed
  /// to have been done already.
  /// This does the whole computation.  You get the output with
  /// GetOneBest(), GetBayesRisk(), and GetSausageStats().
  MinimumBayesRiskNoTime(const Fst<Arc>& ifst, const EditMatrix& edit_matrix = EditMatrix()) 
  : edit_matrix_(edit_matrix) {
    VectorFst<Arc> lat(ifst);
    Map(&lat, SuperFinalMapper<Arc>());
    // Topologically sort the lattice, if not already sorted.
    uint64 props = lat.Properties(fst::kFstProperties, false);
    if (!(props & fst::kTopSorted)) {
      if (fst::TopSort(&lat) == false)
        FSTERROR() << "Cycles detected in lattice.";
    }
    
    // Now we convert the information in "clat" into a special internal
    // format (pre_, post_ and arcs_) which allows us to access the
    // arcs preceding any given state.
    // Note: in our internal format the states will be numbered from 1,
    // which involves adding 1 to the OpenFst states.
    int32 N = lat.NumStates();
    pre_.resize(N + 1);

    // Careful: "Arc" is a class-member struct, not an OpenFst type of arc as one
    // would normally assume.
    WeightConvert<typename Arc::Weight, TropicalWeight> converter;
    for (int32 n = 1; n <= N; n++) {
      for (fst::ArcIterator<FST> aiter(lat, n - 1);
           !aiter.Done();
           aiter.Next()) {
        const Arc &carc = aiter.Value();
        MbrArc arc; // in our local format.
        arc.word = carc.ilabel; // == carc.olabel
        arc.start_node = n;
        arc.end_node = carc.nextstate + 1; // convert to 1-based.
        arc.loglike = -converter(carc.weight).Value();
        // loglike: sum graph/LM and acoustic cost, and negate to
        // convert to loglikes.  We assume acoustic scaling is already done.

        pre_[arc.end_node].push_back(arcs_.size()); // record index of this arc.
        arcs_.push_back(arc);
      }
    }

    // We don't need to look at clat.Start() or clat.Final(state):
    // we know clat.Start() == 0 since it's topologically sorted,
    // and clat.Final(state) is Zero() except for One() at the last-
    // numbered state, thanks to CreateSuperFinal and the topological
    // sorting.

    { // Now set R_ to one best in the FST.
      VectorFst<Arc>  fst_shortest_path;
      ShortestPath(lat, &fst_shortest_path); // take shortest path of FST.
      vector<int32> alignment, words;
      typename Arc::Weight weight;
      
      GetLinearSymbolSequence(fst_shortest_path, &alignment, &words, &weight);
      R_ = words;
      L_ = 0.0; // Set current edit-distance to 0 [just so we know
      // when we're on the 1st iter.]
    }
    
    MbrDecode();
 
  }
  // it will just use the MAP recognition output, but will get the MBR stats for things
  // like confidences.
  
  const std::vector<int32> &GetOneBest() const { // gets one-best (with no epsilons)
    return R_;
  }

  void GetOneBest(StdMutableFst* ofst) {
    ofst->DeleteStates();
    int s = ofst->AddState();
    ofst->SetStart(s);
    for (int i = 0; i != R_.size(); ++i) {
      int d = ofst->AddState();
      ofst->AddArc(s, StdArc(R_[i], R_[i], TropicalWeight::One(), d));
      s = d;
    }
    ofst->SetFinal(s, TropicalWeight::One());
  }

  /// Outputs the confidences for the one-best transcript.
  const std::vector<float> &GetOneBestConfidences() const {
    return one_best_confidences_;
  }

  /// Returns the expected WER over this sentence (assuming
  /// model correctness.
  float GetBayesRisk() const { return L_; }
  
  const std::vector<std::vector<std::pair<int32, float> > > &GetSausageStats() const {
    return gamma_;
  }  

  void GetFst(StdMutableFst* ofst) {
    ofst->DeleteStates();
    int s = ofst->AddState();
    ofst->SetStart(s);
    std::vector<std::vector<std::pair<int32, float> > > stats = GetSausageStats();
    for (int i = 0; i != stats.size(); ++i) {
      if (stats[i].size() == 1 && stats[i][0].first == 0)
        continue;
      int d = ofst->AddState();
      for (int j = 0; j != stats[i].size(); ++j) {
        pair<int32, float> p = stats[i][j];
        ofst->AddArc(s, StdArc(p.first, p.first, -logf(p.second), d));
      }
      s = d;
    }
    ofst->SetFinal(s, TropicalWeight::One());
  }

 private:
  /// Minimum-Bayes-Risk Decode. Top-level algorithm.  Figure 6 of the paper.
  void MbrDecode() {
    for (size_t counter = 0; ; counter++) {
      NormalizeEps(&R_);
      AccStats(); // writes to gamma_
      double delta_Q = 0.0; // change in objective function.
      
      // Caution: q in the line below is (q-1) in the algorithm
      // in the paper; both R_ and gamma_ are indexed by q-1.
      for (size_t q = 0; q < R_.size(); q++) {
        if (do_mbr_) { // This loop updates R_ [indexed same as gamma_]. 
          // gamma_[i] is sorted in reverse order so most likely one is first.
          const vector<pair<int32, float> > &this_gamma = gamma_[q];
          double old_gamma = 0, new_gamma = this_gamma[0].second;
          int32 rq = R_[q], rhat = this_gamma[0].first; // rq: old word, rhat: new.
          for (size_t j = 0; j < this_gamma.size(); j++)
            if (this_gamma[j].first == rq) old_gamma = this_gamma[j].second;
          delta_Q += (old_gamma - new_gamma); // will be 0 or negative; a bound on
          // change in error.
          if (rq != rhat)
            VLOG(2) << "Changing word " << rq << " to " << rhat;
          R_[q] = rhat;
        }
        if (R_[q] != 0) {
          float confidence = 0.0;
          for (int32 j = 0; j < gamma_[q].size(); j++)
            if (gamma_[q][j].first == R_[q]) confidence = gamma_[q][j].second;
          one_best_confidences_.push_back(confidence);
        }
      }
      VLOG(2) << "Iter = " << counter << ", delta-Q = " << delta_Q;
      if (delta_Q == 0) break;
      if (counter > 100) {
        LOG(WARN) << "Iterating too many times in MbrDecode; stopping.";
        break;
      }
    }
    RemoveEps(&R_);
  }
  
  inline double l(int32 a, int32 b) { 
    if (edit_matrix_.empty())
      return (a == b ? 0.0 : 0.24); 
    if (edit_matrix_.find(pair<int,int>(a,b)) == edit_matrix_.end())
      return (a == b ? 0.0 : 0.24); 
    return edit_matrix_.find(pair<int,int>(a,b))->second;
  }

  /// returns r_q, in one-based indexing, as in the paper.
  inline int32 r(int32 q) { return R_[q-1]; }
  
  
  /// Figure 4 of the paper; called from AccStats (Fig. 5)
  double EditDistance(int32 N, int32 Q,
                      vector<double> &alpha,
                      vector<vector<double> > &alpha_dash,
                      vector<double> &alpha_dash_arc) {
    alpha[1] = 0.0; // = log(1).  Line 5.
    alpha_dash[1][0]= 0.0; // Line 5.
    for (int32 q = 1; q <= Q; q++) 
      alpha_dash[1][q] = alpha_dash[1][q - 1] + l(0, r(q)); // Line 7.
    for (int32 n = 2; n <= N; n++) {
      double alpha_n = kLogZeroDouble;
      for (size_t i = 0; i < pre_[n].size(); i++) {
        const MbrArc &arc = arcs_[pre_[n][i]];
        alpha_n = LogAdd(alpha_n, alpha[arc.start_node] + arc.loglike);
      }
      alpha[n] = alpha_n; // Line 10.
      // Line 11 omitted: matrix was initialized to zero.
      for (size_t i = 0; i < pre_[n].size(); i++) {
        const MbrArc &arc = arcs_[pre_[n][i]];
        int32 s_a = arc.start_node, w_a = arc.word;
        BaseFloat p_a = arc.loglike;
        for (int32 q = 0; q <= Q; q++) {
          if (q == 0) {
            alpha_dash_arc[q] = // line 15.
                alpha_dash[s_a][q] + l(w_a, 0) + delta();
          } else {  // a1,a2,a3 are the 3 parts of min expression of line 17.
            int32 r_q = r(q);
            double a1 = alpha_dash[s_a][q-1] + l(w_a, r_q),
                a2 = alpha_dash[s_a][q] + l(w_a, 0) + delta(),
                a3 = alpha_dash_arc[q-1] + l(0, r_q);
            alpha_dash_arc[q] = std::min(a1, std::min(a2, a3));
          }
          // line 19:
          alpha_dash[n][q] += exp(alpha[s_a] + p_a - alpha[n]) * alpha_dash_arc[q];
        }
      }
    }
    return alpha_dash[N][Q]; // line 23.
  }

  void SetZero(vector<double>* v) {
    vector<double>& d = *v;
    for (int i = 0; i != d.size(); ++i)
      d[i] = 0;
  }
  /// Figure 5 of the paper.  Outputs to gamma_ and L_.
  void AccStats() {
    using std::map;
    
    int32 N = static_cast<int32>(pre_.size()) - 1,
        Q = static_cast<int32>(R_.size());

    vector<double> alpha(N+1, 0.0); // index (1...N)
    vector<double> alpha_dash_arc(Q+1, 0.0); // index 0...Q
    vector<vector<double> > alpha_dash(N+1); // index (1...N, 0...Q)
    vector<vector<double> > beta_dash(N+1); // index (1...N, 0...Q)
    vector<double> Qp1(Q+1);
    for (int i = 0; i < N + 1; ++i) {
      alpha_dash.push_back(Qp1);
      beta_dash.push_back(Qp1);
    }
    vector<double> beta_dash_arc(Q + 1); // index 0...Q
    vector<char> b_arc(Q+1); // integer in {1,2,3}; index 1...Q
    vector<map<int32, double> > gamma(Q+1); // temp. form of gamma.
    // index 1...Q [word] -> occ.

    double Ltmp = EditDistance(N, Q, alpha, alpha_dash, alpha_dash_arc); 
    if (L_ != 0 && Ltmp > L_) { // L_ != 0 is to rule out 1st iter.
      LOG(WARN) << "Edit distance increased: " << Ltmp << " > "
                 << L_;
    }
    L_ = Ltmp;
    VLOG(2) << "L = " << L_;
    // omit line 10: zero when initialized.
    beta_dash[N][Q] = 1.0; // Line 11.
    for (int32 n = N; n >= 2; n--) {
      for (size_t i = 0; i < pre_[n].size(); i++) {
        const MbrArc &arc = arcs_[pre_[n][i]];
        int32 s_a = arc.start_node, w_a = arc.word;
        BaseFloat p_a = arc.loglike;
        alpha_dash_arc[0] = alpha_dash[s_a][0]+ l(w_a, 0) + delta(); // line 14.
        for (int32 q = 1; q <= Q; q++) { // this loop == lines 15-18.
          int32 r_q = r(q);
          double a1 = alpha_dash[s_a][q-1] + l(w_a, r_q),
              a2 = alpha_dash[s_a][q] + l(w_a, 0) + delta(),
              a3 = alpha_dash_arc[q-1] + l(0, r_q);
          if (a1 <= a2) {
            if (a1 <= a3) { b_arc[q] = 1; alpha_dash_arc[q] = a1; }
            else { b_arc[q] = 3; alpha_dash_arc[q] = a3; }
          } else {
            if (a2 <= a3) { b_arc[q] = 2; alpha_dash_arc[q] = a2; }
            else { b_arc[q] = 3; alpha_dash_arc[q] = a3; }
          }
        }
        SetZero(&beta_dash_arc);
        for (int32 q = Q; q >= 1; q--) {
          // line 21:
          beta_dash_arc[q] += exp(alpha[s_a] + p_a - alpha[n]) * beta_dash[n][q];
          switch (static_cast<int>(b_arc[q])) { // lines 22 and 23:
            case 1:
              beta_dash[s_a][q-1] += beta_dash_arc[q];
              // next: gamma(q, w(a)) += beta_dash_arc(q)
              AddToMap(w_a, beta_dash_arc[q], &(gamma[q]));
              break;
            case 2:
              beta_dash[s_a][q] += beta_dash_arc[q];
              break;
            case 3:
              beta_dash_arc[q-1] += beta_dash_arc[q];
              // next: gamma(q, epsilon) += beta_dash_arc(q)
              AddToMap(0, beta_dash_arc[q], &(gamma[q]));
              break;
            default:
              FSTERROR() << "Invalid b_arc value"; // error in code.
          }
        }
        beta_dash_arc[0] += exp(alpha[s_a] + p_a - alpha[n]) * beta_dash[n][0];
        beta_dash[s_a][0] += beta_dash_arc[0]; // line 26.
      }
    }
    SetZero(&beta_dash_arc);
    for (int32 q = Q; q >= 1; q--) {
      beta_dash_arc[q] += beta_dash[1][q];
      beta_dash_arc[q-1] += beta_dash_arc[q];
      AddToMap(0, beta_dash_arc[q], &(gamma[q]));
      // the statements below are actually redundant because
      // state_times_[1] is zero.
    }
    for (int32 q = 1; q <= Q; q++) { // a check (line 35)
      double sum = 0.0;
      for (map<int32, double>::iterator iter = gamma[q].begin();
           iter != gamma[q].end(); ++iter) sum += iter->second;
      if (fabs(sum - 1.0) > 0.1)
        LOG(WARN) << "sum of gamma[" << q << ",s] is " << sum;
    }
    // The next part is where we take gamma, and convert
    // to the class member gamma_, which is using a different
    // data structure and indexed from zero, not one.
    gamma_.clear();
    gamma_.resize(Q);
    for (int32 q = 1; q <= Q; q++) {
      for (map<int32, double>::iterator iter = gamma[q].begin();
           iter != gamma[q].end(); ++iter)
        gamma_[q-1].push_back(std::make_pair(iter->first, static_cast<BaseFloat>(iter->second)));
      // sort gamma_[q-1] from largest to smallest posterior.
      GammaCompare comp;
      std::sort(gamma_[q-1].begin(), gamma_[q-1].end(), comp);
    }
  }

struct Int32IsZero {
  bool operator() (int32 i) { return (i == 0); }
};
  /// Removes epsilons (symbol 0) from a vector
  static void RemoveEps(std::vector<int32> *vec) {
    Int32IsZero pred;
    vec->erase(std::remove_if (vec->begin(), vec->end(), pred),
               vec->end());
  }

  // Ensures that between each word in "vec" and at the beginning and end, is
  // epsilon (0).  (But if no words in vec, just one epsilon)
  static void NormalizeEps(std::vector<int32> *vec) {
    RemoveEps(vec);
    vec->resize(1 + vec->size() * 2);
    int32 s = vec->size();
    for (int32 i = s/2 - 1; i >= 0; i--) {
      (*vec)[i*2 + 1] = (*vec)[i];
      (*vec)[i*2 + 2] = 0;
    }
    (*vec)[0] = 0;
  }

  static inline BaseFloat delta() { return 1.0e-05; } // A constant
  // used in the algorithm.

  /// Function used to increment map.
  static inline void AddToMap(int32 i, double d, std::map<int32, double> *gamma) {
    if (d == 0) return;
    std::pair<const int32, double> pr(i, d);
    std::pair<std::map<int32, double>::iterator, bool> ret = gamma->insert(pr);
    if (!ret.second) // not inserted, so add to contents.
      ret.first->second += d;
  }
    
  
  /// Boolean configuration parameter: if true, we actually update the hypothesis
  /// to do MBR decoding (if false, our output is the MAP decoded output, but we
  /// output the stats too).
  bool do_mbr_;
  
  /// Arcs in the topologically sorted acceptor form of the word-level lattice,
  /// with one final-state.  Contains (word-symbol, log-likelihood on arc ==
  /// negated cost).  Indexed from zero.
  std::vector<MbrArc> arcs_;

  /// For each node in the lattice, a list of arcs entering that node. Indexed
  /// from 1 (first node == 1).
  std::vector<std::vector<int32> > pre_;
  
  std::vector<int32> R_; // current 1-best word sequence, normalized to have
  // epsilons between each word and at the beginning and end.  R in paper...
  // caution: indexed from zero, not from 1 as in paper.

  double L_; // current averaged edit-distance between lattice and R_.
  // \hat{L} in paper.
  
  std::vector<std::vector<std::pair<int32, BaseFloat> > > gamma_;
  // The stats we accumulate; these are pairs of (posterior, word-id), and note
  // that word-id may be epsilon.  Caution: indexed from zero, not from 1 as in
  // paper.  We sort in reverse order on the second member (posterior), so more
  // likely word is first.


  std::vector<BaseFloat> one_best_confidences_;
  // vector of confidences for the 1-best output (which could be
  // the MAP output if do_mbr_ == false, or the MBR output otherwise).
  // Indexed by the same index as one_best_times_.
  
  struct GammaCompare{
    // should be like operator <.  But we want reverse order
    // on the 2nd element (posterior), so it'll be like operator
    // > that looks first at the posterior.
    bool operator () (const std::pair<int32, BaseFloat> &a,
                      const std::pair<int32, BaseFloat> &b) const {
      if (a.second > b.second) return true;
      else if (a.second < b.second) return false;
      else return a.first > b.first;
    }
  };
};


//Get the most confident sequence
template<class Arc>
void MbrDecode(const Fst<Arc>& ifst, StdMutableFst* ofst) {
  MinimumBayesRiskNoTime<Arc> mbr(ifst);
  mbr.GetOneBest(ofst);
}

//Get most confident sequence and the confusion network
template<class Arc>
void MbrDecode(const Fst<Arc>& ifst, StdMutableFst* ofst, StdMutableFst* cfst) {
  MinimumBayesRiskNoTime<Arc> mbr(ifst);
  mbr.GetOneBest(ofst);
  mbr.GetFst(cfst);
}
//Get most confident sequence and the confusion network
template<class Arc>
void MbrDecode(const Fst<Arc>& ifst, StdMutableFst* ofst, StdMutableFst* cfst,
  const typename MinimumBayesRiskNoTime<Arc>::EditMatrix& em) {
  MinimumBayesRiskNoTime<Arc> mbr(ifst, em);
  mbr.GetOneBest(ofst);
  mbr.GetFst(cfst);
}

}  // namespace fst

#endif  // DCD_LAT_SAUSAGES_NOTIME_H__
