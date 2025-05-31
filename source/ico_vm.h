#ifndef ICO_VM_H
#define ICO_VM_H

#include "ico_chunk.h"
#include "ico_value.h"
#include "ico_table.h"
#include "ico_object.h"

#define FRAMES_MAX 64 // Maximum call depth
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    IcoValue* base_ptr;
} CallFrame;

// This struct represents the state of an Ico VM
typedef struct {
    CallFrame frames[FRAMES_MAX];       // The VM's call stack (aka the VM's stack)
    int frame_count;                    // The current frame count (aka top of call stack)
    IcoValue stack[STACK_MAX];          // The value stack
    IcoValue* stack_top;                // The value stack pointer (to next slot to-be-used)
    Obj* allocated_objs;                // Linked list of allocated Obj for memory management
    Table strings;                      // For string interning
    Table globals;                      // To store global variables
    ObjUpValue* open_upvalues;          // The list of open upvalues
    Obj** gray_stack;                   // GC: stack of gray objects
    int gray_count;                     // GC: number of gray objects
    int gray_capacity;                  // GC: capacity of the gray stack
    size_t bytes_allocated;             // GC: Number of bytes allocated
    size_t next_gc_run;                 // GC: Threshold for next GC run
} VM;

// Enum for interpreter result or REPL state.
typedef enum {
    INTERPRET_IDLE,
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

// Declare "extern" to let any file that imports this header
// file to be able to use the global VM.
extern VM vm;

// Initialize a VM
void init_vm();

// Interpret a string of Ico source code
InterpretResult vm_interpret(const char* source_code);

// Tear down a VM
void free_vm();

// Push a Value on the VM's stack
void push(IcoValue val);

// Pop the Value at the top of the VM's stack
IcoValue pop();

#endif // !ICO_VM_H
