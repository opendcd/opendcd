// ngram-input.h
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
// Copyright 2009-2013 Brian Roark and Google, Inc.
// Authors: roarkbr@gmail.com  (Brian Roark)
//          allauzen@google.com (Cyril Allauzen)
//          riley@google.com (Michael Riley)
//
// \file
// NGram model class for reading in a model or text for building a model
// Stripped out of the OpenGrm Ngram library for independent use.


#ifndef DCD_NGRAM_INPUT_H__
#define DCD_NGRAM_INPUT_H__

#include <fst/arcsort.h>
#include <fst/fst.h>
#include <fst/matcher.h>
#include <fst/mutable-fst.h>
#include <fst/vector-fst.h>
#include <dcd/ngram-count.h>

namespace dcd {

using namespace fst;

using std::vector;
using std::istream;
using std::ostream;

class NGramInput {
 public:
   typedef StdArc Arc;
   typedef Arc::StateId StateId;
   typedef Arc::Label Label;
   typedef Arc::Weight Weight;

   // Construct an NGramInput object, with a symbol table, fst and streams
   NGramInput(const string &ifile, const string &ofile,
              const string &symbols, const string &epsilon_symbol,
              const string &OOV_symbol, const string &start_symbol,
              const string &end_symbol, const string &bo_ilabel,
              const string &bo_olabel, bool incl_symbols)
     : fst_(NULL), OOV_symbol_(OOV_symbol), start_symbol_(start_symbol),
       end_symbol_(end_symbol), bo_ilabel_(bo_ilabel), bo_olabel_(bo_olabel),
       incl_symbols_(incl_symbols) {
     own_istrm_ = false;
     own_ostrm_ = false;
     if (ifile.empty())
       istrm_ = &std::cin;
     else {
       istrm_ = new ifstream(ifile.c_str());
       if (!(*istrm_))
	 LOG(FATAL) << "Can't open " << ifile << " for reading";
       own_istrm_ = true;
     }
     if (ofile.empty()) {
       ostrm_ = &std::cout;
     } else {
       ostrm_ = new ofstream(ofile.c_str());
       if (!(*ostrm_))
	 LOG(FATAL) << "Can't open " << ofile << " for writing";
       own_ostrm_ = true;
     }
     if (symbols == "") {  // Symbol table not provided
       syms_ = new SymbolTable("NGramSymbols");  // initialize symbol table
       syms_->AddSymbol(epsilon_symbol);  // make epsilon 0
       // add backoff labels
       syms_->AddSymbol(bo_ilabel);
       syms_->AddSymbol(bo_olabel);
       add_symbols_ = 1;
     } else {
       ifstream symstrm(symbols.c_str());
       syms_ = SymbolTable::ReadText(symstrm, "NGramSymbols");
       if (syms_ == 0)
         LOG(FATAL) << "NGramInput: Could not read symbol table file: "
                    << symbols;
       if (syms_->Find(bo_ilabel) < 0)
         LOG(FATAL) << "NGramInput: Backoff ilabel requested but not found.";
       if (syms_->Find(bo_olabel) < 0)
         LOG(FATAL) << "NGramInput: Backoff olabel requested but not found.";

       add_symbols_ = 0;
     }
     // Set backoff labels to zero. relabel at the end.
     bo_ilabel_id_ = 0;
     bo_olabel_id_ = 0;
   }

   ~NGramInput() {
     if (own_istrm_) {
       delete istrm_;
     }
     if (own_ostrm_) {
       delete ostrm_;
     }
   }

   // Read text input of three types: ngram counts, ARPA model or text corpus
   // output either model fsts, a corpus far or a symbol table.
   void ReadInput(bool ARPA, bool symbols) {
     if (ARPA) {  // ARPA format model in, fst model out
       CompileARPAModel();
     } else if (symbols) {
       CompileSymbolTable();
     } else {  // sorted list of ngrams + counts in, fst out
       CompileNGramCounts();
     }
   }

 private:
   // Function that returns 1 if whitespace, 0 otherwise
   bool ws(char c) {
     if (c == ' ') return 1;
     if (c == '\t') return 1;
     return 0;
   }

   // Using whitespace as delimiter, read token from string
   bool GetWhiteSpaceToken(string::iterator *strit,
			   string *str, string *token) {
     while (ws(*(*strit)))  // skip the whitespace preceding the token
       (*strit)++;
     if ((*strit) == str->end())  // no further tokens to be found in string
       return 0;
     while ((*strit) < str->end() && !ws(*(*strit))) {
       (*token) += (*(*strit));
       (*strit)++;
     }
     return 1;
   }

   // Get symbol label from table, add it and ensure no duplicates if requested
   Label GetLabel(string word, bool add, bool dups) {
     Label symlabel = syms_->Find(word);  // find it in the symbol table
     if (!add_symbols_) {  // fixed symbol table provided
       if (symlabel < 0) {
	 symlabel = syms_->Find(OOV_symbol_);
	 if (symlabel < 0) {
	   LOG(FATAL)
	     <<  "NGramInput: OOV Symbol not found in given symbol table: "
	     << OOV_symbol_;
	 }
       }
     }
     else if (add) {
       if (symlabel < 0) {
	 symlabel = syms_->AddSymbol(word);
       } else if (!dups) {  // shouldn't find duplicate
	 LOG(FATAL) << "NGramInput: Symbol already found in list: " << word;
       }
     }
     else if (symlabel < 0) {
       LOG(FATAL) << "NGramInput: Symbol not found in list: " << word;
     }
     return symlabel;
   }

   // GetLabel() if not <s> or </s>, otherwise set appropriate bool values
   Label GetNGramLabel(string ngram_word, bool add, bool dups,
		       bool *stsym, bool *endsym) {
     (*stsym) = (*endsym) = 0;
     if (ngram_word == start_symbol_) {
       (*stsym) = 1;
       return -1;
     } else if (ngram_word == end_symbol_) {
       (*endsym) = 1;
       return -2;
     } else {
       return GetLabel(ngram_word, add, dups);
     }
   }

   // Use string iterator to construct token, then get the label for it
   Label ExtractNGramLabel(string::iterator *strit, string *str,
			   bool add, bool dups, bool *stsym, bool *endsym) {
     string token;
     if (!GetWhiteSpaceToken(strit, str, &token))
       LOG(FATAL) << "NGramInput: No token found when expected";
     return GetNGramLabel(token, add, dups, stsym, endsym);
   }

   // Get backoff state and backoff cost for state (following <epsilon> arc)
   StateId GetBackoffAndCost(StateId st, double *cost) {
     StateId backoff = -1;
     Label backoff_label = bo_ilabel_id_;  // <epsilon> is assumed to be label 0 here
     Matcher<StdFst> matcher(*fst_, MATCH_INPUT);
     matcher.SetState(st);
     if (matcher.Find(backoff_label)) {
       for (; !matcher.Done(); matcher.Next()) {
	 StdArc arc = matcher.Value();
	 if (arc.ilabel == backoff_label) {
	   backoff = arc.nextstate;
	   if (cost)
	     (*cost) = arc.weight.Value();
	 }
       }
     }
     return backoff;
   }

   // Just return backoff state
   StateId GetBackoff(StateId st) {
     return GetBackoffAndCost(st, 0);
   }

   // Ensure matching with appropriate ARPA header strings
   bool ARPAHeaderStringMatch(const string tomatch) {
     string str;
     if (!getline((*istrm_), str)) {
       LOG(FATAL) << "Input stream read error";
     }
     if (str != tomatch) {
       str += "   Line should read: ";
       str += tomatch;
       LOG(FATAL) << "NGramInput: ARPA header mismatch!  Line reads: "
		  << str;
     }
     return true;
   }

   // Extract string token and convert into value of type A
   template <class A>
     bool GetStringVal(string::iterator *strit, string *str, A *val,
		       string *keeptoken) {
     string token;
     if (GetWhiteSpaceToken(strit, str, &token)) {
       stringstream ngram_ss(token);
       ngram_ss >> (*val);
       if (keeptoken)
	 (*keeptoken) = token;  // to store token string if needed
       return 1;
     } else {
       return 0;
     }
   }

   // When reading in string numerical tokens, ensures correct inf values
   void CheckInfVal(string *token, double *val) {
     if ((*token) == "-inf" || (*token) == "-Infinity")
       (*val) = log(0);
     if ((*token) == "inf" || (*token) == "Infinity")
       (*val) = -log(0);
   }

   // Read the header at the top of the ARPA model file, collect n-gram orders
   int ReadARPATopHeader(vector<int> *orders) {
     string str;
     // scan the file until a \data\ record is found
     while (getline((*istrm_), str)) {
       if (str == "\\data\\") break;
     }
     if (!getline((*istrm_), str)) {
       LOG(FATAL) << "Input stream read error, or no \\data\\ record found";
     }
     int order = 0;
     while (str != "") {
       string::iterator strit = str.begin();
       while (strit < str.end() && (*strit) != '=')
	 strit++;
       if (strit == str.end())
	 LOG(FATAL)
	   << "NGramInput: ARPA header mismatch!  No '=' in ngram count.";
       strit++;
       int ngram_cnt;  // must have n-gram count, fails if not found
       if (!GetStringVal(&strit, &str, &ngram_cnt, 0))
	 LOG(FATAL) << "NGramInput: ARPA header mismatch!  No ngram count.";
       orders->push_back(ngram_cnt);
       if (ngram_cnt > 0) order++;  // Some reported n-gram orders may be empty
       if (!getline((*istrm_), str)) {
         LOG(FATAL) << "Input stream read error";
       }
     }
     return order;
   }

   // Get the destination state of arc with requested label.  Assumed to exist.
   StateId GetLabelNextState(StateId st, Label label) {
     Matcher<StdFst> matcher(*fst_, MATCH_INPUT);
     matcher.SetState(st);
     if (matcher.Find(label)) {
       StdArc barc = matcher.Value();
       return barc.nextstate;
     } else {
       LOG(FATAL) << "NGramInput: Lower order prefix n-gram not found: ";
     }
    return kNoStateId;
   }

   // Get the destination state of arc with requested label.  Assumed to exist.
   StateId GetLabelNextStateNoFail(StateId st, Label label) {
     Matcher<StdFst> matcher(*fst_, MATCH_INPUT);
     matcher.SetState(st);
     if (matcher.Find(label)) {
       StdArc barc = matcher.Value();
       return barc.nextstate;
     } else {
       return -1;
     }
   }

   // GetLabelNextState() when arc exists; other results for <s> and </s>
   ssize_t NextStateFromLabel(ssize_t st, Label label,
			      bool stsym, bool endsym,
			      NGramCounter<LogWeightTpl<double> >
			      *ngram_counter) {
     if (stsym) {  // start symbol: <s>
       return ngram_counter->NGramStartState();
     } else if (endsym) {  // end symbol </s>
       LOG(FATAL) << "NGramInput: stop symbol occurred in n-gram prefix";
     } else {
       ssize_t arc_id = ngram_counter->FindArc(st, label);
       return ngram_counter->NGramNextState(arc_id);
     }
   }

   // Extract the token, find the label and the appropriate destination state
   ssize_t GetNextState(string::iterator *strit, string *str, ssize_t st,
			NGramCounter<LogWeightTpl<double> > *ngram_counter) {
     bool stsym = 0, endsym = 0;
     Label label = ExtractNGramLabel(strit, str, 0, 0, &stsym, &endsym);
     return NextStateFromLabel(st, label, stsym, endsym, ngram_counter);
   }

   // Read the header for each of the n-gram orders in the ARPA format file
   void ReadARPAOrderHeader(int order) {
     stringstream ss;
     ss << order + 1;
     string tomatch = "\\";
     tomatch += ss.str();
     tomatch += "-grams:";
     ARPAHeaderStringMatch(tomatch);
   }

   // Find or create the backoff state, and add the appropriate backoff arc
   StateId AddBackoffArc(StateId st, Label label, StateId Unigram, bool stsym,
			 bool endsym, int order, double ngram_backoff_prob) {
     StateId nextstate, backoff_st;
     if (stsym) {  // if <s> (should only occur as 1st symbol in n-gram prefix)
       nextstate = fst_->Start();
       backoff_st = Unigram;
     } else if (endsym) {  // </s> should never occur here, no backoff prob
       LOG(FATAL) << "NGramInput: stop symbol has a backoff prob";
     }
     else {  // standard label (not <s> or </s>)
       nextstate = fst_->AddState();  // needs new state (has backoff prob)
       if (order == 0)
	 backoff_st = Unigram;
       else
	 backoff_st = GetLabelNextStateNoFail(GetBackoff(st), label);
       if (backoff_st < 0) {
	 backoff_st = fst_->AddState();  // needs new state
	 cerr << st << " " << label << " backoff\n";
       }
     }
     fst_->AddArc(nextstate, StdArc(bo_ilabel_id_, bo_olabel_id_, ngram_backoff_prob, backoff_st));
     return nextstate;
   }

   // Ensure labels and states are ordered properly, then store for later checks
   void VerifyNGramSort(StateId st, Label label,
			Label *last_lbl, StateId *last_st) {
     if (label >= 0) {  // arc was created (neither <s> or </s>)
       if ((*last_st) == st && label <= (*last_lbl))
	 LOG(FATAL) << "NGramInput: Input not sorted.";
     }
     (*last_lbl) = label;
     (*last_st) = st;
   }

   // Add an n-gram arc as appropriate and record the state and label if req'd
   void AddNGramArc(StateId st, StateId nextstate, Label label, bool stsym,
		    bool endsym, double ngram_log_prob) {
     if (endsym)  // </s> requires no arc, just final cost
       fst_->SetFinal(st, ngram_log_prob);
     else if (!stsym)  // create arc from st to nextstate
       fst_->AddArc(st, StdArc(label, label, ngram_log_prob, nextstate));
   }

   // Read in n-grams for the particular order.
   void ReadARPAOrder(vector<int> *orders, int order, vector<double> *boweights,
		      NGramCounter<LogWeightTpl<double> > *ngram_counter) {
     string str;
     bool add_words = (order == 0);
     for (int i = 0; i < (*orders)[order]; i++) {
       if (!getline((*istrm_), str)) {
         LOG(FATAL) << "Input stream read error";
       }
       string::iterator strit = str.begin();
       double nlprob, boprob;
       string token;
       if (!GetStringVal(&strit, &str, &nlprob, &token))
	 LOG(FATAL) << "NGramInput: ARPA format mismatch!  No ngram log prob.";
       CheckInfVal(&token, &boprob);  // check for inf value
       nlprob *= -log(10);  // convert to neglog base e from log base 10
       ssize_t st = ngram_counter->NGramUnigramState(), nextstate = -1;
       for (int j = 0; j < order; j++)  // find n-gram history state
	 st = GetNextState(&strit, &str, st, ngram_counter);
       bool stsym, endsym;  // stsym == 1 for <s> and endsym == 1 for </s>
       Label label = ExtractNGramLabel(&strit, &str, add_words,
				       0, &stsym, &endsym);
       if (endsym) {
	 ngram_counter->SetFinalNGramWeight(st, nlprob);
       } else if (!stsym) {
	 // Test for presence of all suffixes of n-gram
	 ssize_t backoff_st = ngram_counter->NGramBackoffState(st);
	 while (backoff_st >= 0) {
	   ngram_counter->FindArc(backoff_st, label);
	   backoff_st = ngram_counter->NGramBackoffState(backoff_st);
	 }
	 ssize_t arc_id = ngram_counter->FindArc(st, label);
	 ngram_counter->SetNGramWeight(arc_id, nlprob);
	 nextstate = ngram_counter->NGramNextState(arc_id);
       } else {
	 nextstate = ngram_counter->NGramStartState();
       }

       if (order < orders->size()-1) {
         if (!GetStringVal(&strit, &str, &boprob, &token))
           boprob = 0;
         if (nextstate >= 0 || boprob != 0) {
           if (nextstate < 0)
             LOG(FATAL) << "NGramInput: Have a backoff cost with no state ID!";
           CheckInfVal(&token, &boprob);
           //if (order == 1) 
           //  cout << str << "\t\t||ns: " << nextstate << " bo: " << boprob << " tok: " << token << endl;
           boprob *= -log(10);  // convert to neglog base e from log base 10
           while (nextstate >= boweights->size())
             boweights->push_back(StdArc::Weight::Zero().Value());
           (*boweights)[nextstate] = boprob;
         }
       }
       /*
       if (GetStringVal(&strit, &str, &boprob, &token) &&
	   (nextstate >= 0 || boprob != 0)) {  // found non-zero backoff cost
	 if (nextstate < 0)
	   LOG(FATAL) << "NGramInput: Have a backoff cost with no state ID!";
	 CheckInfVal(&token, &boprob);  // check for inf value
	 boprob *= -log(10);  // convert to neglog base e from log base 10
	 while (nextstate >= boweights->size())
	   boweights->push_back(StdArc::Weight::Zero().Value());
	 (*boweights)[nextstate] = boprob;
       }
       */
     }
     // blank line at end of n-gram order
     if (!getline((*istrm_), str)) {
       LOG(FATAL) << "Input stream read error";
     }
     if (!str.empty()) {
       LOG(FATAL) << "Expected blank line at end of n-grams";
     }
   }

   StateId FindNewDest(StateId st) {
     StateId newdest = st;
     if (fst_->NumArcs(st) > 1 ||
	 fst_->Final(st) != StdArc::Weight::Zero()) return newdest;
     MutableArcIterator<StdMutableFst> aiter(fst_, st);
     StdArc arc = aiter.Value();
     if (arc.ilabel == bo_ilabel_id_)
       newdest = FindNewDest(arc.nextstate);
     return newdest;
   }

   void SetARPANGramDests() {
     vector<StateId> newdests;
     for (StateId st = 0; st < fst_->NumStates(); st++) {
       newdests.push_back(FindNewDest(st));
     }
     for (StateId st = 0; st < fst_->NumStates(); st++) {
       for (MutableArcIterator<StdMutableFst> aiter(fst_, st);
	    !aiter.Done();
	    aiter.Next()) {
	 StdArc arc = aiter.Value();
	 if (arc.ilabel == bo_ilabel_id_)
	   continue;
	 if (newdests[arc.nextstate] != arc.nextstate) {
	   arc.nextstate = newdests[arc.nextstate];
	   aiter.SetValue(arc);
	 }
       }
     }
   }

   // Put stored backoff weights on backoff arcs
   void SetARPABackoffWeights(vector<double> *boweights) {
     for (StateId st = 0; st < fst_->NumStates(); st++) {
       if (st < boweights->size()) {
	 double boprob = (*boweights)[st];
	 MutableArcIterator<StdMutableFst> aiter(fst_, st);
	 StdArc arc = aiter.Value();
	 if (arc.ilabel == bo_ilabel_id_ || boprob != StdArc::Weight::Zero().Value()) {
	   if (arc.ilabel != bo_ilabel_id_) {
             cerr << "ilabel: " << arc.ilabel << " " << "bo_ilabel_id_: " << bo_ilabel_id_ << endl;
	     LOG(FATAL) << "NGramInput: Have a backoff prob but no arc";
	   } else {
	     arc.weight = boprob;
	   }
	   aiter.SetValue(arc);
	 }
       }
     }
   }

   double GetLowerOrderProb(StateId st, Label label) {
     Matcher<StdFst> matcher(*fst_, MATCH_INPUT);
     matcher.SetState(st);
     if (matcher.Find(label)) {
       StdArc arc = matcher.Value();
       return arc.weight.Value();
     }
     if (!matcher.Find(bo_ilabel_id_)) {
       LOG(FATAL) << "NGramInput: No backoff probability";
     }
     for (; !matcher.Done(); matcher.Next()) {
       StdArc arc = matcher.Value();
       if (arc.ilabel == bo_ilabel_id_) {
	 return arc.weight.Value() + GetLowerOrderProb(arc.nextstate, label);
       }
     }
     LOG(FATAL) << "NGramInput: No backoff arc found";
   }

   // Descends backoff arcs to find backoff final cost and set
   double GetFinalBackoff(StateId st) {
     if (fst_->Final(st) != StdArc::Weight::Zero())
       return fst_->Final(st).Value();
     double bocost;
     StateId bostate = GetBackoffAndCost(st, &bocost);
     if (bostate >= 0)
       fst_->SetFinal(st, bocost + GetFinalBackoff(bostate));
     return fst_->Final(st).Value();
   }

   void FillARPAHoles() {
     for (StateId st = 0; st < fst_->NumStates(); st++) {
       double boprob;
       StateId bostate = -1;
       for (MutableArcIterator<StdMutableFst> aiter(fst_, st);
	    !aiter.Done();
	    aiter.Next()) {
	 StdArc arc = aiter.Value();
	 if (arc.ilabel == bo_ilabel_id_) {
	   boprob = arc.weight.Value();
	   bostate = arc.nextstate;
	 } else {
	   if (arc.weight == StdArc::Weight::Zero()) {
	     arc.weight = boprob + GetLowerOrderProb(bostate, arc.ilabel);
	     aiter.SetValue(arc);
	   }
	 }
       }
       if (bostate >= 0 && fst_->Final(st) != StdArc::Weight::Zero() &&
	   fst_->Final(bostate) == StdArc::Weight::Zero()) {
	 GetFinalBackoff(bostate);
       }
     }
   }

   // Handle some pathalogical cases in broken models like those we find 
   // in the WSJ evaluation data.
   // This just resets any remaining arcs with weight Weight::Zero() to 
   // a very small, non-inf value so that subsequent operations do not croak.
   //
   // Also relabel the backoff arcs if necessary.
   // We do this here because the transformation algorithm depends on 
   // the backoff label ID being the lowest after sort.  Annoying.
   void FinalizeArcs() {
     bo_ilabel_id_ = syms_->Find(bo_ilabel_);
     bo_olabel_id_ = syms_->Find(bo_olabel_);
     bool kaldi_bo_labels = (bo_ilabel_id_ != 0 || bo_olabel_id_ != 0);

     for (StateId st = 0; st < fst_->NumStates(); st++) {
       for (MutableArcIterator<StdMutableFst> aiter(fst_, st);
             !aiter.Done();
             aiter.Next()) {
         StdArc arc = aiter.Value();
         if (arc.weight == StdArc::Weight::Zero()) {
           cerr << "Reseting Inf value for arc: " << st << " "
                << arc.nextstate << " " << arc.ilabel << " "
                << arc.olabel << " " << arc.weight << endl;
           arc.weight = 99.0;
         }

         if (arc.ilabel == 0 && kaldi_bo_labels){
           arc.ilabel = bo_ilabel_id_;
           arc.olabel = bo_olabel_id_;
         }
         aiter.SetValue(arc);
       }
     }
   }

   // Read in headers/n-grams from an ARPA model text file, dump resulting fst
   void CompileARPAModel() {
     vector<int> orders;
     ReadARPATopHeader(&orders);
     vector<double> boweights;
     NGramCounter<LogWeightTpl<double> > ngram_counter(orders.size(), false,
                                                       1e-9F, bo_ilabel_id_,
                                                       bo_olabel_id_);
     for (int i = 0; i < orders.size(); i++) {  // Read n-grams of each order
       ReadARPAOrderHeader(i);
       ReadARPAOrder(&orders, i, &boweights, &ngram_counter);
     }
     ARPAHeaderStringMatch("\\end\\");  // Verify that everything parsed well
     fst_ = new StdVectorFst();
     ngram_counter.GetFst(fst_);
     
     ArcSort(fst_, StdILabelCompare());
     SetARPABackoffWeights(&boweights);
     FillARPAHoles();
     SetARPANGramDests();
     Connect(fst_);
     FinalizeArcs();
     DumpFst();
   }

   // Redirect hi order arcs in acyclic count format to states, record backoffs
   void StateBackoffAndCycles(StateId st, StateId bo,
			      vector<StateId> *bo_dest) {
     (*bo_dest)[st] = bo;  // record the backoff state to be added later
     for (MutableArcIterator<StdMutableFst> aiter(fst_, st);
	  !aiter.Done();
	  aiter.Next()) {
       StdArc arc = aiter.Value();
       StateId nst = GetLabelNextState(bo, arc.ilabel);
       if (fst_->Final(arc.nextstate) == StdArc::Weight::Zero() &&
	   fst_->NumArcs(arc.nextstate) == 0) {  // if nextstate not in model
	 arc.nextstate = nst;  // point to state that will persist in the model
	 aiter.SetValue(arc);
       } else {
	 StateBackoffAndCycles(arc.nextstate, nst, bo_dest);
       }
     }
   }

   // Create re-entrant model topology from acyclic count automaton
   void AddBackoffAndCycles(StateId Unigram) {
     ArcSort(fst_, StdILabelCompare());  // ensure arcs fully sorted
     vector<StateId> bo_dest;  // to record backoff destination if needed
     for (StateId st = 0; st < fst_->NumStates(); st++)
       bo_dest.push_back(-1);
     if (fst_->Start() != Unigram)  // repoint n-grams starting with <s>
       StateBackoffAndCycles(fst_->Start(), Unigram, &bo_dest);
     for (ArcIterator<StdMutableFst> aiter(*fst_, Unigram);
	  !aiter.Done();
	  aiter.Next()) {  // repoint n-grams starting with unigram arcs
       StdArc arc = aiter.Value();
       StateBackoffAndCycles(arc.nextstate, Unigram, &bo_dest);
     }
     for (StateId st = 0; st < fst_->NumStates(); st++) {  // add backoff arcs
       if (bo_dest[st] >= 0)  // if backoff state has been recorded
	 fst_->AddArc(st, StdArc(bo_ilabel_id_, bo_olabel_id_,
				 StdArc::Weight::Zero(), bo_dest[st]));
     }
     ArcSort(fst_, StdILabelCompare());  // resort, req'd for new backoff arcs
     Connect(fst_);  // connect to dispose of states that are not in the model
   }

   // Control allocation of ARPA model start state; evidence comes incrementally
   void CheckInitState(vector<string> *words, StateId *Init,
		       StateId Unigram, StateId Start) {
     if (words->size() > 2 && (*Init) == Unigram) {  // 1st evidence order > 1
       if (Start >= 0)  // if start unigram already seen, use state as initial
	 (*Init) = Start;
       else  // otherwise, need to create a start state
	 (*Init) = fst_->AddState();
       fst_->SetStart((*Init));  // Set it as start state
     }
   }

   // Start new word when encountering whitespace, move iterator past whitespace
   bool InitNewWord(vector<string> *words, string::iterator *strit,
		    string *str) {
     while (ws(*(*strit)))  // skip sequence of whitespace
       (*strit)++;
     if ((*strit) != str->end()) {  // if not empty string
       words->push_back(string());  // start new empty word
       return 1;
     }
     return 0;
   }

   // Read in N-gram tokens as well as count (last token) from string
   double ReadNGramFromString(string str, vector<string> *words,
			      StateId *Init, StateId Unigram, StateId Start) {
     string::iterator strit = str.begin();
     if (!InitNewWord(words, &strit, &str)) {  // init first word, empty
       LOG(FATAL) << "NGramInput: empty line in file: format error";
     }
     while (strit < str.end()) {
       if (ws(*strit)) {
	 InitNewWord(words, &strit, &str);
       } else {
	 (*words)[words->size() - 1] += (*strit);  // add character to word
	 strit++;
       }
     }
     stringstream cnt_ss((*words)[words->size() - 1]);  // last token is count
     double ngram_count;
     cnt_ss >> ngram_count;
     CheckInitState(words, Init, Unigram, Start);  // Check start state status
     return -log(ngram_count);  // opengrm encoding of count is -log
   }

   // iterate through words in the n-gram history to find current state
   StateId GetHistoryState(vector<string> *words, vector<Label> *last_labels,
			   vector<StateId> *last_states, StateId st) {
     for (int i = 0; i < words->size() - 2; i++) {
       bool stsym, endsym;
       Label label = GetNGramLabel((*words)[i], 0, 0, &stsym, &endsym);
       if (label != (*last_labels)[i])  // should be from last n-gram
	 LOG(FATAL) << "NGramInput: n-gram prefix not seen in previous n-gram";
       st = (*last_states)[i];  // retrieve previously stored state
     }
     return st;
   }

   // When reading in final token of n-gram, determine the nextstate
   StateId GetCntNextSt(StateId st, StateId Unigram, StateId Init,
			StateId *Start, bool stsym, bool endsym) {
     StateId nextstate = -1;
     if (stsym) {  // start symbol: <s>
       if (st != Unigram)  // should not occur
	 LOG(FATAL) << "NGramInput: start symbol occurred in n-gram suffix";
       if (Init == Unigram)  // Don't know if model is order > 1 yet
	 (*Start) = fst_->AddState();  // Create state associated with <s>
       else  // Already created 2nd order Start state, stored as Init
	 (*Start) = Init;
       nextstate = (*Start);
     } else if (!endsym) {  // not a </s> symbol, hence need to create a state
       nextstate = fst_->AddState();
     }
     return nextstate;
   }

   // Update last label and state, for retrieval with following n-grams
   int UpdateLast(vector<string> *words, int longest_ngram,
		  vector<Label> *last_labels, vector<StateId> *last_states,
		  Label label, StateId nextst) {
     if (words->size() > longest_ngram + 1) {  // add a dimension to vectors
       longest_ngram++;
       last_labels->push_back(-1);
       last_states->push_back(-1);
     }
     (*last_labels)[words->size() - 2] = label;
     (*last_states)[words->size() - 2] = nextst;
     return longest_ngram;
   }

   // Read in a sorted n-gram count file, convert to opengrm format
   void CompileNGramCounts() {
     fst_ = new StdVectorFst();  // create new fst
     StateId Init = fst_->AddState(), Unigram = Init, Start = -1;
     int longram = 0;  // for keeping track of longest observed n-gram in file
     string str;
     vector<Label> last_labels;  // store labels from prior n-grams
     vector<StateId> last_states;  // store states from prior n-grams
     while (getline((*istrm_), str)) {  // for each string
       vector<string> words;
       double ngram_count =  // read in word tokens from string, and return cnt
	 ReadNGramFromString(str, &words, &Init, Unigram, Start);
       StateId st =  // find n-gram history state from prefix words in n-gram
	 GetHistoryState(&words, &last_labels, &last_states, Unigram);
       bool stsym, endsym;
       Label label =  // get label of word suffix of n-gram
	 GetNGramLabel(words[words.size() - 2], 1, 1, &stsym, &endsym);
       StateId nextst =  // Get the next state from history state and label
	 GetCntNextSt(st, Unigram, Init, &Start, stsym, endsym);
       AddNGramArc(st, nextst, label, stsym, endsym, ngram_count);  // add arc
       longram =  // update states and labels for subsequent n-grams
	 UpdateLast(&words, longram, &last_labels, &last_states, label, nextst);
     }
     if (Init == Unigram)  // Set Init as start state for unigram model
       fst_->SetStart(Init);
     AddBackoffAndCycles(Unigram);  // turn into reentrant opengrm format
     DumpFst();
   }

   // Tokenize string and store labels in a vector for building an fst
   double FillStringLabels(string *str, vector<Label> *labels,
			   bool string_counts) {
     string token = "";
     string::iterator strit = str->begin();
     double count = 1;
     if (string_counts) {
       GetWhiteSpaceToken(&strit, str, &token);
       count = atof(token.c_str());
       token = "";
     }
     while (GetWhiteSpaceToken(&strit, str, &token)) {
       labels->push_back(GetLabel(token, 1, 1));  // store index
       token = "";
     }
     return count;
   }

   // Compile a given string into a linear fst
   void CompileStringFst(string *str, bool string_counts) {
     fst_ = new StdVectorFst();  // create new fst
     StateId Init = fst_->AddState();  // create init state
     fst_->SetStart(Init);
     StateId fi = fst_->AddState();  // create final state
     StateId s = Init;  // current state=init
     vector<Label> labels;
     double count = FillStringLabels(str, &labels, string_counts);
     if (string_counts) {
       fst_->SetFinal(fi, -log(count));
     } else {
       fst_->SetFinal(fi, Weight::One());
     }
     for (int i = 0; i < labels.size(); i++) {  // for each word
       StateId ns;
       if (i < labels.size() - 1)  // if not last word add state
	 ns = fst_->AddState();
       else  // otherwise next=final
	 ns = fi;
       fst_->AddArc(s, StdArc(labels[i], labels[i], Weight::One(), ns));
       s = ns;  // new origin state = destination st
     }
   }

   // From text corpus to symbol table
   void CompileSymbolTable() {
     for (string str; getline((*istrm_),str); ) {
       vector<Label> labels;
       FillStringLabels(&str, &labels, 0);
     }
     if (!OOV_symbol_.empty())
       syms_->AddSymbol(OOV_symbol_);
     syms_->WriteText(*ostrm_);
   }

   // Write resulting fst to specified stream
   void DumpFst() {
     if (incl_symbols_) {
       fst_->SetInputSymbols(syms_);
       fst_->SetOutputSymbols(syms_);
     }
     fst_->Write(*ostrm_, FstWriteOptions());
   }

   MutableFst<Arc> *fst_;
   SymbolTable *syms_;
   bool add_symbols_;
   string OOV_symbol_;
   string start_symbol_;
   string end_symbol_;
   istream *istrm_;
   ostream *ostrm_;
   bool own_istrm_;
   bool own_ostrm_;
   string bo_ilabel_;
   string bo_olabel_;
   int bo_ilabel_id_;
   int bo_olabel_id_;
   bool incl_symbols_;
 };

}  // namespace dcd

#endif  // DCD_NGRAM_INPUT_H__
