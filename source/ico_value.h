#ifndef ICO_VALUE_H
#define ICO_VALUE_H

#include "ico_common.h"

// The parent type for all heap-allocated types. The "struct Obj" is
// defined in another module due to cyclic dependency issues.
typedef struct Obj Obj ;

// Forward declare the obj's string subtype
typedef struct ObjString ObjString;

// The types of values from the VM's POV
typedef enum : int {
    VAL_BOOL,   // Boolean
    VAL_NULL,   // Null
    VAL_INT,    // 64-bit signed long int
    VAL_FLOAT,  // IEEE 754 double-precision float
    VAL_OBJ,    // For heap-allocated types
    VAL_ERROR,  // Special value type for error, can't be created by users.
} ValueType;

// A tagged union that can hold any type
typedef struct {
    ValueType type;
    bool is_num;
    union {
        bool boolean;
        long num_int;
        double num_float;
        Obj* obj;
        char* error;
        uint32_t ui32[2];
    } as;
} IcoValue;

// We always want IcoValue to be no more than 16 bytes
_Static_assert( sizeof(IcoValue) <= 16,
    "C23 enum type is not supported, IcoValue is more than 16 bytes. Please disable this flag.");

// By hashing the string ":)" and ":(" in advance
#define TRUE_HASH (uint32_t)2231767820
#define FALSE_HASH (uint32_t)2248545439

// Macros to convert native C values to Ico Value
#define BOOL_VAL(b)     ((IcoValue){VAL_BOOL, false, {.boolean = b}})
#define NULL_VAL        ((IcoValue){VAL_NULL, false, {.num_int = 0}})
#define INT_VAL(i)      ((IcoValue){VAL_INT, true, {.num_int = i}})
#define FLOAT_VAL(f)    ((IcoValue){VAL_FLOAT, true, {.num_float = f}})
#define OBJ_VAL(o)      ((IcoValue){VAL_OBJ, false, {.obj = (Obj*)o}})
#define ERROR_VAL(s)    ((IcoValue){VAL_ERROR, false, {.error = s}})

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
#define IS_NUMBER(val)  ((val).is_num)

// ValueArray to represent a constant pool of a chunk
typedef struct {
    int capacity;
    int size;
    IcoValue* values;
} ValueArray;

// Initialize a ValueArray
void init_value_array(ValueArray* val_arr);

// Append a value to the end of a ValueArray
void append_value_array(ValueArray* val_arr, IcoValue val);

// Free the memory blocks of a ValueArray
void free_value_array(ValueArray* val_arr);

// Print a Value
void print_value(IcoValue val);

// Compare two values and return true if they are equal
bool values_equal(IcoValue a, IcoValue b);

#endif // !ICO_VALUE_H

