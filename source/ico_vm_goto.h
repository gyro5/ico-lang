// LSP will always complain on this file, as it is supposed
// to be included inside a function (specifically vm_run()).

#undef VM_DISPATCH
#undef VM_CASE
#undef VM_BREAK

// Computed goto dispatch:
// - VM_DISPATCH goes to the first instruction
// - VM_CASE uses macro concatenation with "##" to create labels
// - VM_BREAK goes to the next instruction
#define VM_DISPATCH(ins)    goto *label_table[ins];
#define VM_CASE(opcode)     L_##opcode:
#define VM_BREAK            VM_DISPATCH(READ_NEXT_BYTE())

// Using C99 designator index syntax and GNU C label as value for computed gotos.
static const void* const label_table[] = {
    [OP_RETURN] = &&L_OP_RETURN,
    [OP_CONSTANT] = &&L_OP_CONSTANT,
    [OP_NULL] = &&L_OP_NULL,
    [OP_TRUE] = &&L_OP_TRUE,
    [OP_FALSE] = &&L_OP_FALSE,
    [OP_NEGATE] = &&L_OP_NEGATE,
    [OP_ADD] = &&L_OP_ADD,
    [OP_SUBTRACT] = &&L_OP_SUBTRACT,
    [OP_MULTIPLY] = &&L_OP_MULTIPLY,
    [OP_DIVIDE] = &&L_OP_DIVIDE,
    [OP_MODULO] = &&L_OP_MODULO,
    [OP_POWER] = &&L_OP_POWER,
    [OP_NOT] = &&L_OP_NOT,
    [OP_EQUAL] = &&L_OP_EQUAL,
    [OP_GREATER] = &&L_OP_GREATER,
    [OP_LESS] = &&L_OP_LESS,
    [OP_PRINT] = &&L_OP_PRINT,
    [OP_PRINTLN] = &&L_OP_PRINTLN,
    [OP_POP] = &&L_OP_POP,
    [OP_DEFINE_GLOBAL] = &&L_OP_DEFINE_GLOBAL,
    [OP_GET_GLOBAL] = &&L_OP_GET_GLOBAL,
    [OP_SET_GLOBAL] = &&L_OP_SET_GLOBAL,
    [OP_GET_LOCAL] = &&L_OP_GET_LOCAL,
    [OP_SET_LOCAL] = &&L_OP_SET_LOCAL,
    [OP_JUMP_IF_FALSE] = &&L_OP_JUMP_IF_FALSE,
    [OP_JUMP] = &&L_OP_JUMP,
    [OP_LOOP] = &&L_OP_LOOP,
    [OP_CALL] = &&L_OP_CALL,
    [OP_CLOSURE] = &&L_OP_CLOSURE,
    [OP_GET_UPVALUE] = &&L_OP_GET_UPVALUE,
    [OP_SET_UPVALUE] = &&L_OP_SET_UPVALUE,
    [OP_CLOSE_UPVALUE] = &&L_OP_CLOSE_UPVALUE,
    [OP_STORE_VAL] = &&L_OP_STORE_VAL,
    [OP_READ] = &&L_OP_READ,
    [OP_CREATE_LIST] = &&L_OP_CREATE_LIST,
    [OP_GET_ELEMENT] = &&L_OP_GET_ELEMENT,
    [OP_SET_ELEMENT] = &&L_OP_SET_ELEMENT,
    [OP_GET_RANGE] = &&L_OP_GET_RANGE,
};