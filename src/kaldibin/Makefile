ifndef KALDI_ROOT
$(error KALDI_ROOT is not set)
endif

all: make-arc-types lattice-to-far dcd-recog-kaldi

VPATH=../lib:../bin

include $(KALDI_ROOT)/src/kaldi.mk
UNAME_S=$(shell uname)
ifeq ($(UNAME_S),Darwin)
  CXXFLAGS+=-DOS_MACOSX
else
  CXXFLAGS+=-DOS_LINUX
endif
DOTGITDIR+=../../.git
CXXFLAGS+=-I$(KALDI_ROOT)/src/ -Wno-mismatched-tags -DHAVE_KALDI -I../include/
FSTLIBDIR=$(KALDI_ROOT)/tools/openfst/

gitrevision.cc: ${DOTGITDIR}/HEAD ${DOTGITDIR}/index
	echo "const char* g_dcd_gitrevision = \"$(shell git rev-parse HEAD)\";" > $@

compiler-version.cc: Makefile ${MK}
	echo "const char* g_dcd_compiler_version  = \"$(shell $(CXX) --version)\";" > $@

compiler-flags.cc: Makefile ${MK}
	echo  "const char* g_dcd_cflags = \"$(CXXFLAGS)\"; const char* g_dcd_lflags =\"$(LDFLAGS) $(LDLIBS)\";" > $@


make-arc-types: make-arc-types.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) \
  $(KALDI_ROOT)/src/lat/kaldi-lat.a \
	$(KALDI_ROOT)/src/util/kaldi-util.a \
	$(KALDI_ROOT)/src/gmm/kaldi-gmm.a \
	$(KALDI_ROOT)/src/hmm/kaldi-hmm.a \
	$(KALDI_ROOT)/src/tree/kaldi-tree.a \
  $(KALDI_ROOT)/src/base/kaldi-base.a \
	$(KALDI_ROOT)/src/matrix/kaldi-matrix.a \
	$(ATLASLIBS)

lattice-to-far: lattice-to-far.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) \
  $(KALDI_ROOT)/src/lat/kaldi-lat.a \
	$(KALDI_ROOT)/src/util/kaldi-util.a \
	$(KALDI_ROOT)/src/gmm/kaldi-gmm.a \
	$(KALDI_ROOT)/src/hmm/kaldi-hmm.a \
	$(KALDI_ROOT)/src/tree/kaldi-tree.a \
  $(KALDI_ROOT)/src/base/kaldi-base.a \
	$(KALDI_ROOT)/src/matrix/kaldi-matrix.a \
	$(ATLASLIBS)

dcd-recog-kaldi: dcd-recog.o text-utils.o log.o utils.o compiler-flags.o \
	memdebug.o compiler-version.o cpu-stats.o config.o feat-readers.o gitrevision.o
	$(CXX)  $^ -o $@  -L$(FSTLIBDIR)/lib/fst $(LDFLAGS) $(LDLIBS) \
	$(KALDI_ROOT)/src/lat/kaldi-lat.a \
	$(KALDI_ROOT)/src/feat/kaldi-feat.a \
	$(KALDI_ROOT)/src/transform/kaldi-transform.a \
	$(KALDI_ROOT)/src/gmm/kaldi-gmm.a \
	$(KALDI_ROOT)/src/hmm/kaldi-hmm.a \
	$(KALDI_ROOT)/src/tree/kaldi-tree.a \
	$(KALDI_ROOT)/src/matrix/kaldi-matrix.a \
	$(KALDI_ROOT)/src/thread/kaldi-thread.a \
	$(KALDI_ROOT)/src/util/kaldi-util.a \
	$(KALDI_ROOT)/src/base/kaldi-base.a \
	$(ATLASLIBS)


clean:
	rm -rf *.o make-arc-types dcd-recog-kaldi

%.o:%.cc
	$(CXX) $(CXXFLAGS) -c $<

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c $<


