#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "clox_common.h"
#include "clox_value.h"
#include "clox_chunk.h"
#include "clox_table.h"

// Types of heap-allocated value
typedef enum {
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
} ObjType;

// The "struct Obj" is declared in clox_value.h
// and defined here.
struct Obj {
    ObjType type;       // Type tag for the obj
    bool is_marked;     // For GC marking phase
    struct Obj* next;   // For GC sweeping phase
};

// "length" is to know the string length without
// walking the string. "chars" will have a null
// terminator so that C library can work with it.
struct ObjString {
    Obj obj;        // Common obj tag
    int length;     // Length of the string
    char* chars;    // Content of the string
    uint32_t hash;  // Hash code of the string
};

// Runtime representation for upvalues
typedef struct ObjUpValue {
    Obj obj;
    Value* location;
    Value closed;               // To hold the value when closed
    struct ObjUpValue* next;    // For intrusive linked list of open upvalues
} ObjUpValue;

// Compile-time representation of a function
typedef struct {
    Obj obj;
    int arity;
    CodeChunk chunk;
    ObjString* name;
    int upvalue_count;
} ObjFunction;

// To represent the closure at runtime of a function,
// which is created when the function declaration is executed.
typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpValue** upvalues;  // The array of pointers to ObjUpvalue's
                            // (to share the ObjUpvalue's with other closures)
    int upvalue_count;
} ObjClosure;

// Function pointer type for Lox's native functions
typedef Value (*NativeFn)(int arg_count, Value* args);

// Obj subtype for Lox's native function
typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

// Obj subtype for Lox's classes
typedef struct {
    Obj obj;
    ObjString* name;
    Table methods;
} ObjClass;

// Obj subtype for Lox's instances/objects
typedef struct {
    Obj obj;
    ObjClass* class_;
    Table fields;
} ObjInstance;

// Obj subtype for bound method
typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

// Get the obj type tag from a Value
#define obj_type(val) (as_obj(val)->type)

// Macros for checking the type of an Obj
#define is_string(val)          is_obj_type(val, OBJ_STRING)
#define is_function(val)        is_obj_type(val, OBJ_FUNCTION)
#define is_closure(val)         is_obj_type(val, OBJ_CLOSURE)
#define is_native(val)          is_obj_type(val, OBJ_NATIVE)
#define is_class(val)           is_obj_type(val, OBJ_CLASS)
#define is_instance(val)        is_obj_type(val, OBJ_INSTANCE)
#define is_bound_method(val)    is_obj_type(val, OBJ_BOUND_METHOD)

// Inline function to check an Obj's type.
static inline bool is_obj_type(Value val, ObjType target_type) {
    // "val" is used twice, so an inline function is used instead
    // of a macro so that "val" is not executed/evaluated twice.
    return is_obj(val) && as_obj(val)->type == target_type;
}

// Macros to cast an obj Value into a specific Obj type
#define as_string(val)          ((ObjString*)as_obj(val))
#define as_c_string(val)        (((ObjString*)as_obj(val))->chars)
#define as_function(val)        ((ObjFunction*)as_obj(val))
#define as_closure(val)         ((ObjClosure*)as_obj(val))
#define as_native_c_func(val)   (((ObjNative*)as_obj(val))->function)
#define as_class(val)           ((ObjClass*)as_obj(val))
#define as_instance(val)        ((ObjInstance*)as_obj(val))
#define as_bound_method(val)    ((ObjBoundMethod*)as_obj(val))

// Create a ObjString by copying the content of a C string
// into a newly allocated block
ObjString* copy_and_create_str_obj(const char* source_str, int length);

// Create a ObjString by taking ownership of the passed
// char* (which points to an already allocated block)
ObjString* take_own_and_create_str_obj(char* chars, int length);

// Create a new ObjUpvalue that points to the passed Value slot.
ObjUpValue* new_upvalue_obj(Value* slot);

// Create a new ObjFunction and return its address
ObjFunction* new_function_obj();

// Create a new ObjClosure that wraps the given function.
ObjClosure* new_closure_obj(ObjFunction* function);

// Create a new ObjNative from a C function of type NativeFn
ObjNative* new_native_func_obj(NativeFn c_func);

// Create a new ObjClass with the given name
ObjClass* new_class_obj(ObjString* class_name);

// Create a new ObjInstance with the given class
ObjInstance* new_instance_obj(ObjClass* class_);

// Create a bound method that binds a method with a receiver
ObjBoundMethod* new_bound_method(Value receiver, ObjClosure* method);

// Print an Obj (which is a Value)
void print_object(Value val);

#endif