# Shiny Profiler
# GNU Makefile for Linux
#
# By default libShiny.a will be compiled with g++ C++ compiler. If you prefer
# or need a plain C version use the 'cclib' target. The C compiled lib should
# be compatible to C AND C++ code according to ISO C99 and ISO C++98 (or
# later).
#
# To build Shiny as a static C++ lib use:
#
# make
#
# To build it as a static C lib (ISO C99) use:
#
# make cclib
#
# For building libs including more debug support use:
#
# make cxxdebug
# or
# make ccdebug
#
# If you experience incompatibilities please feel free to tailor this makefile
# to your needs and report problems here:
# - sourceforge: http://sourceforge.net/projects/shinyprofiler/
# - Google Code: http://code.google.com/p/shinyprofiler/


SRCS		= $(wildcard src/*.c)
OBJS		= $(SRCS:.c=.o)

LIB_NAME	= libShiny.a
LIB_PATH	= lib
LIB_PATH_DEBUG	= lib/debug


##################
# Compiler options
FLAGS		= -g -O3 -fPIC -Wall
INCLUDE		= -Iinclude

# C options
cclib ccdebug: CCOMP = $(CC)
cclib: FLAGS += -std=c99

# C++ options
cxxlib cxxdebug: CCOMP = $(CXX)

# Debug options
ccdebug cxxdebug: FLAGS = -g -ggdb3 -O0 -Wall
ccdebug cxxdebug: LIB_PATH = $(LIB_PATH_DEBUG)


##################
# Target definitions

# All target builds static C++ lib
all: cxxlib

# Copy static lib into lib/ or lib/debug
cclib cxxlib ccdebug cxxdebug: $(LIB_NAME)
	mkdir -p $(LIB_PATH)
	mv $^ $(LIB_PATH)

# Build static lib in either C or C++
$(LIB_NAME): $(OBJS)
	$(AR) rcs $@ $^

# Create object files from source code
.c.o:
	$(CCOMP) $(FLAGS) $(INCLUDE) -c $< -o $@


# Remove temporary files and compilation results
clean:
	$(RM) $(OBJS) $(LIB_PATH)/$(LIB_NAME) $(LIB_PATH_DEBUG)/$(LIB_NAME) 


help:
	@echo "Available targets for Shiny"
	@echo " cxxlib: Build static library libShiny.a using C++ compiler"
	@echo " cclib: Build static library libShiny.a using C compiler"
	@echo ""
	@echo " cxxdebug: Build static library libShiny.a with debug symbols using C++ compiler"
	@echo " ccdebug: Build static library libShiny.a with debug symbols using C compiler"
	@echo ""
	@echo " clean: Clean up directories"

.PHONY: all cclib cxxlib ccdebug cxxdebug help clean
