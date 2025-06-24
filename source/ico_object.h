#ifndef ICO_OBJECT_H
#define ICO_OBJECT_H

#include "ico_common.h"
#include "ico_value.h"
#include "ico_chunk.h"
#include "ico_table.h"

// Types of heap-allocated value
#ifdef C23_ENUM_FIXED_TYPE
typedef enum : char {
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_LIST,
    OBJ_TABLE,
} ObjType;
#else
typedef enum {
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_LIST,
    OBJ_TABLE,
} ObjType;
#endif

// The "struct Obj" is declared in ico_value.h and defined here.
struct Obj {
    ObjType type;       // Type tag for the obj
    bool is_marked;     // For GC marking phase
    uint32_t hash;      // For hash table
    struct Obj* next;   // For GC sweeping phase
};

#ifdef C23_ENUM_FIXED_TYPE
// Static checks (aka at compile time) for the sizes of some types
_Static_assert( sizeof(ObjType) == sizeof(char),
    "C23 enum type is not supported, ObjType is not char. Please disable this flag.");
_Static_assert( sizeof(Obj) == 16,
    "C23 enum type is not supported, Obj is not 16 bytes. Please disable this flag.");
#endif

// length is to know the string length without walking the string.
// chars will have a null terminator so that C library can work with it.
struct ObjString {
    Obj obj;        // Common obj tag
    int length;     // Length of the string
    char* chars;    // Content of the string
};

// Runtime representation for upvalues
typedef struct ObjUpValue {
    Obj obj;
    IcoValue* location;
    IcoValue closed;          // To hold the value when closed
    struct ObjUpValue* next;  // For intrusive linked list of open upvalues
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
    ObjUpValue** upvalues;  // The array of pointers to ObjUpvalues
    int upvalue_count;
} ObjClosure;

// Function pointer type for native functions
typedef IcoValue (*NativeFn)(int arg_count, IcoValue* args);

// Obj subtype for native function
typedef struct {
    Obj obj;
    NativeFn function;
    int arity;
    ObjString* name;
} ObjNative;

// Obj subtype for list
typedef struct {
    Obj obj;
    ValueArray array;
} ObjList;

// Obj subtype for table
typedef struct {
    Obj obj;
    Table table;
} ObjTable;

// Get the obj type tag from a Value
#define OBJ_TYPE(val) (AS_OBJ(val)->type)

// Macros for checking the type of an Obj
#define IS_STRING(val)          is_obj_type(val, OBJ_STRING)
#define IS_FUNCTION(val)        is_obj_type(val, OBJ_FUNCTION)
#define IS_CLOSURE(val)         is_obj_type(val, OBJ_CLOSURE)
#define IS_NATIVE(val)          is_obj_type(val, OBJ_NATIVE)
#define IS_LIST(val)            is_obj_type(val, OBJ_LIST)
#define IS_TABLE(val)           is_obj_type(val, OBJ_TABLE)

// Inline function to check an Obj's type.
static inline bool is_obj_type(IcoValue val, ObjType target_type) {
    // "val" is used twice, so an inline function is used instead
    // of a macro so that "val" is not executed/evaluated twice.
    return IS_OBJ(val) && AS_OBJ(val)->type == target_type;
}

// Macros to cast an obj Value into a specific Obj type
#define AS_STRING(val)          ((ObjString*)AS_OBJ(val))
#define AS_C_STRING(val)        (((ObjString*)AS_OBJ(val))->chars)
#define AS_FUNCTION(val)        ((ObjFunction*)AS_OBJ(val))
#define AS_CLOSURE(val)         ((ObjClosure*)AS_OBJ(val))
#define AS_NATIVE(val)          ((ObjNative*)AS_OBJ(val))
#define AS_NATIVE_C_FUNC(val)   (((ObjNative*)AS_OBJ(val))->function)
#define AS_LIST(val)            ((ObjList*)AS_OBJ(val))
#define AS_TABLE(val)           ((ObjTable*)AS_OBJ(val))

// For getting the canonical index (for list and string)
#define TRUE_INT_IDX(i, size) (i >= 0 ? i : size + i)

// Create a ObjString by copying the content of a C string
// into a newly allocated block
ObjString* copy_and_create_str_obj(const char* source_str, int length);

// Create a ObjString by taking ownership of the passed
// char* (which points to an already allocated block)
ObjString* take_own_and_create_str_obj(char* chars, int length);

// Get the substring from start to end (INCLUSIVE) of the string str.
// If start > end, a reversed substring is created and returned.
// Assume the 2 passed indices (start and end) are valid indices.
ObjString* get_substring_obj(ObjString* str, int start, int end);

// Create a new ObjUpvalue that points to the passed Value slot.
ObjUpValue* new_upvalue_obj(IcoValue* slot);

// Create a new ObjFunction and return its address.
ObjFunction* new_function_obj();

// Create a new ObjClosure that wraps the given function.
ObjClosure* new_closure_obj(ObjFunction* function);

// Create a new ObjNative from a C function of type NativeFn.
ObjNative* new_native_func_obj(NativeFn c_func, int arity, ObjString* name);

// Create a new ObjList.
ObjList* new_list_obj();

// Get the sublist from start to end (INCLUSIVE) of the list "list".
// If start > end, a reversed sublist is created and returned.
// Assume the 2 passed indices (start and end) are valid indices.
ObjList* get_sublist_obj(ObjList* list, int start, int end);

// Create a new ObjTable
ObjTable* new_table_obj();

// Print an Obj (which is an IcoValue)
void print_object(IcoValue val);

#endif //!ICO_OBJECT_H