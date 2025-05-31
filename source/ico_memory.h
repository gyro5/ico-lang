#ifndef ICO_MEMORY_H
#define ICO_MEMORY_H

#include "ico_common.h"
#include "ico_object.h"
#include "ico_table.h"

// Return the new capacity when growing is required
#define GROW_CAPACITY(cap) ((cap) < 8 ? 8 : (cap) * 2)

// Use the reallocate() function with pointer type cast
#define GROW_ARRAY(type, pointer, old_cap, new_cap) \
    (type*) reallocate(pointer, sizeof(type) * (old_cap), \
            sizeof(type) * (new_cap))

// Use reallocate() to free the array
#define FREE_ARRAY(type, ptr, old_cap) \
    reallocate(ptr, sizeof(type) * (old_cap), 0)

// Grow the allocated block at ptr to the new size
void* reallocate(void* ptr, size_t old_size, size_t new_size);

// Allocate a new block for "count" elements of the given C type
#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

// Free a block pointed to by "ptr" of type "type"
#define FREE(type, ptr) reallocate(ptr, sizeof(type), 0)


// Free all remaining heap-allocated objects and data owned by the VM
void free_objects();

// GC function: Mark one Obj-type object as gray
void mark_object(Obj* obj);

// GC function: Mark one heap-allocated object
void mark_value(IcoValue val);

// GC function: Mark all keys and all values in a hash table
void mark_table(Table* table);

// Collect all unreachable objects and free them (aka. the GC function)
void collect_garbage();

#endif // !ICO_MEMORY_H
