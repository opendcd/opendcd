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
					-Wno-deprecated-writable-strings -DOS_MACOSX -O2 -std=c++11 -g

CXXFLAGS+=-DMEMDEBUG 
LDFLAGS+= -ldl -g

#CXXFLAGS+=-fopenmp -lpthread 
#LDFLAGS+=-fopenmp -lpthread 
#CXXFLAGS+=-I../../3rdparty/Shiny/include/ -DHAVESHINY 
#LDFLAGS+=-L../../3rdparty/Shiny/lib/ -lshiny
#CXXFLAGS+= -DHAVEJEMALLOC -I/Users/dixonp/installed/include/
#LDFLAGS+=-L/Users/dixonp/installed/lib/ -ljemalloc
