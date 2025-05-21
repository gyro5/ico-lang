#include <stdlib.h>

#include "clox_chunk.h"
#include "clox_memory.h"
#include "clox_vm.h"

void init_chunk(CodeChunk *chunk) {
    chunk->size = 0;
    chunk->capacity = 0;
    chunk->code_chunk = NULL;
    chunk->line_nums = NULL;
    init_value_array(&chunk->const_pool);
}

void append_chunk(CodeChunk* chunk, uint8_t byte, int line_num) {
    // Check the capacity of the current backing array
    if (chunk->capacity < chunk->size + 1) {
        int old_cap = chunk->capacity;
        chunk->capacity = grow_capacity(old_cap);
        chunk->code_chunk = grow_array(uint8_t, chunk->code_chunk, old_cap, chunk->capacity);
        chunk->line_nums = grow_array(int, chunk->line_nums, old_cap, chunk->capacity);
    }

    // Add the new byte to the chunk
    chunk->code_chunk[chunk->size] = byte;
    chunk->line_nums[chunk->size] = line_num;
    chunk->size++;
}

void free_chunk(CodeChunk* chunk) {
    // Free the bytecode array
    free_array(uint8_t, chunk->code_chunk, chunk->capacity);

    // Free the line numbers array
    free_array(int, chunk->line_nums, chunk->capacity);

    // Also free the constant pool
    free_value_array(&chunk->const_pool);
    
    // Use init_chunk to reset the fields
    init_chunk(chunk);
}

int add_constant(CodeChunk* chunk, Value val) {
    push(val); // To avoid Obj value being sweeped by the GC
    append_value_array(&chunk->const_pool, val);
    pop();
    return chunk->const_pool.size - 1;
}
