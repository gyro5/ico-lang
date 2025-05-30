# Compiler options
CC := gcc
# CFLAGS := -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -std=c23
CFLAGS := -Wall -Wextra -Wno-unused-parameter -Wno-unused-function

# Flags for debug build
DFLAGS := -g -Og -DDEBUG -std=gnu17

# Flags for release build that uses GCC's labels as values extension
# for computed gotos dispatching (similar to Lua's jump table).
# "-fno-gcse" is needed for GCC to not optimize away the gotos.
GNU_RFLAGS := -O3 -fno-gcse -std=gnu17

# Flags for release build that only uses ANSI C (ie. switch dispatch)
ANSI_RFLAGS := -O3 -flto -std=c17 -DSWITCH_DISPATCH

# Files
SRC_DIR := source
HEADERS := $(wildcard $(SRC_DIR)/ico_*.h)
SOURCES := $(wildcard $(SRC_DIR)/ico_*.c) main.c
DEBUG_OBJS := $(addprefix build/debug/, $(notdir $(SOURCES:.c=.o)))
RELEASE_OBJS := $(addprefix build/release/, $(notdir $(SOURCES:.c=.o)))
RELEASE_ANSI_OBJS := $(addprefix build/release_ansi/, $(notdir $(SOURCES:.c=.o)))

# Note:
# - $@ is target, $< is first prerequisite, $^ is all prerequisites.
#
# - For pattern rules (like "build/debug/%.o"), both the target and
#   the prerequisites must match for the rule to match (ie. all of
#   the prerequisites must exist).
#
# - @ at the start of a command to disable echoing

# Default: debug build
.PHONY: default debug release all

default: debug

all: debug release

debug: build/icod

release: build/ico

release_ansi: build/ico_ansi

# Debug build
build/icod: $(DEBUG_OBJS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(DFLAGS) $^ -o $@

build/debug/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@mkdir -p build/debug
	$(CC) $(CFLAGS) $(DFLAGS) -c -o $@ $<

# Release build
build/ico: $(RELEASE_OBJS)
	@mkdir -p build
	$(CC) $(CFLAGS) ${GNU_RFLAGS} $^ -o $@

build/release/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@mkdir -p build/release
	$(CC) $(CFLAGS) ${GNU_RFLAGS} -c -o $@ $<

# Release build ANSI
build/ico_ansi: $(RELEASE_ANSI_OBJS)
	@mkdir -p build
	$(CC) $(CFLAGS) ${ANSI_RFLAGS} $^ -o $@

build/release_ansi/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@mkdir -p build/release_ansi
	$(CC) $(CFLAGS) ${ANSI_RFLAGS} -c -o $@ $<