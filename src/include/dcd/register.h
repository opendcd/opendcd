// register.h
// \file
// Decoder register

#ifndef DCD_REGISTER_H__
#define DCD_REGISTER_H__

#include <dcd/constants.h>

namespace dcd {

class ParseOptions;
class SearchOptions;

template<class TransModel, class B>
int ArcDecoderMain(ParseOptions &po, SearchOptions *opts, 
    const string &word_symbols_file);


struct DecodeMainEntryBase {
  virtual int Run(ParseOptions &po, SearchOptions *opts, 
                  const string &word_symbols_file) = 0;
  virtual ~DecodeMainEntryBase() = 0;
};

DecodeMainEntryBase::~DecodeMainEntryBase() {
}

//extern unordered_map<string, DecodeMainEntryBase*> main_map;

template<class T, class B>
struct DecodeMainEntry : public DecodeMainEntryBase {
  virtual int Run(ParseOptions &po, SearchOptions *opts, 
                  const string &word_symbols_file) {
    return ArcDecoderMain<T, B>(po, opts, word_symbols_file);
  }
};


void ClearMainRegister() {
  for (typename unordered_map<string, DecodeMainEntryBase*>::iterator it 
      =  main_map.begin(); it != main_map.end(); ++it ) {
    delete it->second;
  }
  main_map.clear();
}

DecodeMainEntryBase* FindEntry(const string& key) {
  typename unordered_map<string, DecodeMainEntryBase*>::iterator it = 
    main_map.find(key);
  if (it == main_map.end()) { 
  #ifdef HAVEDL
    logger(INFO) << "Attempting to load shared object : " << key << "-dcd.so";
    void* handle = dlopen(key + "-dcd.so", RTLD_LAZY);
    if (!handle)
      logger(FATAL) << "FindEntry : " << dlerror();
    DecodeMainEntryBase* entry = FindEntry(key);
    if (!entry)
      logger(FATAL) <<"Entry lookup failed in : " << key << "-dcd.so";
    return entry;
  #endif
    return 0;
  } else {
    return it->second;
  }
}

template<class T, class B>
struct DecodeMainRegisterer {
  explicit DecodeMainRegisterer(const string& name) {
    main_map[name] = new DecodeMainEntry<T, B>;
  }
};

#define REGISTER_DECODER_MAIN(S, T, D, B) \
  static DecodeMainRegisterer<T<D>, B> \
    decode_registerer ## _ ## T ## _ ## D ## _ ## B(S);

//Idea for a wrapper class around the decoder to hide
//the underlying template types

class DecoderClassImplBase {
 public:
  template<class F>
  virtual float Decode() = 0;
};

template<class Arc, class Trans, class L = Lattice> 
class DecoderClassImpl {
 public:
   template<class F> 
   virtual float Decode() {
   };
 //ArcDecoder<Arc, Trans, L>;
};

class DecoderClass {
 public:

  template<class F>
  virtual float Decode(const F& f) {
    return impl_-Deocde(f);
  }

  template<class Arc, class Trans, class L = Lattice> 
  const ArcDecoder<Arc, Trans, L>*  GetDecoder() {
    string str = Arc::Type() + "_" + Trans::Type(); + "_" + L::Type();
    if (str != Type()) {
      return NULL;
    } else {
      typedef DecoderClassImpl<Arc, Trans, L>* D;
      D* dec =static_cast<D*>(impl_);
      return impl_->GetFst();
    }
  }
 private:
  DecoderClassBaseImpl* impl_;
};

} //namespace dcd

#endif //DCD_REGISTER_H__

