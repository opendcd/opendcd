SRCDIR=openfst-src
GRMDIR=opengrm-src
#all: openfst Shiny
all: openfst
PWD=$(shell pwd)
LOCAL=$(PWD)/local

$(SRCDIR): | openfst-src.tar.gz
	mkdir -p $(SRCDIR)
	tar -zxf openfst-src.tar.gz -C $(SRCDIR) --strip=1

opengrm: opengrm_compiled

opengrm_compiled: opengrm-src/Makefile
	cd opengrm-src && $(MAKE) -j install

opengrm-src/Makefile: $(GRMDIR)
	cd opengrm-src/; ./configure --prefix=$(LOCAL) \
		CXXFLAGS=-I$(PWD)/local/include/ \
		LDFLAGS="-L$(PWD)/local/lib/ -L$(PWD)/local/lib/fst/"

$(GRMDIR): | opengrm-src.tar.gz
	mkdir -p $(GRMDIR)
	tar -zxf opengrm-src.tar.gz -C $(GRMDIR) --strip=1

opengrm-src.tar.gz:
	curl "http://openfst.cs.nyu.edu/twiki/pub/GRM/NGramDownload/opengrm-ngram-1.2.1.tar.gz" -o opengrm-src.tar.gz

openfst-src.tar.gz:
	curl "http://openfst.org/twiki/pub/FST/FstDownload/openfst-1.4.1.tar.gz" -o openfst-src.tar.gz

openfst-src/Makefile: $(SRCDIR)
	cd openfst-src/; ./configure --prefix=$(LOCAL) \
	--enable-static --enable-bin --enable-compact-fsts --enable-const-fsts --enable-far \
	--enable-linear-fsts --enable-lookahead-fsts --enable-ngram-fsts --enable-pdt --enable-bin

.PHONY: openfst # so target will be made even though "openfst" exists.

openfst: openfst_compiled 

.PHONY: openfst_compiled
openfst_compiled: openfst-src/Makefile
	cd openfst-src && $(MAKE) -j install

Shiny :
	$(MAKE) -j -C $@

clean:
	rm openfst-src/Makefile

clobber: clean
	rm -rf $(SRCDIR)
	rm -f openfst-src.tar.gz

