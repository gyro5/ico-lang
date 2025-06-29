#ifndef ICO_CHUNK_H
#define ICO_CHUNK_H

#include "ico_common.h"
#include "ico_value.h"

// Enum for types of opcode
typedef enum {
    OP_RETURN,      // [op_ret]: Return

    // Constants instructions
    OP_CONSTANT,    // [opcode][const_idx]: Push a constant on the VM stack
    OP_NULL,        // [constant null]: Push null on the VM stack
    OP_TRUE,        // [constant true]: Push true on the VM stack
    OP_FALSE,       // [constant false]: Push false on the VM stack

    // Arithmetic instructions (operands are from the VM stack)
    OP_NEGATE,      // [simple negate]
    OP_ADD,         // [arithmetic +]
    OP_SUBTRACT,    // [arithmetic -]
    OP_MULTIPLY,    // [arithmetic *]
    OP_DIVIDE,      // [arithmetic /]
    OP_MODULO,      // [integer %]
    OP_POWER,       // [arithmetic ^]

    // Logical and comparison instructions
    OP_NOT,         // [logical not]
    OP_EQUAL,       // [comparison =]
    OP_GREATER,     // [comparison >]
    OP_LESS,        // [comparison <]

    OP_PRINT,       // [print]: Pop the VM stack and print the value
    OP_PRINTLN,     // [println]
    OP_POP,         // [pop]: Pop the VM stack

    // For global and local variables
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,

    // Jump instructions
    OP_JUMP_IF_FALSE,   // [jump][off][set]: Conditional jump forward
    OP_JUMP,            // [jump][off][set]: Unconditional jump forward
    OP_LOOP,            // [jump][off][set]: Unconditional jump backward

    // Function-related instructions
    OP_CALL,            // [op_call][arg_count]: Function call
    OP_CLOSURE,         // [op_clos][obj_func_const_idx][is_local1][idx1][is_local2][idx2]...
                        // : Create a new ObjClosure with upvalues
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSE_UPVALUE,   // [op_close_upvalue]: Hoist the local var at stack top to the heap

    // Other instructions
    OP_STORE_VAL,       // [op_store_val]: store value in the VM struct (internal)
    OP_READ,            // [op_read]: Read (IO) instruction

    // Container and element access instructions
    OP_CREATE_LIST,     // [op_create_list][member_count]: Create an ObjList on the stack
    OP_GET_ELEMENT,     // [op_get_ele]: Access an element of a list, string, or table
    OP_SET_ELEMENT,     // [op_set_ele]: Set an element of a list or a table
    OP_GET_RANGE,       // [op_get_range]: Get a range of elements in a list or string
    OP_CREATE_TABLE,    // [op_create_table]: Create a new empty ObjTable
} OpCode;

typedef struct {
    int size;               // Number of elements
    int capacity;           // Actual capacity
    uint8_t* chunk;    // Array of bytecode
    int* line_nums;         // Array of line numbers corresponding to the bytecodes
    ValueArray const_pool;  // Array of constant values
} CodeChunk;

// Initialize a new CodeChunk.
void init_chunk(CodeChunk* chunk);

// Append a byte to the end of a chunk.
void append_chunk(CodeChunk* chunk, uint8_t byte, int line_num);

// Free the memory of a CodeChunk.
void free_chunk(CodeChunk* chunk);

// Add a constant to the constant pool of the chunk
// and return its index in the pool.
int add_constant(CodeChunk* chunk, IcoValue val);

#endif // !ICO_CHUNK_H
