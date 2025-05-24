#ifndef ICO_COMMON_H
#define ICO_COMMON_H

#include <stdbool.h> // To use C99 "bool" type
#include <stddef.h>  // For "size_t" and "NULL"
#include <stdint.h>  // For sized ints

#define C23_ENUM_FIXED_TYPE

// ANSI colors
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_RESET   "\x1b[0m"
#define COLOR_BOLD    "\x1b[1m"

#define DEBUG_PRINT_BYTECODE
#define DEBUG_TRACE_EXECUTION
// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC
#define DEBUG_PRINT_TOKEN

#define UINT8_COUNT (UINT8_MAX + 1)

#endif // !ICO_COMMON_H