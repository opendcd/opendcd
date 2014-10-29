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

