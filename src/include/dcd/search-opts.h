// search-opts.h
// \file
// Search optiosn

#ifndef DCD_SEARCH_OPTS_H__
#define DCD_SEARCH_OPTS_H__

#include <dcd/config.h>
#include <dcd/constants.h>

namespace fst {
 class SymbolTable;
}

namespace dcd {
  
struct Variant {
  
  Variant() : type(kNone) { } 

  explicit Variant(int *value) : i(value), type(kInt)  { }

  explicit Variant(float *value) : f(value), type(kFloat) { }

  explicit Variant(bool *value) : b(value), type(kBool) { } 
  
  explicit Variant(const Variant& other) : v(other.v), type(other.type) { } 
  
  static const int kInt = 0;
  static const int kFloat = 1;
  static const int kBool = 2;
  static const int kNone = 3;
  union {
    int* i; float* f;  bool* b; void* v;
  };
  int type;
  friend std::ostream& operator<<(std::ostream& os, const Variant& value);
};

std::ostream& operator << (std::ostream& os, const Variant& value) {
  switch (value.type) {
    case Variant::kInt: os << *value.i; break;
    case Variant::kFloat: os << *value.f; break;
    case Variant::kBool: os << ((*value.b) ? "true" : "false"); break;
    default: os << "Null"; 
  }
  return os;
}


//Struct containing the parameters for the search. Feel like
//there is a better way to do this.

struct SearchOptions {

  struct Element {
    string flag;
    string description;
    Variant value;
  };

  SearchOptions() {
    Init(&beam, kDefaultBeam, "beam");
    Init(&band, kMaxArcs, "band");
    Init(&acoustic_scale, kDefaultAcousticScale, "acoustic_scale");
    Init(&trans_scale, kDefaultTranScale, "trans_scale");
    Init(&use_lattice_pool, false, "use_lattice_pool");
    Init(&cache_destinatation_states, true, "cache_dest_states");
    Init(&gc_period, kDefaultGcPeriod, "gc_period");
    Init(&gc_check, false, "gc_check");
    //"Peform checking of the allocated states. "
    //                       "(Will cause substantial slow downs)"
    Init(&gen_lattice, false, "gen_lattice"); //Generate recognition lattices"
    Init(&lattice_beam, kDefaultBeam, "lattice_beam");
    Init(&use_search_pool, false, "use_search_pool");
    Init(&fst_reset_period, 32, "fst_reset_period");
    Init(&early_mission, false, "early_mission");
    Init(&dump_traceback, false, "dump_traceback");
    Init(&acoustic_lookahead, 0.0f, "acoustic_lookahead");
	  Init(&colorize, true, "colorize");
    Init(&prune_eps, true, "prune_eps");
    Init(&nbest, 0, "nbest");
    Init(&insertion_penalty, 0.0f, "insertion_penalty");
  }

  float beam;
  int band;
  int nbest;
  float insertion_penalty;
  float acoustic_lookahead;
  float acoustic_scale;
  float trans_scale;
  float lattice_beam;
  const SymbolTable* wordsyms;
  const SymbolTable* phonesyms;
  bool use_lattice_pool;
  bool cache_destinatation_states;
  int gc_period;
  int fst_reset_period;
  bool gc_check;
  bool gen_lattice;
  bool use_search_pool;
  bool early_mission;
  bool dump_traceback;
  bool colorize;
  bool prune_eps;
  string source; //File name of input
  std::map<std::string, Variant> params;
  
  template<class T>
  void Init(T* t, const T& def, const std::string& name) {
    *t = def;
    Variant v(t);
    params[name] = v;
  }

  friend std::ostream& operator<<(std::ostream& os, const SearchOptions& opts);

  void Check() {
    lattice_beam = min(lattice_beam, beam);
  }
  
  void Register(ParseOptions* po) {
    for (std::map<std::string, Variant>::iterator it = params.begin();
         it != params.end(); ++it) {
      Variant& value = it->second;   
      switch (value.type) {
          case Variant::kInt: po->Register(it->first, value.i, ""); break;
          case Variant::kFloat: po->Register(it->first, value.f, ""); break;
          case Variant::kBool: po->Register(it->first, value.b, ""); break;
        }
    }
 }
 private:
  DISALLOW_COPY_AND_ASSIGN(SearchOptions);
};

std::ostream& operator << (std::ostream& os, const SearchOptions& opts) {
  for (std::map<string, Variant>::const_iterator it = opts.params.begin(); 
         it != opts.params.end(); ++it)
    os << "\t\t  " << it->first << " : " << it->second << endl;
  return os;
}
} //namespace dcd

#endif
