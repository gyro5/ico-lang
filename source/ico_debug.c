#include <stdio.h>

#include "ico_debug.h"
#include "ico_chunk.h"
#include "ico_value.h"
// #include "ico_object.h"

//------------------------------
//      STATIC FUNCTIONS
//------------------------------

// Print a simple instruction with only a single opcode.
static int simple_instruction(const char* ins_name, int offset) {
    printf("%s\n", ins_name);
    return offset + 1; // Because simple instruction only takes 1 byte
}

// Print a constant instruction. General format of these
// instructions: [opcode][const_idx]
static int constant_instruction(const char* ins_name, CodeChunk* chunk, int offset) {
    // The index of the constant in the pool is right after the opcode
    uint8_t constant_idx = chunk->chunk[offset + 1];

    // Print the opcode and the constant index.
    // "%-16s" means 16 spaces and left-aligned.
    printf("%-16s %4d '", ins_name, constant_idx);

    // Print the actual constant value
    print_value(chunk->const_pool.values[constant_idx]);

    printf("'\n");

    // Constant instruction is 2 bytes
    return offset + 2;
}

// Print a byte instruction. General format of these
// instructions: [opcode][byte]
static int byte_instruction(const char* name, CodeChunk* chunk, int offset) {
    uint8_t stack_index = chunk->chunk[offset + 1];
    printf("%-16s %4d\n", name, stack_index);
    return offset + 2;
}

// Print a jump instruction. General format:
// [jump_opcode][off][set]
static int jump_instruction(const char* name, int sign, CodeChunk* chunk, int offset) {
    // Calculate the jump distance
    uint16_t jump_dist = (uint16_t)(chunk->chunk[offset + 1] << 8);
    jump_dist |= chunk->chunk[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump_dist);
    return offset + 3;
}

// Print an invoke instruction
static int invoke_instruction(const char* name, CodeChunk* chunk, int offset) {
    uint8_t const_idx = chunk->chunk[offset + 1];
    uint8_t arg_count = chunk->chunk[offset + 2];
    printf("%-16s (%d args) %4d '", name, arg_count, const_idx);
    print_value(chunk->const_pool.values[const_idx]);
    printf("'\n");
    return offset + 3;
}

//------------------------------
//      HEADER FUNCTIONS
//------------------------------

void disass_chunk(CodeChunk* chunk, const char* chunk_name) {
    // Print chunk name
    printf("\n== %s ==\n", chunk_name);

    // Print header
    printf("Offs Line OpCode       ConstIdx ConstValue\n");
    printf("(jump offset)              From -> To     \n");
    printf("------------------------------------------\n");

    for (int offset = 0; offset < chunk->size; ) {
        // Disassemble instruction-by-instruction
        // Offset of next instruction is returned from the
        // disass_instruction() function because instructions
        // can have different sizes.
        offset = disass_instruction(chunk, offset);
    }
}

int disass_instruction(CodeChunk* chunk, int offset) {
    // Print the current offset
    printf("%04d ", offset);

    // Print the line numbers
    if (offset > 0 && chunk->line_nums[offset] == chunk->line_nums[offset - 1]) {
        // For instructions on the same line as the current one
        printf("   | ");
    }
    else {
        // First line (first byte), and any new lines in the Lox source code
        printf("%4d ", chunk->line_nums[offset]);
    }

    // Switch on the curent opcode to choose how to print the instruction
    uint8_t instruction = chunk->chunk[offset];
    switch (instruction) {
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);

        case OP_NEGATE:
            return simple_instruction("OP_NEGATE", offset);

         case OP_ADD:
            return simple_instruction("OP_ADD", offset);

        case OP_SUBTRACT:
            return simple_instruction("OP_SUBTRACT", offset);

        case OP_MULTIPLY:
            return simple_instruction("OP_MULTIPLY", offset);

        case OP_DIVIDE:
            return simple_instruction("OP_DIVIDE", offset);

        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);

        case OP_NULL:
            return simple_instruction("OP_NULLL", offset);

        case OP_TRUE:
            return simple_instruction("OP_TRUE", offset);

        case OP_FALSE:
            return simple_instruction("OP_FALSE", offset);

        case OP_NOT:
            return simple_instruction("OP_NOT", offset);

        case OP_EQUAL:
            return simple_instruction("OP_EQUAL", offset);

        case OP_GREATER:
            return simple_instruction("OP_GREATER", offset);

        case OP_LESS:
            return simple_instruction("OP_LESS", offset);

        case OP_PRINT:
            return simple_instruction("OP_PRINT", offset);

        case OP_POP:
            return simple_instruction("OP_POP", offset);

        case OP_GET_GLOBAL:
            return constant_instruction("OP_GET_GLOBAL", chunk, offset);

        case OP_DEFINE_GLOBAL:
            return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);

        case OP_SET_GLOBAL:
            return constant_instruction("OP_SET_GLOBAL", chunk, offset);

        case OP_GET_LOCAL:
            return byte_instruction("OP_GET_LOCAL", chunk, offset);

        case OP_SET_LOCAL:
            return byte_instruction("OP_SET_LOCAL", chunk, offset);

        case OP_JUMP_IF_FALSE:
            return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);

        case OP_JUMP:
            return jump_instruction("OP_JUMP", 1, chunk, offset);

        case OP_LOOP:
            return jump_instruction("OP_LOOP", -1, chunk, offset);

        case OP_CALL:
            return byte_instruction("OP_CALL", chunk, offset);
/*
        case OP_CLOSURE: {
            offset++;

            // Print the opcode and the constant index of the wrapped ObjFunction
            uint8_t constant_idx = chunk->code_chunk[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", constant_idx);

            // Print the wrapped ObjFunction
            Value func_val = chunk->const_pool.values[constant_idx];
            print_value(func_val);
            printf("\n");

            // Print the upvalues
            ObjFunction* func = as_function(func_val);
            for (int i = 0; i < func->upvalue_count; i++) {
                int is_local = chunk->code_chunk[offset++];
                int idx = chunk->code_chunk[offset++];
                printf("%04d      |                     %s %d\n",
                    offset - 2, is_local ? "local" : "upvalue", idx);
            }

            return offset;
        }

        case OP_GET_UPVALUE:
            return byte_instruction("OP_GET_UPVALUE", chunk, offset);

        case OP_SET_UPVALUE:
            return byte_instruction("OP_SET_UPVALUE", chunk, offset);

        case OP_CLOSE_UPVALUE:
            return simple_instruction("OP_CLOSE_UPVALUE", offset);

        case OP_CLASS:
            return constant_instruction("OP_CLASS", chunk, offset);

        case OP_GET_PROPERTY:
            return constant_instruction("OP_GET_PROPERTY", chunk, offset);

        case OP_SET_PROPERTY:
            return constant_instruction("OP_SET_PROPERTY", chunk, offset);

        case OP_METHOD:
            return constant_instruction("OP_METHOD", chunk, offset);

        case OP_INVOKE:
            return invoke_instruction("OP_INVOKE", chunk, offset);

        case OP_INHERIT:
            return simple_instruction("OP_INHERIT", offset);

        case OP_GET_SUPER:
            return constant_instruction("OP_GET_SUPER", chunk, offset);

        case OP_SUPER_INVOKE:
            return invoke_instruction("OP_SUPER_INVOKE", chunk, offset);
*/
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}