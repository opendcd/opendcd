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
					-Wno-deprecated-writable-strings -DOS_LINUX  -std=c++0x -g
CXXFLAGS+=-DMEMDEBUG 

