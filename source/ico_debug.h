#ifndef ICO_DEBUG_H
#define ICO_DEBUG_H

#include "ico_chunk.h"

// Print the human-readable version of the bytecode chunk
void disass_chunk(CodeChunk* chunk, const char* chunk_name);

// Print the bytecode and return the offset of the next instruction in a chunk
int disass_instruction(CodeChunk* chunk, int offset);

#endif // !ICO_DEBUG_H
