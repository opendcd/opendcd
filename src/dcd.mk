#Compiler settings

# Intelligent relative dir handling
# See: http://stackoverflow.com/questions/322936
# for an explanatino of what is happening
TOP := $(dir $(lastword $(MAKEFILE_LIST)))

CC=gcc
CXX=g++

DOTGITDIR+=../../.git

#Add -stdlib=libstdc++  to CXXFLAGS and LDFLAGS for OSX 10.9

CXXFLAGS+=-I../include/ \
					-Wno-sign-compare -Wno-dangling-else \
					-Wno-deprecated-writable-strings -DOS_MACOSX -O2 -std=c++11 -g

#					-I/Users/dixonp/tools/kaldi-trunk-latest/src/
#CXXFLAGS+=-DNODEBUG -march=native -fomit-frame-pointer -funroll-loops
#CXXFLAGS+=-DUSERDESTL
#-g
CXXFLAGS+=
#-stdlib=libstdc++ 
LDFLAGS+=
#-stdlib=libstdc++ 
LDFLAGS+= -ldl -L/usr/local/lib/fst/ -L/usr/local/lib/ -g
#-g

# -stdlib=libstdc++ 
#CXXFLAGS+=-fopenmp -lpthread 
#LDFLAGS+=-fopenmp -lpthread 

#CXXFLAGS+=-I../../3rdparty/Shiny/include/ -DHAVESHINY 
#LDFLAGS+=-L../../3rdparty/Shiny/lib/ -lshiny

CXXFLAGS+=-DMEMDEBUG 
#CXXFLAGS+= -DHAVEJEMALLOC -I/Users/dixonp/installed/include/
#LDFLAGS+=-L/Users/dixonp/installed/lib/ -ljemalloc
