// Macros for memory management

#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include "clox_common.h"
#include "clox_object.h"
#include "clox_table.h"

// Return the new capacity when growing is required
#define grow_capacity(cap) ((cap) < 8 ? 8 : (cap) * 2)

// Use the reallocate() function with pointer type cast
#define grow_array(type, pointer, old_cap, new_cap) \
    (type*) reallocate(pointer, sizeof(type) * (old_cap), \
            sizeof(type) * (new_cap))

// Use reallocate() to free the array
#define free_array(type, ptr, old_cap) \
    reallocate(ptr, sizeof(type) * (old_cap), 0)

// Grow the allocated block at ptr to the new size
void* reallocate(void* ptr, size_t old_size, size_t new_size);

// Allocate a new block for "count" elements of the given C type
#define allocate(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

// Free a block pointed to by "ptr" of type "type"
#define free_ptr(type, ptr) reallocate(ptr, sizeof(type), 0)

// Free all remaining heap-allocated objects and data owned
// by the VM.
void free_objects();

// GC function: Mark one Obj-type object as gray
void mark_object(Obj* obj);

// GC function: Mark one heap-allocated object
// (Which means numbers, bools, and nil are not marked).
void mark_value(Value val);

// GC function: Mark all keys and all values in a hash table
void mark_table(Table* table);

// Collect all unreachable objects and free them (aka. the GC function)
void collect_garbage();

#endif // !CLOX_MEMORY_H
