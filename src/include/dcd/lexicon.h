// lexicon.h
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
// Author: jorono@yandex-team.ru (Josef Novak)
// \file
// Basic OpenFst-based lexicon class.
// Default behavior generates a deterministic
// lexicon in the form of a lexical tree.

#ifndef DCD_LEXICON_H__
#define DCD_LEXICON_H__

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <fst/fstlib.h>
#include <math.h>
#include <dcd/log.h>

namespace dcd {
  // Is this a no-no?
  using namespace fst;
  typedef map<vector<string>, vector<pair<string,double> > > LexMap;
  
  /** \class Lexicon
	  Basic lexicon class.  Efficient generation as a
	  pre-determinized, lexicographically sorted tree.
  */
  template <class Arc, class AWeight>
  class LexiconFst {
  public:
	double      silprob_;
	double      non_silprob_;
	string      lexicon_;
	string      eps_;
	string      sil_;
	bool        pos_;
	bool        closure_;
	SymbolTable isyms_;
	SymbolTable osyms_;
	int         start_;
    int         sil_id_;
    LexMap      lexmap_;
    Logger      lexlog_;
    set<int>    disambig_int_; //Kaldi wants these
    VectorFst<Arc> lfst_;

	//prettified: ε
    LexiconFst( ) : 
	    lexicon_(""), det_(true), isyms_(SymbolTable("isyms")),
		osyms_(SymbolTable("osyms")), vlog_(1), eps_("<eps>"), 
		do_sil_(false) { }

    LexiconFst( const string lexicon ) :
		lexicon_(lexicon), det_(true), isyms_(SymbolTable("isyms")), 
        osyms_(SymbolTable("osyms")), vlog_(1), eps_("<eps>"), sil_("SIL"), 
        do_sil_(false), silprob_(Arc::Weight::One().Value()) {
	  isyms_.AddSymbol(eps_);
      osyms_.AddSymbol(eps_);
      lfst_.AddState();
      lfst_.SetStart(0);
    }

    LexiconFst( const string lexicon, string eps, string sil,
				double silprob, bool pos, bool det, bool do_sil,
				SymbolTable isyms, SymbolTable osyms,
				Logger lexlog, bool closure ) :
		lexicon_(lexicon), eps_(eps), sil_(sil), silprob_(silprob), pos_(pos),
		det_(det), do_sil_(do_sil), isyms_(isyms), osyms_(osyms),
		lexlog_(lexlog), closure_(closure) {
    
	  if( silprob_ == 1.0 ){
        non_silprob_ = _ToLog( silprob_ );
        silprob_     = _ToLog( silprob_ );
      } else {
        non_silprob_ = _ToLog( 1.0 - silprob_ );
        silprob_     = _ToLog( silprob_ );
      }
    
      if( isyms_.NumSymbols() == 0 )
        isyms_.AddSymbol(eps_);
      else if( isyms_.Find((int64)0) != eps_ )
        lexlog_(WARN) << "Possible input epsilon mismatch: " << eps_ << " != "
                      << isyms_.Find((int64)0);
    
      if( osyms_.NumSymbols() == 0 )
        osyms_.AddSymbol(eps_);
      else if( osyms_.Find((int64)0) != eps_ )
        lexlog_(WARN) << "Possible output epsilon mismatch: " << eps_ << " != "
                      << osyms_.Find((int64)0);
      sil_id_ = isyms_.AddSymbol(sil_);
    
      start_ = lfst_.AddState();
      lfst_.SetStart(start_);
      //lfst_.AddState();
      //lfst_.AddArc( 0, Arc( sil_id_, 0, silprob_, 1 ) );
      lfst_.AddArc( 0, Arc( sil_id_, 1, 0, 0 ) );
      /*
      if( do_sil_ ){
        start_ = lfst_.AddState();
        lfst_.AddArc( 0, Arc( 0, 0, non_silprob_, start_ ) );
        lfst_.AddArc( 0, Arc( sil_id_, 0, silprob_, start_ ) );
      }
      */
    }
                       
	bool GenerateFst( ) {
      for( LexMap::iterator it=lexmap_.begin();
           it != lexmap_.end(); it++ ) {
        AddEntryToFst( it->first, it->second );
      }
      return true;
    }

    void Write( string ofstname ){
      lfst_.SetInputSymbols(&isyms_);
      lfst_.SetOutputSymbols(&osyms_);
      lfst_.Write(ofstname);
      return;
    }
  
    /** \function AddEntryToFst
        Add a single entry to the lexicon.
        Traverse the lexicon tree and start adding states as needed.
    */
    bool AddEntryToFst( vector<string> phones,
                        vector<pair<string,double> > words ){
      int pid = 0;
      int sid = start_;
      while( pid < phones.size() ){
        int nsid = FindLabel( sid, isyms_.Find(phones[pid]), MATCH_INPUT);
        if( nsid == kNoStateId )
          break;
        ++pid;
        sid = nsid;
      }
      
      while( pid < phones.size() ){
        int nsid = lfst_.AddState();
        lfst_.AddArc( sid, Arc(
            isyms_.Find(phones[pid]), 0, Arc::Weight::One(), nsid
                               ) );
        ++pid;
        sid = nsid;
      }

      // A better long-term option may be to encode the set of
      // words associated with the pronunciation as a set, and
      // perform a more sophisticated match - avoiding disambig
      // symbols entirely. How would this work in practice?
      for( int i = 0; i < words.size(); i++ ){
        int wid  = osyms_.Find(words[i].first);
        string hash;
        sprintf( (char*)hash.c_str(), "#%d", i );
        int isym = 0;
        if( ( i > 0 && pos_ ) || !pos_ ) {
            isym = isyms_.AddSymbol(hash);
            //Kaldi wants these tracked separately
            disambig_int_.insert(isym);
        }
        
        if( FindLabel(sid, wid, MATCH_OUTPUT)==kNoStateId ){
          int nsid = lfst_.AddState();
          lfst_.AddArc( sid, Arc( isym, wid, words[i].second, nsid ) );
          lfst_.SetFinal( nsid, Arc::Weight::One() );
        }
      }
    
      return true;
    }

    /** \function FindLabel
        This user the OpenFst Matcher idiom to find
        existing partial matches in the phonetic tree structure.
        Label-lookup uses binary search - which means that
        all information should be pre-sorted for best performance.
        This includes:
         * phoneme symbols list
         * word symbols lise
         * actual dictionary entries
    */
    int FindLabel( int sid, int lab, MatchType mtype ){
      SortedMatcher<Fst<Arc> > matcher(lfst_, mtype);
      matcher.SetState(sid);
      if( matcher.Find(lab) ){
        for( ; !matcher.Done(); matcher.Next() ){
          const Arc &arc = matcher.Value();
          return arc.nextstate;
        }
      }
      // We could have some negative labels
      // We should guarantee the failure label
      return kNoStateId;
    }

	/** \function LoadLexicon
		Load the lexicon from a standard dictionary file:
		  WORD [prob] ph1 ph2 ph3 ph4         
        The std::map and std::set containers maintain lexicographic order by
        default, so this should be in the desired order when we finish.
    */
    void LoadLexicon( ){
      set<string> phones, words;

      ifstream ifp(lexicon_.c_str());
      string   line;
      while( getline(ifp, line) ){
        vector<string> pron;
        char*  tok   = strtok((char*)line.c_str(), " \t");
        string word  = tok;
        AWeight prob = Arc::Weight::One().Value();
        words.insert(word);
        tok = strtok(NULL, " \t");

        while( tok != NULL ){
          char* endptr;
          double tmp = strtod(tok,&endptr);
          if( endptr == tok + strlen(tok) ) {
            //If there is phoneme label that is a valid float 
            // then this will behave badly.  Seems unlikely...
            prob = _ToLog(tmp); 
            tok = strtok(NULL, " \t");
          } else {
            pron.push_back(tok);
            phones.insert(tok);
            tok = strtok(NULL, " \t");
          }
        }

        // Add positional monophone info, if needed
        if( pos_ == true ){
          if( pron.size() == 1 ){
            pron[0] += "_S";
            phones.insert(pron[0]);
          }else{
            for( int i=0; i < pron.size(); i++ ){
              if( i == 0 )
                pron[i] += "_B";
              else if( i == pron.size()-1 )
                pron[i] += "_E";
              else
                pron[i] += "_I";
              phones.insert(pron[i]);
            }
          }
        }
      
        if( closure_ == false && do_sil_ == true ){
          lexmap_[pron].push_back(
              make_pair(word, Times(prob, non_silprob_).Value() ) );
        
          pron.push_back(sil_);
          lexmap_[pron].push_back(
              make_pair(word, Times(prob, silprob_).Value() ) );
        } else {
          lexmap_[pron].push_back(
              make_pair(word, prob.Value()) );
        }
      }
      ifp.close();
    
      // The std::set container also maintains lexicographic order by default
      for( set<string>::iterator it = phones.begin();
           it != phones.end(); it++ )
        isyms_.AddSymbol(*it);
      for( set<string>::iterator it = words.begin();
           it != words.end(); it++ )
        osyms_.AddSymbol(*it);
      
      return;
    }

    /* \function CloseLexicon
       This is a 'special' closure algorithm for the determinized lexicon.
       It does the following:
         1.) push labels & weights to the start
         2.) do local removal of 'safe' epsilons and all final states
         3.) connect all dangling final arcs to the start state
        * Shrinks result by %20 absolute compared to standard opts
        * Does so maintaining determinism *and* without modifying topology
        * Compare to standard Kaldi recipe + fstdeterminize + fstclosure
          on 180k CMUdict: Kaldi: 30MB, this: 12MB (and only 1 eps transition)
    */
    void CloseLexicon( bool push=true ){
      if( push == true ) {
        VectorFst<Arc>* tmp = new VectorFst<Arc>();
        Push<Arc,REWEIGHT_TO_INITIAL>(lfst_, tmp, kPushWeights|kPushLabels);
        lfst_ = *tmp;
        delete tmp;
      }

      vector<int> del_states;
      for( StateIterator<MutableFst<Arc> > siter( lfst_ );
           !siter.Done(); siter.Next() ){
        int st = siter.Value();
        for( MutableArcIterator<MutableFst<Arc> > aiter( &lfst_, st );
             !aiter.Done(); aiter.Next() ){
          Arc arc = aiter.Value();
          if( lfst_.Final(arc.nextstate) == AWeight::One() ){
            del_states.push_back(arc.nextstate);
            if( arc.ilabel == 0 && arc.olabel == 0 &&
                arc.weight==AWeight::One() ){
              lfst_.SetFinal(st,AWeight::One());
            } else {
              arc.nextstate = 0;
              aiter.SetValue(arc);
            }
          }
        }
      }

      if( del_states.size()==0 ){
        //We have finished; now set the one final state and
        // check for useless redundant weight on the 2 entry arcs
        lfst_.SetFinal(start_, AWeight::One());
        MutableArcIterator<MutableFst<Arc> > aiter(&lfst_, 0);
        aiter.Seek(0);
        Arc a1 = aiter.Value();
        aiter.Seek(1);
        Arc a2 = aiter.Value();
        if( a1.weight == a2.weight ){
          a2.weight = AWeight::One();
          aiter.SetValue(a2);
          aiter.Seek(0);
          a1.weight = AWeight::One();
          aiter.SetValue(a1);
        }
        return;
      }else{
        //We have not finished yet.
        lfst_.DeleteStates(del_states);
        CloseLexicon(false);
      }
      return;
    }
           
  private:
    // Return the natural (base-e) logarithm of the input
    // Should this be returned as the AWeight template?
    static inline double _ToLog( double val ){ return -log(val); }
    
    bool  det_;
    bool  do_sil_;
    int   vlog_;
  };

}  //  namespace dcd

#endif  //  DCD_LEXICON_H__
