#ifndef CLOX_DEBUG_H
#define CLOX_DEBUG_H

#include "clox_chunk.h"

// Print the human-readable version of the bytecode chunk
void disass_chunk(CodeChunk* chunk, const char* chunk_name);

// Print the bytecode and return the offset of the next instruction in a chunk
int disass_instruction(CodeChunk* chunk, int offset);

#endif // !CLOX_DEBUG_H
