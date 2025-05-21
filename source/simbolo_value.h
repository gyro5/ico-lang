#ifndef CLOX_VALUE_H
#define CLOX_VALUE_H

#include "clox_common.h"

// The parent type for all heap-allocated types
// The "struct Obj" is defined in another module
// due to cyclic dependency issues.
typedef struct Obj Obj ;

// Forward declare the obj's "subtypes".
// The structs are defined in clox_object.h.
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

#include <string.h>

// Using uint for easier bits manipulation
typedef uint64_t Value;

// Quiet NaN bitmask
#define QNAN     ((uint64_t)0x7ffc000000000000)

// Sign bit bitmask (for Obj)
#define SIGN_BIT ((uint64_t)0x8000000000000000)

// Tags for nil, true, false
#define TAG_NIL     1     // 01
#define TAG_FALSE   2     // 10
#define TAG_TRUE    3     // 11

// Number:
#define number_val(num) num_to_val(num)
#define as_number(val)  val_to_num(val)
#define is_number(val)  (((val) & QNAN) != QNAN) // Signaling NaN is considered number too

static inline Value num_to_val(double num) {
    Value val;
    memcpy(&val, &num, sizeof(double));
    return val;
}

static inline double val_to_num(Value val) {
    double num;
    memcpy(&num, &val, sizeof(Value));
    return num;
}

// Nil:
#define nil_val         ((Value)(uint64_t)(QNAN | TAG_NIL))
#define is_nil(val)     ((val) == nil_val)

// Bool:
#define false_val       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define true_val        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define bool_val(b)     ((b) ? true_val : false_val)
#define as_bool(val)    ((val) == true_val)
#define is_bool(val)    (((val) | 1) == true_val )

// Obj:
#define obj_val(obj)    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))
#define as_obj(val)     ((Obj*)(uintptr_t)((val) & ~(SIGN_BIT | QNAN)))
#define is_obj(val)     (((val) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#else

// The types of values from the VM's POV
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,    // For heap-allocated types
} ValueType;

// A tagged union that can hold any type
typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

// Macros to convert native C values to clox Value
#define bool_val(val)   ((Value){VAL_BOOL, {.boolean = val}})
#define nil_val         ((Value){VAL_NIL, {.number = 0}})
#define number_val(val) ((Value){VAL_NUMBER, {.number = val}})
#define obj_val(object) ((Value){VAL_OBJ, {.obj = (Obj*)object}})

// Macros to convert a clox Value to a C-native value
#define as_bool(val)    ((val).as.boolean)
#define as_number(val)  ((val).as.number)
#define as_obj(val)     ((val).as.obj)

// Macros to check the type of a clox Value
#define is_bool(val)    ((val).type == VAL_BOOL)
#define is_nil(val)     ((val).type == VAL_NIL)
#define is_number(val)  ((val).type == VAL_NUMBER)
#define is_obj(val)     ((val).type == VAL_OBJ)

#endif

// ValueArray to represent a constant pool of a chunk
typedef struct {
    int capacity;
    int size;
    Value* values;
} ValueArray;

// Initialize a ValueArray
void init_value_array(ValueArray* val_arr);

// Append a value to the end of a ValueArray
void append_value_array(ValueArray* val_arr, Value val);

// Free the memory blocks of a ValueArray
void free_value_array(ValueArray* val_arr);

// Print a Value
void print_value(Value val);

// Compare two values and return true if they are equal
bool values_equal(Value a, Value b);

#endif // !CLOX_VALUE_H

