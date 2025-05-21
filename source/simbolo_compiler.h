#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "clox_vm.h"
#include "clox_object.h"

// Compile a string of Lox source code into an ObjFunction
// and return it. Return NULL if there are compile errors.
ObjFunction* compile(const char* source_code);

// GC function: Mark the objects created and used by the compiler,
// such as the ObjFunction's.
void mark_compiler_roots();

#endif