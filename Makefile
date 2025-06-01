######################################
##	     ICO COMPILE OPTIONS        ##
######################################

# Ico compiling options. Uncomment or comment these lines to enable/disable
# these options.

# Use computed gotos for dispatching instead of switch dispatch.
# Please note that debug build always uses switch dispatch.
USE_GOTO := 1

# Use libedit line editor. This requires libedit to be installed.
USE_LIBEDIT := 1

######################################
##		   COMPILE SETTINGS         ##
######################################

# You don't need to change anything under this line

# Compiler options
CC = gcc
CFLAGS = -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function

# If use libedit
ifdef USE_LIBEDIT
L_LIBEDIT = -ledit
D_LIBEDIT = -DUSE_LIBEDIT
endif

# Library flags ("-lm": <math.h>)
LFLAGS = -lm $(L_LIBEDIT)

# Flags for debug build
DFLAGS = -g -Og -DDEBUG -std=c17 -DSWITCH_DISPATCH $(D_LIBEDIT)

ifdef USE_GOTO
# Flags for release build that uses GCC's labels as values extension
# for computed gotos dispatching (similar to Lua's jump table).
# "-fno-gcse" is needed for GCC to not optimize away the gotos.
RFLAGS = -O3 -fno-gcse -std=gnu17 $(D_LIBEDIT)
else
# Flags for release build that only uses ANSI C (ie. switch dispatch)
RFLAGS = -O3 -flto -std=c17 -DSWITCH_DISPATCH $(D_LIBEDIT)
endif

# Files
SRC_DIR = source
HEADERS = $(wildcard $(SRC_DIR)/ico_*.h)
SOURCES = $(wildcard $(SRC_DIR)/ico_*.c) main.c
DEBUG_OBJS = $(addprefix build/debug/, $(notdir $(SOURCES:.c=.o)))
RELEASE_OBJS = $(addprefix build/release/, $(notdir $(SOURCES:.c=.o)))

.PHONY: default all debug release clean clean_debug clean_release

default: release

all: debug release

debug: build/icod

release: build/ico

# Makefile syntax:
# - $@ is target, $< is first prerequisite, $^ is all prerequisites.
#
# - For pattern rules (like "build/debug/%.o"), both the target and
#   the prerequisites must match for the rule to match (ie. all of
#   the prerequisites must exist).
#
# - @ at the start of a command to disable echoing

# Debug build
build/icod: $(DEBUG_OBJS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(DFLAGS) $^ -o $@ $(LFLAGS)

build/debug/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@mkdir -p build/debug
	$(CC) $(CFLAGS) $(DFLAGS) -c -o $@ $<

# Release build
build/ico: $(RELEASE_OBJS)
	@mkdir -p build
	$(CC) $(CFLAGS) ${RFLAGS} $^ -o $@ $(LFLAGS)

build/release/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@mkdir -p build/release
	$(CC) $(CFLAGS) ${RFLAGS} -c -o $@ $<

# - at the start of a line to ignore error
clean: clean_debug clean_release

clean_debug:
	-rm build/debug/*.o
	-rm build/icod

clean_release:
	-rm build/release/*.o
	-rm build/ico