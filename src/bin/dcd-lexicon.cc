//Main lexicon generation command
#include <iostream>

#include <dcd/log.h>
#include <dcd/parse-options.h>
#include <dcd/lexicon.h>

using namespace std;
using namespace dcd;
using namespace fst;


int main(int argc, char *argv[]) {

  const char *usage = "Efficiently generate a lexicona and all ancillary "
  "information.\nUsage: dcd-lexicon [options] pronunciation-lexicon "
  "lexicon.fst";

  ParseOptions po(usage);
  string prefix        = "";
  string eps           = "<eps>";
  string sil           = "SIL";
  string isymbols      = "";
  string osymbols      = "";
  bool   pos           = false;
  bool   det           = true;
  bool   opt           = true;
  bool   closure       = false;
  bool   keep_isymbols = false;
  bool   keep_osymbols = false;
  bool   do_sil        = false;
  double silprob       = 1.0;
  string linereturn    = "\n\t\t\t\t";

  po.Register("prefix", &prefix, "Output files prefix.\n\t\t\t\t");
  po.Register("eps", &eps, "Epsilon symbol.\n\t\t\t\t");
  po.Register("sil", &sil, "Silence phoneme label.\n\t\t\t\t");
  po.Register("isymbols", &isymbols, "Input symbols file, if any.\n\t\t\t\t"
              "Will be auto-generated from the lexicon if not supplied."
              "\n\t\t\t\t");
  po.Register("osymbols", &osymbols, "Output symbols file, if any.\n\t\t\t\t"
              "Will be auto-generated from the lexicon if not supplied."
              "\n\t\t\t\t");
  po.Register("pos", &pos, "Generate positional phoneme information."
              "\n\t\t\t\t");
  po.Register("det", &det, "Generate the lexicon FST as a deterministic tree."
              "\n\t\t\t\tThis is is typically faster and more efficient, "
              "especially if the \n\t\t\t\tlexicon is to be used in the " 
              "context of lookahead composition.\n\t\t\t\t");
  po.Register("opt", &opt, "Optimize the resulting lexicon.  Push labels and"
              "\n\t\t\t\tweights in the log semiring, and epsilon remove "
              "the result.\n\t\t\t\t");
  po.Register("closure", &closure, "Compute the closure of the lexicon."
              "\n\t\t\t\t");
  po.Register("keep-isymbols", &keep_isymbols, "Store input symbols in the "
              "FST.\n\t\t\t\tUseful in some instances for drawing and "
              "downstream operations.\n\t\t\t\t");
  po.Register("keep-osymbols", &keep_osymbols, "Store output symbols in the "
              "FST.\n\t\t\t\tUseful in some instances for drawing and "
              "downstream operations.\n\t\t\t\t");
  po.Register("do-sil", &do_sil, "Add optional silences to the lexicon."
              "\n\t\t\t\t");
  po.Register("silprob", &silprob, "Probability of optional silence."
              "\n\t\t\t\t");
  
  po.Read(argc, argv);
              
  if (po.NumArgs() != 2) {
    po.PrintUsage();
    exit(1);
  }

  string pronunciation_lexicon = po.GetArg(1);
  string lexicon_fst           = po.GetArg(2);
  
  Logger lexlog("dcd-lexicon", std::cerr);
 
  lexlog(INFO) << "Attempting to load lexicon: " << pronunciation_lexicon;

  SymbolTable* isyms = ( isymbols=="" ? new SymbolTable("isyms")
                         : SymbolTable::ReadText(isymbols) );
  SymbolTable* osyms = ( osymbols=="" ? new SymbolTable("osyms")
                         : SymbolTable::ReadText(osymbols) );
  
  dcd::LexiconFst<StdArc,TropicalWeight>
  lex( pronunciation_lexicon, eps, sil, silprob,
       pos, det, do_sil, *isyms, *osyms,
       lexlog, closure );

  string ofile = "";
  lex.LoadLexicon();
  lex.GenerateFst();
  if( closure == true )
    lex.CloseLexicon();
  
  if( lexicon_fst == "-" )
    lex.Write("");
  else
    lex.Write( ( ( prefix == "" ) ? "" : prefix+"." )+lexicon_fst );

  ofstream ofp( ( prefix=="" ? "disambig.int" : prefix+".disambig.int" ).c_str() );
  for( set<int>::iterator it=lex.disambig_int_.begin();
       it != lex.disambig_int_.end(); it++ )
    ofp << *it << endl;
  ofp.close();
  
  delete isyms;
  delete osyms;
}
