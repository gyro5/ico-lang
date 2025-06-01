#include <stdio.h>
#include <string.h>

#include "ico_memory.h"
#include "ico_object.h"
#include "ico_value.h"
#include "ico_vm.h"
#include "ico_table.h"

//---------------------------------------
//      STATIC FUNCTIONS AND MACROS
//---------------------------------------

// Allocate an Obj with specific size and type that depend on the
// specific Obj subtype. Also set up metadata for memory management purpose.
static Obj* allocate_object(size_t size, ObjType type) {
    // "size" depends on the specific Obj subtype
    Obj* obj = (Obj*)reallocate(NULL, 0, size);

    obj->hash = 0; // No hash by default
    obj->type = type; // Set the type tag
    obj->is_marked = false; // All objects start out as unmarked

    // Add to head of Obj linked list (for memory management)
    obj->next = vm.allocated_objs;
    vm.allocated_objs = obj;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)obj, size, type);
#endif

    return obj;
}

// Allocate a memory block for a specific Obj subtype.
#define ALLOCATE_OBJ(type, type_enum) \
    (type*)allocate_object(sizeof(type), type_enum)

// Allocate and create an ObjString with the passed content and length.
// The passed char* (allocated block) is used as-is and owned by the
// returned ObjString.
static ObjString* allocate_str_obj(char* chars, int length, uint32_t hash) {
    ObjString* obj_str = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    obj_str->chars = chars;
    obj_str->length = length;
    ((Obj*)obj_str)->hash = hash;

    // To prevent the ObjString from being sweeped by the GC
    IcoValue val_str = OBJ_VAL(obj_str);
    push(val_str);

    // Intern the string whenever we create a new one
    table_set(&vm.strings, val_str, NULL_VAL);
    pop();

    return obj_str;
}

// Return the FNV-1a hash code of a char/byte array
static uint32_t hash_chars(const char* str, int length) {
    // Pre-chosen constant of FNV-1a hash algorithm
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)str[i];
        hash *= 16777619; // Another pre-chosen constant
    }

    return hash;
}

// Return the FNV-1a hash code of a memory address
static uint32_t hash_address(void* address) {
    union {
        void* address;
        char chars[4];
    } temp;
    temp.address = address;

    // Treat the address as an array of 4 chars
    return hash_chars(temp.chars, 4);
}

// Helper function to print an ObjFunction
static void print_function_obj(ObjFunction* func) {
    if (func->name == NULL) {
        // For DEBUG_TRACE_EXECUTION as users have
        // no way to print the top-level "function".
        printf("<script>");
        return;
    }
    printf("<fn %s()>", func->name->chars);
}

//------------------------------
//      HEADER FUNCTIONS
//------------------------------

ObjString* copy_and_create_str_obj(const char* source_str, int length) {
    // Calculate the hash. Use source_str to not include '\0'
    uint32_t hash = hash_chars(source_str, length);

    // Check for interned string and return that if available
    ObjString* interned = table_find_string(&vm.strings, source_str, length, hash);
    if (interned != NULL) return interned;

    // Allocate memory for the content of the ObjString,
    // including the NULL terminator.
    char* new_str = ALLOCATE(char, length + 1);

    // Copy the string content and add the NULL terminator.
    memcpy(new_str, source_str, length);
    new_str[length] = '\0';

    return allocate_str_obj(new_str, length, hash);
}

ObjString* take_own_and_create_str_obj(char* chars, int length) {
    // This function is used for when the user creates new strings
    // at runtime, such as by string concatenation.

    // Calculate the hash
    uint32_t hash = hash_chars(chars, length);

    // Check for interned string
    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        // Found interned -> Free the current char array and use the interned one.
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    // Otherwise, keep and use the current char array
    return allocate_str_obj(chars, length, hash);
}

ObjUpValue* new_upvalue_obj(IcoValue* slot) {
    ObjUpValue* upvalue = ALLOCATE_OBJ(ObjUpValue, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->closed = NULL_VAL;
    upvalue->next = NULL;
    // No hash because upvalues are not first-class
    return upvalue;
}

ObjFunction* new_function_obj() {
    ObjFunction* func = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    func->arity = 0;
    func->name = NULL;
    init_chunk(&func->chunk);
    func->upvalue_count = 0;
    // No hash because functions (!= closure) are not first-class
    return func;
}

ObjClosure* new_closure_obj(ObjFunction* function) {
    // Allocate and initialize the array of upvalues
    ObjUpValue** upvalues = ALLOCATE(ObjUpValue*, function->upvalue_count);
    for (int i = 0; i < function->upvalue_count; i++) {
        upvalues[i] = NULL;
    }

    // Allocate the ObjClosure
    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    ((Obj*)closure)->hash = hash_address(closure);

    return closure;
}

ObjNative* new_native_func_obj(NativeFn c_func, int arity, ObjString* name) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function = c_func;
    ((Obj*)native)->hash = hash_address(native);
    native->arity = arity;
    native->name = name;
    return native;
}

void print_object(IcoValue val) {
    switch (OBJ_TYPE(val)) {
        case OBJ_STRING:
            printf("%s", AS_C_STRING(val));
            break;

        case OBJ_UPVALUE:
            // Should normally be unreachable as upvalues are
            // not first-class values
            printf("<upvalue>");
            break;

        case OBJ_FUNCTION:
            // Should normally be unreachable
            print_function_obj(AS_FUNCTION(val));
            break;

        case OBJ_CLOSURE:
            print_function_obj(AS_CLOSURE(val)->function);
            break;

        case OBJ_NATIVE:
            printf("<native fn %s()>", AS_NATIVE(val)->name->chars);
            break;
    }
}