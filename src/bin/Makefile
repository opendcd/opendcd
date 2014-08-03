#all: dcd-decode fstxprintstrings
all: dcd-recog farfilter shiny \
	   farprintnbeststrings 
	
	#arc-expand dcd-lexicon 

#Allow this to be specified for the platform
include ../dcd.mk

includes = $(wildcard $(TOP)/include/dcd/*.h)

CXXFLAGS+=-I../../3rdparty/
VPATH=../lib:../../3rdparty/exec-stream:../../3rdparty/gzstream/

gitrevision.cc: ${DOTGITDIR}/HEAD ${DOTGITDIR}/index
	echo "const char* g_dcd_gitrevision = \"$(shell git rev-parse HEAD)\";" > $@

compiler-version.cc: Makefile ../dcd.mk
	echo "const char* g_dcd_compiler_version  = \"$(shell $(CXX) --version)\";" > $@

compiler-flags.cc: Makefile ../dcd.mk
	echo  "const char* g_dcd_cflags = \"$(CXXFLAGS)\"; const char* g_dcd_lflags =\"$(LDFLAGS) $(LDLIBS)\";" > $@

dcd-recog: dcd-recog.o parse-options.o text-utils.o log.o utils.o gitrevision.o compiler-flags.o \
	memdebug.o compiler-version.o cpu-stats.o config.o feat-readers.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfar 

dcd-recg.cc: ../include/dcd/arc-decoder.h

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
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfar

far-to-lattice: far-to-lattice.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfar -lfstfarscript

lattice4-arc.so: kaldi-lattice-arc-reg.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfar -lfstfarscript -shared 

shiny:
	$(MAKE) -C ../../3rdparty/Shiny

clean:
	rm -rf *.o dcd-recog arc-expand dcd-lexicon dcd-ngram compiler-flags.cc compiler-version.cc

%.o:%.cc ${includes}
	$(CXX) $(CXXFLAGS) -c $<

%.o:%.cpp ${includes}
	$(CXX) $(CXXFLAGS) -c $<
