# Compiler options
CC := gcc
CFLAGS := -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -std=c99

# Files
SRC_DIR := source
HEADERS := $(wildcard $(SRC_DIR)/*.h)
SOURCES := $(wildcard $(SRC_DIR)/*.c)
DEBUG_OBJS := $(addprefix build/debug/, $(notdir $(SOURCES:.c=.o)))
RELEASE_OBJS := $(addprefix build/release/, $(notdir $(SOURCES:.c=.o)))

# Note: 
# - $@ is target, $< is first prerequisite, $^ is all prerequisites.
#
# - For pattern rules (like "build/debug/%.o"), both the target and
#   the prerequisites must match for the rule to match (ie. all of
#   the prerequisites must exist).

# Default: debug build
.PHONY: default debug release all

default: debug

all: debug release

debug: build/clox

release: build/clox_release

build/clox: $(DEBUG_OBJS)
	mkdir -p build
	$(CC) $(CFLAGS) -O0 -DDEBUG -g $^ -o $@

build/debug/%.o: $(SRC_DIR)/%.c $(HEADERS)
	mkdir -p build/debug
	$(CC) $(CFLAGS) -O0 -DDEBUG -g -c -o $@ $<

# Release build
build/clox_release: $(RELEASE_OBJS)
	mkdir -p build
	$(CC) $(CFLAGS) -O3 -flto $^ -o $@

build/release/%.o: $(SRC_DIR)/%.c $(HEADERS)
	mkdir -p build/release
	$(CC) $(CFLAGS) -O3 -flto -c -o $@ $<
