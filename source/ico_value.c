#include <stdio.h>

#include "ico_memory.h"
#include "ico_value.h"
#include "ico_object.h"

void init_value_array(ValueArray* val_arr) {
    val_arr->values = NULL;
    val_arr->capacity = 0;
    val_arr->size = 0;
}

void append_value_array(ValueArray* val_arr, IcoValue val) {
    // Increase array capacity if needed
    if (val_arr->size + 1 > val_arr->capacity) {
        int old_cap = val_arr->capacity;
        val_arr->capacity = GROW_CAPACITY(old_cap);
        val_arr->values = GROW_ARRAY(IcoValue, val_arr->values, old_cap, val_arr->capacity);
    }

    // Append the new value to the end of the array
    val_arr->values[val_arr->size] = val;
    val_arr->size++;
}

void free_value_array(ValueArray* val_arr) {
    FREE_ARRAY(IcoValue, val_arr->values, val_arr->capacity);
    init_value_array(val_arr);
}

void print_value(IcoValue val) {
    switch (val.type) {
        case VAL_BOOL:
            printf(AS_BOOL(val) ? ":)" : ":(");
            break;

        case VAL_NULL:
            printf("#");
            break;

        case VAL_INT:
            printf("%ld", AS_INT(val));
            break;

        case VAL_FLOAT:
            // "%g" -> print double using the more approriate
            // of normal or exponential notation.
            printf("%g", AS_FLOAT(val));
            break;

        case VAL_OBJ:
            print_object(val);
            break;

        case VAL_ERROR:
            printf("TODO error %s", AS_ERROR(val));
            break;
    }
}

bool values_equal(IcoValue b, IcoValue a) {
    // Check for same type first
    if (a.type != b.type) {
        return false;
    }

    // Then compare the actual values
    switch (a.type) {
        case VAL_BOOL:      return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL:      return true;
        case VAL_INT:       return AS_INT(a) == AS_INT(b);
        case VAL_FLOAT:     return AS_FLOAT(a) == AS_FLOAT(b);

        case VAL_OBJ:       return AS_OBJ(a) == AS_OBJ(b);
        // Note: thanks to string interning, doing equality comparison
        // on 2 strings can be done with just pointer comparison.
        // Pointer comparison also works for other types of Obj.

        case VAL_ERROR:     // Always not equal
        default:            return false; // Unreachable
    }
}