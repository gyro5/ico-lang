#include <stdio.h>
#include <string.h>

#include "clox_memory.h"
#include "clox_value.h"
#include "clox_object.h"

void init_value_array(ValueArray* val_arr) {
    val_arr->values = NULL;
    val_arr->capacity = 0;
    val_arr->size = 0;
}

void append_value_array(ValueArray* val_arr, Value val) {
    // Increase array capacity if needed
    if (val_arr->size + 1 > val_arr->capacity) {
        int old_cap = val_arr->capacity;
        val_arr->capacity = grow_capacity(old_cap);
        val_arr->values = grow_array(Value, val_arr->values, old_cap, val_arr->capacity);
    }

    // Append the new value to the end of the array
    val_arr->values[val_arr->size] = val;
    val_arr->size++;
}

void free_value_array(ValueArray* val_arr) {
    free_array(Value, val_arr->values, val_arr->capacity);
    init_value_array(val_arr);
}

void print_value(Value val) {
#ifdef NAN_BOXING
    if (is_bool(val)) {
        printf(as_bool(val) ? "true" : "false");
    }
    else if (is_nil(val)) {
        printf("nil");
    }
    else if (is_number(val)) {
        printf("%g", as_number(val));
    }
    else if (is_obj(val)) {
        print_object(val);
    }
#else
    switch (val.type) {
        case VAL_BOOL:
            printf(as_bool(val) ? "true" : "false");
            break;

        case VAL_NIL:
            printf("nil");
            break;

        case VAL_NUMBER:
            // "%g" -> print double using the more approriate
            // of normal or exponential notation.
            printf("%g", as_number(val));
            break;

        case VAL_OBJ:
            print_object(val);
            break;
    }
#endif
}

bool values_equal(Value a, Value b) {
#ifdef NAN_BOXING

    #ifdef NAN_BOXING_IEEE_754
    if (is_number(a) && is_number(b)) {
        return as_number(a) == as_number(b);
    }
    #endif

    return a == b;

#else

    // Check for same type first
    if (a.type != b.type) {
        return false;
    }

    // Then compare the actual values
    switch (a.type) {
        case VAL_BOOL:   return as_bool(a) == as_bool(b);
        case VAL_NIL:    return true;
        case VAL_NUMBER: return as_number(a) == as_number(b);
        case VAL_OBJ:    return as_obj(a) == as_obj(b); // Compare the Obj* pointers
        // Note: thanks to string interning, doing equality comparison
        // on 2 strings can be done with just pointer comparison.
        // Pointer comparison also works for other types of Obj.

        default:            return false; // Unreachable
    }
#endif
}