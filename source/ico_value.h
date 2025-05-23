#ifndef ICO_VALUE_H
#define ICO_VALUE_H

#include "ico_common.h"

// The parent type for all heap-allocated types.
// The "struct Obj" is defined in another module
// due to cyclic dependency issues.
typedef struct Obj Obj ;

// Forward declare the obj's string subtype
typedef struct ObjString ObjString;

// The types of values from the VM's POV
typedef enum {
    VAL_BOOL,   // Boolean
    VAL_NULL,   // Null
    VAL_INT,    // 64-bit signed long int
    VAL_FLOAT,  // IEEE 754 double-precision float
    VAL_OBJ,    // For heap-allocated types
    VAL_ERROR,  // Special value type for error,
                // can't be created by users.
} ValueType;

// A tagged union that can hold any type
typedef struct {
    ValueType type;
    union {
        bool boolean;
        long num_int;
        double num_float;
        Obj* obj;
        char* error;
    } as;
} Value;

// Macros to convert native C values to Ico Value
#define BOOL_VAL(b)     ((Value){VAL_BOOL, {.boolean = b}})
#define NULL_VAL        ((Value){VAL_NULL, {.num_int = 0}})
#define INT_VAL(i)      ((Value){VAL_INT, {.num_int = i}})
#define FLOAT_VAL(f)    ((Value){VAL_FLOAT, {.num_float = f}})
#define OBJ_VAL(o)      ((Value){VAL_OBJ, {.obj = (Obj*)o}})
#define ERROR_VAL(s)    ((Value){VAL_ERROR, {.error = s}})

// Macros to convert an Ico Value to a C-native value
#define AS_BOOL(val)    ((val).as.boolean)
#define AS_INT(val)     ((val).as.num_int)
#define AS_FLOAT(val)   ((val).as.num_float)
#define AS_OBJ(val)     ((val).as.obj)
#define AS_ERROR(val)   ((val).as.error)

// Macros to check the type of an Ico Value
#define IS_BOOL(val)    ((val).type == VAL_BOOL)
#define IS_NULL(val)    ((val).type == VAL_NULL)
#define IS_INT(val)     ((val).type == VAL_INT)
#define IS_FLOAT(val)   ((val).type == VAL_FLOAT)
#define IS_OBJ(val)     ((val).type == VAL_OBJ)
#define IS_ERROR(val)   ((val).type == VAL_ERROR)

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

#endif // !ICO_VALUE_H

