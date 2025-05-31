#ifndef ICO_COMMON_H
#define ICO_COMMON_H

#include <stdbool.h> // To use C99 "bool" type
#include <stddef.h>  // For "size_t" and "NULL"
#include <stdint.h>  // For sized ints

// ANSI colors
#define COLOR_RED     "\e[31m"
#define COLOR_GREEN   "\e[32m"
#define COLOR_BLUE    "\e[34m"
#define COLOR_BOLD    "\e[1m"
#define COLOR_RESET   "\e[0m"

#define UINT8_COUNT (UINT8_MAX + 1)

// GCC still supports this even when "-std=c23" is not set
#define C23_ENUM_FIXED_TYPE

// Switch dispatch is enabled by default when compiling with
// "-DSWITCH_DISPATCH" but it can be enabled manually here.
// #ifndef SWITCH_DISPATCH
// #define SWITCH_DISPATCH
// #endif

#ifdef DEBUG // Will be enabled at the compile command
#define DEBUG_PRINT_BYTECODE
#define DEBUG_TRACE_EXECUTION
#define DEBUG_STRESS_GC
#define DEBUG_LOG_GC
#define DEBUG_PRINT_TOKEN
#endif

#endif // !ICO_COMMON_H