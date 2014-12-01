all: dcd-recog far-to-lattice farprintnbeststrings farfilter 

#Select the Makefile based on the platform
UNAME_S=$(shell uname)
ifeq ($(UNAME_S),Darwin)
  MK=../darwin.mk
else
  MK=../linux.mk
endif
include ${MK}

includes = $(wildcard $(TOP)/include/dcd/*.h)

CXXFLAGS+=-I../../3rdparty/
VPATH=../lib:../../3rdparty/exec-stream
FSTLIBDIR=$(shell cd ../../3rdparty/local/lib; pwd)
LDFLAGS+= -ldl -L$(FSTLIBDIR) -L$(FSTLIBDIR)/fst
LDFLAGS+= -Wl,-rpath -Wl,$(FSTLIBDIR) -Wl,-rpath -Wl,$(FSTLIBDIR)/fst/

gitrevision.cc: ${DOTGITDIR}/HEAD ${DOTGITDIR}/index
	echo "const char* g_dcd_gitrevision = \"$(shell git rev-parse HEAD)\";" > $@

compiler-version.cc: Makefile ${MK}
	echo "const char* g_dcd_compiler_version  = \"$(shell $(CXX) --version)\";" > $@

compiler-flags.cc: Makefile ${MK}
	echo  "const char* g_dcd_cflags = \"$(CXXFLAGS)\"; const char* g_dcd_lflags =\"$(LDFLAGS) $(LDLIBS)\";" > $@

dcd-recog: dcd-recog.o parse-options.o text-utils.o log.o utils.o gitrevision.o compiler-flags.o \
	memdebug.o compiler-version.o cpu-stats.o config.o feat-readers.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfarscript

dcd-recog-profile: dcd-recog.o parse-options.o text-utils.o log.o utils.o gitrevision.o compiler-flags.o \
	memdebug.o compiler-version.o cpu-stats.o config.o feat-readers.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfar

dcd-recog.cc: ../include/dcd/arc-decoder.h

dcd-lex: dcdlex.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfarscript

fstxprintstrings: fstxprintstrings.o
	$(CXX) $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstscript

farprintnbeststrings: farprintnbeststrings.o text-utils.o
	$(CXX) $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstscript -lfstfar -lfstfarscript

fstconfidence: fstconfidence.o
	$(CXX) $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstscript

arc-expand: arc-expand.o utils.o
	$(CXX) $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst 

dcd-lexicon: dcd-lexicon.o parse-options.o text-utils.o log.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst 

farfilter: farfilter.o exec-stream.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfar -lfstfarscript -lpthread

far-to-lattice: far-to-lattice.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfar -lfstfarscript

lattice4-arc.so: kaldi-lattice-arc-reg.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfar -lfstfarscript -shared 

olabel_cache_lookahead-fst.so: olabel_cache_lookahead-fst.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfar -lfstfarscript -shared 


shiny:
	$(MAKE) -C ../../3rdparty/Shiny

clean:
	rm -rf *.o dcd-recog arc-expand dcd-lexicon dcd-ngram compiler-flags.cc compiler-version.cc

%.o:%.cc ${includes}
	$(CXX) $(CXXFLAGS) -c $<

%.o:%.cpp ${includes}
	$(CXX) $(CXXFLAGS) -c $<
