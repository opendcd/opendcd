#Compiler settings

# Intelligent relative dir handling
# See: http://stackoverflow.com/questions/322936
# for an explanatino of what is happening
TOP := $(dir $(lastword $(MAKEFILE_LIST)))

CC=gcc
CXX=g++

DOTGITDIR+=../../.git
CXXFLAGS+=-I../include/ -I../../3rdparty/openfst-src/include/ \
					-Wno-sign-compare -Wno-dangling-else \
					-Wno-deprecated-writable-strings -DOS_LINUX -O2 -std=c++0x -g

LIBDIR=$(shell readlink -f ../../3rdparty)/openfst-src/lib/
LDFLAGS+= -ldl -L$(LIBDIR) -L$(LIBDIR)/fst
LDFLAGS+= -Wl,-rpath -Wl,$(LIBDIR) -Wl,-rpath -Wl,$(LIBDIR)/fst/


#CXXFLAGS+=-DMEMDEBUG 

#CXXFLAGS+= -DHAVEJEMALLOC -I/Users/dixonp/installed/include/
#LDFLAGS+=-L/Users/dixonp/installed/lib/ -ljemalloc
