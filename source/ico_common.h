// Common header file for clox

#ifndef ICO_COMMON_H
#define ICO_COMMON_H

#include <stdbool.h> // To use C99 "bool" type
#include <stddef.h>  // For "size_t" and "NULL"
#include <stdint.h>  // For sized ints

// #define DEBUG_PRINT_BYTECODE
// #define DEBUG_TRACE_EXECUTION
// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC

// Uncomment this to disable NaN Boxing
#define NAN_BOXING
// #define NAN_BOXING_IEEE_754

#define UINT8_COUNT (UINT8_MAX + 1)

#endif // !ICO_COMMON_H