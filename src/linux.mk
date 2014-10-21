#Compiler settings

# Intelligent relative dir handling
# See: http://stackoverflow.com/questions/322936
# for an explanatino of what is happening
TOP := $(dir $(lastword $(MAKEFILE_LIST)))

CC=gcc
CXX=g++

DOTGITDIR+=../../.git
CXXFLAGS+=-I../include/ \
					-Wno-sign-compare -Wno-dangling-else \
					-Wno-deprecated-writable-strings -DOS_LINUX -O2 -std=c++11 -g

LDFLAGS+= -ldl -L../../3rdparty/openfst-src/lib/fst/ -L../../3rdparty/openfst-src/lib/

CXXFLAGS+=-DMEMDEBUG 

#CXXFLAGS+= -DHAVEJEMALLOC -I/Users/dixonp/installed/include/
#LDFLAGS+=-L/Users/dixonp/installed/lib/ -ljemalloc
