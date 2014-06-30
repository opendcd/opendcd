// token.h
// \file
// Simple token defintion

#ifndef DCD_TOKEN_H__
#define DCD_TOKEN_H__

#include <dcd/constants.h>

namespace dcd {

//L is the type of the lattice state
template<class L>
class TokenTpl {
 public:  
  //Create a default inactive token
  TokenTpl () : tb_(0), cost_(kMaxCost) { }
  
	TokenTpl(L tb, float cost) : tb_(tb), cost_(cost) { }

  //if the tb_ is zero or null return false
  inline bool Active() const {
    return tb_;
  }

  inline void Clear() {
    tb_ = 0;
    cost_ = kMaxCost;
  }

  inline void SetValue(L tb, float cost) {
    tb_ = tb;
    cost_ = cost;
  }

  //Extend the token cost_ by cost or return the inactive token
  inline TokenTpl Expand(float cost) const {
    return Active() ? TokenTpl(tb_, cost_ + cost) : TokenTpl();
  }

  //Combine with token, arriving from search arc
  //if this Token is not active allocate a new lattice state
  //Add new lattice arc for search arc, AddArc could perform
  //on-the-fly rescoring and return a better cost than token.Cost()
  //Return a pair giving the cost of the arc arriving in the state
  //and the best score of the state
  //TODO are the extract parameters slowing things down
  template<class LATTICE, class SearchArc>
  inline pair<float, float> Combine(const TokenTpl& token, 
                                    const SearchArc& arc, 
                                    LATTICE* lattice, int time, 
                                    float threshold, 
                                    const SearchOptions& opts) {
    //Token should be the exit token from the arc
    //Might have  insertion penalty somewhere else
    if (!this->Active())
      tb_ = lattice->AddState(time, arc.NextState());
   
    //Re-scored arc might be the best 
    pair<float, float> costs = 
      lattice->AddArc(token.tb_, tb_, token.Cost(), arc, threshold, opts);
    cost_ = costs.second;
    return costs;
  }

	//Combine token with the current Token, keeping only
	//the best score and not generating a traceback arc 
	//or lattice arc
	inline float Combine(const TokenTpl& token) {
		if (token.Cost() < cost_) {
			cost_ = token.cost_;
			tb_ = token.tb_;
		}
		return cost_;
	}

	//Exted token by cost w with the current Token, keeping only
	//the best score and not generating a traceback arc 
	//or lattice arc
	inline float Combine(const TokenTpl token, float w) {
		if (token.cost_ + w < cost_) {
			cost_ = token.cost_ + w;
			tb_ = token.tb_;
		}
		return cost_;
	}
 
	template<class Arc>
	int GetBestSequence(VectorFst<Arc>* ofst) const {
		return tb_->GetBestSequence(ofst);
	}

  template<class T>
  friend ostream& operator<<(ostream& os, const TokenTpl<T>& dt);

  inline float Cost() const { return  cost_; }

  inline void SetCost(float cost) { cost_ = cost; }

  inline L LatticeState() const { return tb_; }
    
  inline void GcMark() { if (tb_) tb_->GcMark(); }

  L tb_;

  float cost_;

  unsigned int alignment_;
};

//Extends the token cost by cost F
template<class L, class F>
inline TokenTpl<L> Times(const TokenTpl<L>& token, const F& f) {
  if (!token.Active())
    return token;
  return TokenTpl<L>(token.LatticeState(), token.Cost() + f);
}

//For debugging
template<class T>
ostream& operator << (ostream& os, const TokenTpl<T>& token) {
  os << token.tb_ << " " << token.cost_;
  return os;
}

} //namespace dcd
#endif
