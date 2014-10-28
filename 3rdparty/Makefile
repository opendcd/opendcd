SRCDIR=openfst-src

all: openfst Shiny

$(SRCDIR): | openfst-src.tar.gz
	mkdir -p $(SRCDIR)
	tar -zxf openfst-src.tar.gz -C $(SRCDIR) --strip=1

openfst-src.tar.gz:
	curl "http://openfst.org/twiki/pub/FST/FstDownload/openfst-1.4.1.tar.gz" -o openfst-src.tar.gz

openfst-src/Makefile: $(SRCDIR)
	cd openfst-src/; ./configure --prefix=`pwd` \
	--enable-static --enable-bin --enable-compact-fsts --enable-const-fsts --enable-far \
	--enable-linear-fsts --enable-lookahead-fsts --enable-ngram-fsts --enable-pdt --enable-bin \
	CXX=$(CXX) CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" LIBS="-ldl"

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

