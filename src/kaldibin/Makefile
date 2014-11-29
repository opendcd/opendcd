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

online2-wav-nnet2-am-compute2: online2-wav-nnet2-am-compute2.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) \
	$(KALDI_ROOT)/src/online2/kaldi-online2.a \
  $(KALDI_ROOT)/src/ivector/kaldi-ivector.a \
  $(KALDI_ROOT)/src/nnet2/kaldi-nnet2.a \
	$(KALDI_ROOT)/src/lat/kaldi-lat.a \
  $(KALDI_ROOT)/src/decoder/kaldi-decoder.a \
  $(KALDI_ROOT)/src/cudamatrix/kaldi-cudamatrix.a \
  $(KALDI_ROOT)/src/feat/kaldi-feat.a \
  $(KALDI_ROOT)/src/transform/kaldi-transform.a \
  $(KALDI_ROOT)/src/gmm/kaldi-gmm.a \
  $(KALDI_ROOT)/src/thread/kaldi-thread.a \
  $(KALDI_ROOT)/src/hmm/kaldi-hmm.a \
  $(KALDI_ROOT)/src/tree/kaldi-tree.a \
  $(KALDI_ROOT)/src/matrix/kaldi-matrix.a \
  $(KALDI_ROOT)/src/util/kaldi-util.a \
  $(KALDI_ROOT)/src/base/kaldi-base.a \
	$(ATLASLIBS)

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

lattice4-arc.so: lattice-arc.cc
	$(CXX) -I$(FSTLIBDIR)/include -I$(KALDI_ROOT)/src/ -DHAVE_ATLAS -fPIC $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfar -lfstfarscript -shared

compact-lattice4-arc.so: compact-lattice-arc.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst -lfstfar -lfstfarscript -shared

clean:
	rm -rf *.o make-arc-types dcd-recog-kaldi online2-wav-nnet2-am-compute2

%.o:%.cc
	$(CXX) $(CXXFLAGS) -c $<

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c $<


