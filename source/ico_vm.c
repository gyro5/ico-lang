#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "ico_common.h"
#include "ico_vm.h"
#include "ico_debug.h"
#include "ico_compiler.h"
#include "ico_value.h"
#include "ico_memory.h"
#include "ico_object.h"

//------------------------------
//      STATIC FUNCTIONS
//------------------------------

// Reset the VM's value stack
static void reset_stack() {
    // Reset the stack pointer back to index 0 of the stack
    vm.stack_top = vm.stack;
    // vm.frame_count = 0;

    // Reset the list of open upvalues each time a top-level
    // "function" is executed (ex. each REPL line)
    // vm.open_upvalues = NULL;
}

// Peek the Value at distance away from the stack top
static IcoValue peek(int distance) {
    // stack_top points to the next slot to be used,
    // so -1 is the Value at the top of the stack.
    //
    // This means distance=0 is the stack top,
    // distance=1 is the next item, and so on.
    return *(vm.stack_top - 1 - distance);
}

// Return the falsiness of a clox Value
// (False only when nil or boolean false)
static bool is_falsey(IcoValue val) {
    return IS_NULL(val) || (IS_BOOL(val) && !AS_BOOL(val));
}


// Perform concatenation on the 2 strings on the stack top,
// assuming they are already checked to be strings
static void concat_strings() {
    // s2 before s1 due to stack LIFO. Peek instead of pop to prevent the strings
    // from being GC-ed so that they are available when we do memcpy().
    ObjString* s2 = AS_STRING(peek(0));
    ObjString* s1 = AS_STRING(peek(1));

    // Populate the new ObjString
    int concat_length = s1->length + s2->length;
    char* concat_chars = ALLOCATE(char, concat_length + 1);
    memcpy(concat_chars, s1->chars, s1->length);
    memcpy(concat_chars + s1->length, s2->chars, s2->length);
    concat_chars[concat_length] = '\0';

    ObjString* result_obj = take_own_and_create_str_obj(concat_chars, concat_length);
    pop();
    pop();
    push(OBJ_VAL(result_obj));
}

// Report a runtime error with a format string as the error message
static void runtime_error(const char* format_str, ...) {
    // Boilerplate to use "variadic function" (aka take varying number of arguments).
    // vfprintf() is like printf() but takes an explicit variadic list (va_list) instead.
    va_list args;
    va_start(args, format_str);
    vfprintf(stderr, format_str, args); // Print the error message
    va_end(args);
    fputs("\n", stderr);

    // Print the stack trace.
    // for (int i = vm.frame_count - 1; i >= 0; i--) {
    //     CallFrame* frame = &vm.frames[i];
    //     ObjFunction* func = frame->closure->function;

    //     // Calculate the index of the current instruction from
    //     // the instruction pointer and the start of the code chunk.
    //     // "- 1" is because ip points to the next instruction.
    //     size_t bytecode_idx = frame->ip - func->chunk.code_chunk - 1;
    //     fprintf(stderr, "[line %d] in ", func->chunk.line_nums[bytecode_idx]);

    //     // Print the function name
    //     if (func->name != NULL) {
    //         fprintf(stderr, "%s()\n", func->name->chars);
    //     }
    //     else {
    //         fprintf(stderr, "script\n");
    //     }
    // }

    reset_stack(); // For the next REPL run
}
/*
// Define a new native function and add it and its name
// as value and key to the hash table of global variables.
static void define_native_func(const char* name, NativeFn func) {
    // Need to push then pop immediately due to garbage collection
    push(obj_val(copy_and_create_str_obj(name, (int)strlen(name))));
    push(obj_val(new_native_func_obj(func)));
    table_set(&vm.globals, as_string(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

// Helper function to actually set up a function call.
static bool call_helper(ObjClosure* closure, int arg_count) {
    // Check arity
    if (arg_count != closure->function->arity) {
        runtime_error("Expect %d arguments but got %d.",
            closure->function->arity, arg_count);
        return false;
    }

    // Check for call stack overflow
    if (vm.frame_count == FRAMES_MAX) {
        runtime_error("Call stack overflow.");
        return false;
    }

    // Set up a new call frame
    CallFrame* new_frame = &vm.frames[vm.frame_count++];
    new_frame->closure = closure;
    new_frame->ip = closure->function->chunk.code_chunk;
    new_frame->base_ptr = vm.stack_top - arg_count - 1; // starts at the callee obj on the stack

    return true;
}

// Start a call on a Value by setting up a new CallFrame.
// Return false if the value is not callable.
static bool call_value(Value callee, int arg_count) {
    if (is_obj(callee)) {
        switch (obj_type(callee)) {
            case OBJ_CLOSURE:
                return call_helper(as_closure(callee), arg_count);

            case OBJ_NATIVE: {
                NativeFn c_func = as_native_c_func(callee);

                // Call the C function
                Value ret_val = c_func(arg_count, vm.stack_top - arg_count);

                // Discard the "call frame" (which only has arguments)
                // and push the return value back
                vm.stack_top -= arg_count + 1; // "+1" for the ObjNative
                push(ret_val);

                return true;
            }

            default: // Non-callable Obj types
                break;
        }
    }

    // Not even an Obj
    runtime_error("Can only call functions and classes.");
    return false;
}

// Capture a local variable (of the enclosing function) into an
// ObjUpvalue. Will reuse the ObjUpvalue if this local var has
// been captured before.
static ObjUpValue* capture_upvalue(Value* upper_local) {
    // Try to find an existing upvalue that capture this local var
    ObjUpValue* prev = NULL;
    ObjUpValue* curr = vm.open_upvalues;
    while (curr != NULL && curr->location > upper_local) {
        prev = curr;
        curr = curr->next;
    }

    // Found an existing upvalue
    if (curr != NULL && curr->location == upper_local) {
        return curr;
    }

    // Otherwise, create a new upvalue and insert it to
    // the linked list of open upvalues at the right location
    ObjUpValue* new_upvalue = new_upvalue_obj(upper_local);
    new_upvalue->next = curr; // curr will be the upvalue right after the new one
    if (prev == NULL) {
        // Insert to head of the linked list
        vm.open_upvalues = new_upvalue;
    }
    else {
        // Insert right after prev (prev->new->curr)
        prev->next = new_upvalue;
    }
    return new_upvalue;
}

// Close (Hoist to heap) all upvalues in the list of open upvalues
// from the stack top to the passed stack slot "last".
static void close_all_upvalues_from(Value* last) {
    while (vm.open_upvalues != NULL && vm.open_upvalues->location >= last) {
        ObjUpValue* curr = vm.open_upvalues;

        curr->closed = *curr->location; // Copy the local var to the heap
        curr->location = &curr->closed; // Redirect to the hoisted value

        vm.open_upvalues = curr->next;
    }
}

*/


/*************************************
    THE MAIN VM EXECUTION FUNCTION
**************************************/

// Run the current bytecode chunk in the VM
static InterpretResult vm_run() {
    // CallFrame* curr_frame = &vm.frames[vm.frame_count - 1];
    VM* curr_frame = &vm;

#ifdef  DEBUG_TRACE_EXECUTION
    printf("\n============ Execution Trace =============\n");
#endif

// Return the next byte in the code chunk, then increment ip
#define READ_NEXT_BYTE() (*curr_frame->ip++)

// Get the constant indexed by the next byte
// #define READ_CONSTANT() "\"
//     (curr_frame->closure->function->chunk.const_pool.values[read_next_byte()])
#define READ_CONSTANT() \
    (curr_frame->chunk->const_pool.values[READ_NEXT_BYTE()])

// Get the next 2 bytes as an unsigned short
#define READ_SHORT() \
    (curr_frame->ip += 2, \
    (uint16_t)((curr_frame->ip[-2] << 8) | curr_frame->ip[-1]))

// Same as read_constant() but also convert to ObjString*
#define READ_STRING() AS_STRING(READ_CONSTANT())

// Helper for the below macro. Also used for OP_ADD and OP_DIVIDE.
#define BINARY_OP_RESULT(va, vb, floatCaseVal, intCaseVal, op) \
    IS_FLOAT(va) ? /*va is float*/ \
        (IS_FLOAT(vb) ? \
            floatCaseVal(AS_FLOAT(va) op AS_FLOAT(vb)) : \
            floatCaseVal(AS_FLOAT(va) op AS_INT(vb))) \
        : /*va is int*/ \
        (IS_FLOAT(vb) ? \
            floatCaseVal(AS_INT(va) op AS_FLOAT(vb)) : \
            intCaseVal(AS_INT(va) op AS_INT(vb)))

// Common macro for the binary number operation:
// - op: the binary operation
// - floatCaseVal: result type when either value is float
// - intCaseVal: result type when both values are int
//
// b is popped first because of LIFO.
// This macro is inspired by Lua.
#define BINARY_OP(floatCaseVal, intCaseVal, op) \
    { \
    IcoValue vb = peek(0); \
    IcoValue va = peek(1); \
    if (!IS_NUMBER(vb) || !IS_NUMBER(va)) { \
        runtime_error("Operands must be 2 numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
    } \
    vm.stack_top[-2] = BINARY_OP_RESULT(va, vb, floatCaseVal, intCaseVal, op); \
    pop(); \
    }

    // Read and execute the bytecode byte-by-byte
    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        // Print the content of the VM's stack
        printf("          stack: ");
        for (IcoValue* slot = vm.stack; slot < vm.stack_top; slot++) {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");

        // If in debug mode, print the next instruction to be executed.
        // disass_instruction() needs an int offset, hence the pointer math
        // disass_instruction(&curr_frame->closure->function->chunk,
        //     (int)(curr_frame->ip - curr_frame->closure->function->chunk.code_chunk));
        disass_instruction(curr_frame->chunk,
            (int)(curr_frame->ip - curr_frame->chunk->chunk));

        printf("\n");
#endif

        /***********************
            OPCODE SWITCHING
        ************************/
        uint8_t instruction;
            // printf("%d", instruction);
        switch (instruction= READ_NEXT_BYTE()) {
            case OP_CONSTANT: {
                // Quick note: The {} is required because before C23,
                // declaring a variable right after a label (which is
                // "case OP_CONSTANT:" in this case) is not allowed.
                IcoValue constant = READ_CONSTANT();
                push(constant);
                break;
            }

            case OP_NULL: {
                push(NULL_VAL);
                break;
            }

            case OP_TRUE: {
                push(BOOL_VAL(true));
                break;
            }

            case OP_FALSE: {
                push(BOOL_VAL(false));
                break;
            }

            case OP_NEGATE: {
                IcoValue v = peek(0);

                // Perform direct negation instead of pop then push
                if (IS_INT(v)) {
                    vm.stack_top[-1].as.num_int = -AS_INT(v);
                }
                else if (IS_FLOAT(v)) {
                    vm.stack_top[-1].as.num_float = -AS_FLOAT(v);
                }
                else {
                    runtime_error("Operand must be an int or a float.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_ADD: { // OP_ADD also handles string concatenation
                IcoValue vb = peek(0);
                IcoValue va = peek(1);
                if (IS_STRING(va) && IS_STRING(vb)) {
                    concat_strings();
                }
                else if (IS_NUMBER(va) && IS_NUMBER(vb)) {
                    vm.stack_top[-2] = BINARY_OP_RESULT(va, vb, FLOAT_VAL, INT_VAL, +);
                    pop();
                }
                else {
                    runtime_error("Operands must be 2 numbers or 2 strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_SUBTRACT: BINARY_OP(FLOAT_VAL, INT_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(FLOAT_VAL, INT_VAL, *); break;

            case OP_DIVIDE: {
                IcoValue vb = peek(0);
                IcoValue va = peek(1);
                if (!IS_NUMBER(vb) || !IS_NUMBER(va)) {
                    runtime_error("Operands must be 2 numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (IS_INT(va) && IS_INT(vb) && AS_INT(vb) == 0) {
                    runtime_error("Can't do integer division by 0.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm.stack_top[-2] = BINARY_OP_RESULT(va, vb, FLOAT_VAL, INT_VAL, /);
                pop();
                break;
            }

            case OP_MODULO: {
                IcoValue vb = peek(0);
                IcoValue va = peek(1);
                if (!IS_INT(vb) || !IS_INT(va)) {
                    runtime_error("Operands for modulo must be 2 integers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                long b = AS_INT(vb);
                if (b == 0) {
                    runtime_error("Can't do integer modulo by 0.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm.stack_top[-2] = INT_VAL(AS_INT(va) % b);
                pop();
                break;
            }

            case OP_NOT:
                // Direct assignment instead of pop then push
                vm.stack_top[-1] = BOOL_VAL(is_falsey(vm.stack_top[-1]));
                break;

            case OP_EQUAL:
                // Stack LIFO -----------> b      a
                push(BOOL_VAL(values_equal(pop(), pop())));
                break;

            case OP_GREATER: BINARY_OP(BOOL_VAL, BOOL_VAL, >); break;
            case OP_LESS:    BINARY_OP(BOOL_VAL, BOOL_VAL, <); break;

            case OP_RETURN: {
                // Value ret_val = pop();

                // // Pop the call frame from the call stack
                // vm.frame_count--;

                // // Close all open upvalues of the current function
                // close_all_upvalues_from(curr_frame->base_ptr);

                // // No more call frame --> Done!
                // if (vm.frame_count == 0) {
                //     pop(); // Pop the top-level ObjFunction
                //     return INTERPRET_OK;
                // }

                // // Otherwise, return to the caller
                // vm.stack_top = curr_frame->base_ptr;
                // push(ret_val);
                // curr_frame = &vm.frames[vm.frame_count - 1];
                return INTERPRET_OK;
                break;
            }

            case OP_PRINT:
                // The expression has been evaluated by the preceeding
                // bytecodes and pushed on the VM's stack.
                print_value(pop());
                break;

            case OP_PRINTLN:
                print_value(pop());
                printf("\n");
                break;

            case OP_POP: pop(); break;

            case OP_DEFINE_GLOBAL: {
                // Insert the global variable and its initialized value
                // into the globals hash table.
                IcoValue var_name = READ_CONSTANT();
                table_set(&vm.globals, var_name, peek(0));

                // Need to pop AFTER the value is added to the globals table
                // so that GC doesn't collect it.
                pop();
                break;
            }

            case OP_GET_GLOBAL: {
                IcoValue var_name = READ_CONSTANT();
                IcoValue value;

                // Try to access the variable with this name
                if (!table_get(&vm.globals, var_name, &value)) {
                    runtime_error("Undefined variable '%s'.", AS_STRING(var_name)->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }

                // Found the variable -> Push the value to the VM stack
                push(value);
                break;
            }

            case OP_SET_GLOBAL: {
                IcoValue var_name = READ_CONSTANT();

                // Try to set the variable value
                if (table_set(&vm.globals, var_name, peek((0)))) {
                    // If variable not already declared
                    table_delete(&vm.globals, var_name);
                    runtime_error("Undefined variable '%s'.", AS_STRING(var_name)->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
/*
            case OP_GET_LOCAL: {
                // Get the index of the local variable on the VM stack
                uint8_t stack_index = READ_NEXT_BYTE();
                push(curr_frame->base_ptr[stack_index]);
                break;
            }

            case OP_SET_LOCAL: {
                uint8_t stack_index = READ_NEXT_BYTE();
                curr_frame->base_ptr[stack_index] = peek(0);
                // Don't pop, only peek because assignment is an expr
                break;
            }
/*
            case OP_JUMP_IF_FALSE: {
                uint16_t jump_dist = read_short();
                if (is_falsey(peek(0))) {
                    curr_frame->ip += jump_dist;
                }
                break;
            }

            case OP_JUMP: {
                uint16_t jump_dist = read_short();
                curr_frame->ip += jump_dist;
                break;
            }

            case OP_LOOP: {
                uint16_t jump_dist = read_short();
                curr_frame->ip -= jump_dist; // jump back
                break;
            }

            case OP_CALL: {
                int arg_count = read_next_byte();

                // stack_peek(arg_count) will be the callee value
                if (!call_value(peek(arg_count), arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                // Successful call will add a new frame to the call stack
                // so we need to update the current frame to it.
                curr_frame = &vm.frames[vm.frame_count - 1];
                // After this, the VM will use the latest call frame
                // for the code chunk and the ip to be executed next.

                break;
            }

            case OP_CLOSURE: {
                // Create the closure object
                ObjFunction* function = as_function(read_constant());
                ObjClosure* closure = new_closure_obj(function);
                push(obj_val(closure));

                // Populate the array of upvalue pointers of the closure
                for (int i = 0; i < closure->upvalue_count; i++) {
                    uint8_t is_local = read_next_byte();
                    uint8_t idx = read_next_byte();

                    if (is_local) { // Local var of the enclosing function
                        closure->upvalues[i] = capture_upvalue(curr_frame->base_ptr + idx);
                    }
                    else { // Upvalue of the enclosing function
                        closure->upvalues[i] = curr_frame->closure->upvalues[idx];
                        // Copy the pointer to the actual ObjUpvalue.
                        //
                        // We use the current call frame because it is for the current
                        // function being executed, which is where the inner function
                        // declaration happens (aka. where OP_CLOSURE is executed).
                    }
                }

                break;
            }

            case OP_GET_UPVALUE: {
                uint8_t upvalue_idx = read_next_byte();
                push(*curr_frame->closure->upvalues[upvalue_idx]->location);
                break;
            }

            case OP_SET_UPVALUE: {
                uint8_t upvalue_idx = read_next_byte();
                *curr_frame->closure->upvalues[upvalue_idx]->location = peek(0);
                break;
            }

            case OP_CLOSE_UPVALUE: {
                close_all_upvalues_from(vm.stack_top - 1); // Only 1 value at stack top
                pop();
                break;
            }
                */

            default: { // For dev only, will be unreachable
                printf("Unsupported opcode!");
                break;
            }
        }
    }

// Because these macros are only used in this function. TODO
#undef READ_NEXT_BYTE
#undef READ_CONSTANT
#undef READ_STRING
#undef READ_SHORT
#undef BINARY_OP
#undef BINARY_OP_RESULT
}

//------------------------------
//      NATIVE FUNCTIONS
//------------------------------

static IcoValue clock_native(int arg_count, IcoValue* args) {
    return INT_VAL(clock() / CLOCKS_PER_SEC);
}

//------------------------------
//      HEADER FUNCTIONS
//------------------------------

// The global VM variable/object
VM vm;

void init_vm() {
    reset_stack();

    vm.ip = NULL;
    vm.chunk = NULL;

    // // No allocated Objs yet
    // vm.allocated_objs = NULL;

    // // Initialize the gray stack (for GC)
    // vm.gray_count = 0;
    // vm.gray_capacity = 0;
    // vm.gray_stack = NULL;

    // // Initialize the GC trigger
    // vm.bytes_allocated = 0;
    // vm.next_gc_run = 1024 * 1024; // Arbitrarily chosen -> See book/notebook

    // // Initialize the hash tables
    init_table(&vm.globals); // table of global variables
    init_table(&vm.strings); // table for string interning

    // // Add native functions
    // define_native_func("clock", clock_native);
}

void free_vm() {
    free_table(&vm.globals);
    free_table(&vm.strings);
    // free_objects();
}

InterpretResult vm_interpret(const char *source_code) {
    // // Compile the source code and get the ObjFunction for top-level code
    // ObjFunction* top_level_func = compile(source_code);
    // if (top_level_func == NULL) return INTERPRET_COMPILE_ERROR;

    // // Set up the top-level "function" as the first call
    // push(obj_val(top_level_func));
    // ObjClosure* top_level_closure = new_closure_obj(top_level_func);
    // pop();
    // push(obj_val(top_level_closure));
    // call_helper(top_level_closure, 0);

    init_vm();

    vm.chunk = compile(source_code);
    vm.ip = vm.chunk->chunk;

    // Run and return the result
    return vm_run();
}

/*
The reason for not checking for empty stack or stack overflow
is performance. Pop and push is used a lot, and checking is expensive.
Instead, the compiler takes care to use precise numbers of
pops and pushes, allowing stack operation to be fast.
*/

void push(IcoValue val) {
    *vm.stack_top = val;
    vm.stack_top++;
}

IcoValue pop() {
    vm.stack_top--;
    return *vm.stack_top;
}