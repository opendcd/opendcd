ifndef KALDI_ROOT
$(error KALDI_ROOT is not set)
endif

all: make-arc-types

include $(KALDI_ROOT)/src/kaldi.mk
CXXFLAGS+=-I${KALDI_ROOT}/src/ -Wno-mismatched-tags

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

clean:
	rm -rf *.o make-arc-types

%.o:%.cc
	$(CXX) $(CXXFLAGS) -c $<

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c $<

#$(KALDI_ROOT)/src/hmm/kaldi-hmm.a 
