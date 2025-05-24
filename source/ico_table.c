#include <stdlib.h>
#include <string.h>

#include "ico_memory.h"
#include "ico_table.h"
#include "ico_value.h"
#include "ico_object.h"

// The maximum load factor of a table
#define TABLE_MAX_LOAD 0.75

Entry absent_entry = {NULL_VAL, NULL_VAL};

//------------------------------
//      STATIC FUNCTIONS
//------------------------------

// Empty: key=null, value=null
// Tombstone: key=null, value=true

static Entry* find_int_entry(Entry* entries, uint32_t capacity, IcoValue vkey) {
    // See find_obj_entry for algorithm explanation comments

    uint32_t index = (vkey.as.ui32[0] ^ vkey.as.ui32[1]) & (capacity - 1);
    long ikey = AS_INT(vkey);
    Entry* tombstone = NULL;
    for (;;) {
        Entry* entry = entries + index;
        IcoValue curr_key = entry->key;
        if (IS_NULL(curr_key)) {
            if (IS_NULL(entry->value)) { // Empty slot
                return tombstone != NULL ? tombstone : entry;
            }
            else { // Tombstone
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (IS_INT(curr_key) && AS_INT(curr_key) == ikey) { // Found the key
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

static Entry* find_float_entry(Entry* entries, uint32_t capacity, IcoValue vkey) {
    // See find_obj_entry for algorithm explanation comments

    uint32_t index = (vkey.as.ui32[0] ^ vkey.as.ui32[1]) & (capacity - 1);
    double fkey = AS_FLOAT(vkey);
    Entry* tombstone = NULL;
    for (;;) {
        Entry* entry = entries + index;
        IcoValue curr_key = entry->key;
        if (IS_NULL(curr_key)) {
            if (IS_NULL(entry->value)) { // Empty slot
                return tombstone != NULL ? tombstone : entry;
            }
            else { // Tombstone
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (IS_FLOAT(curr_key) && AS_FLOAT(curr_key) == fkey) { // Found the key
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

static Entry* find_bool_entry(Entry* entries, uint32_t capacity, bool vkey) {
    // See find_obj_entry for algorithm explanation comments

    uint32_t hash = vkey ? TRUE_HASH : FALSE_HASH;
    uint32_t index = hash & (capacity - 1);
    Entry* tombstone = NULL;
    for (;;) {
        Entry* entry = entries + index;
        IcoValue curr_key = entry->key;
        if (IS_NULL(curr_key)) {
            if (IS_NULL(entry->value)) { // Empty slot
                return tombstone != NULL ? tombstone : entry;
            }
            else { // Tombstone
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (IS_BOOL(curr_key) && AS_BOOL(curr_key) == vkey) { // Found the key
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

static Entry* find_obj_entry(Entry* entries, uint32_t capacity, Obj* target) {
    // Calculate the supposed index from the hash
    uint32_t index = target->hash & (capacity - 1);

    Entry* tombstone = NULL;

    // Linear probing to find the index where the entry belongs
    for (;;) {
        Entry* entry = entries + index;
        IcoValue curr_key = entry->key;

        // Found an empty slot or a tombstone
        if (IS_NULL(curr_key)) {
            // IMPORTANT: to not break the probing sequence, we only return when
            // we reach an EMPTY slot,not a tombstone. This guarantees that
            // we searched pass all tombstones.

            // The tombstone (instead of the empty slot) is returned
            // to reuse the tombstone slot and save space.

            if (IS_NULL(entry->value)) { // Empty slot
                return tombstone != NULL ? tombstone : entry;
            }
            else { // Tombstone
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (IS_OBJ(curr_key) && AS_OBJ(curr_key) == target) { // Found the key
            return entry;
        }

        index = (index + 1) & (capacity - 1);

        // Infinite loop CAN'T happen thanks to the load factor being maintained.
    }
}

// The main find_entry function:
// Find an entry slot for the new key in the backing array,
// and return a pointer to that slot. Input is "entries" instead
// of the table so that newly allocated entry array can be used.
// Return an empty slot if can't find target. Return NULL if
// target key is invalid.
static Entry* find_entry(Entry* entries, uint32_t capacity, IcoValue target) {
    // Switch case based on key type
    switch (target.type) {
        case VAL_BOOL: return find_bool_entry(entries, capacity, AS_BOOL(target));
        case VAL_INT: return find_int_entry(entries, capacity, target);
        case VAL_FLOAT: return find_float_entry(entries, capacity, target);
        case VAL_OBJ: return find_obj_entry(entries, capacity, AS_OBJ(target));

        case VAL_ERROR: case VAL_NULL: default:
            // Should be unreachable --> Check in VM when try to access // TODO
            return &absent_entry;
    }
}

// Resize the table's backing array to the new capacity
static void adjust_table_capacity(Table* table, uint32_t new_capacity) {
    Entry* new_entries = ALLOCATE(Entry, new_capacity);

    // Initialize the new memory block, as C doesn't
    // guarantee the allocated block is clean
    for (uint32_t i = 0; i < new_capacity; i++) {
        new_entries[i].key = NULL_VAL;
        new_entries[i].value = NULL_VAL;
    }

    // Re-inserting all existing elements.
    // Recalculate count to exclude tombstones.
    table->count = 0;
    for (uint32_t i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (IS_NULL(entry->key)) continue;

        Entry* dest = find_entry(new_entries, new_capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;

        table->count++;
    }

    // Free the old entry array
    FREE_ARRAY(Entry, table->entries, table->capacity);

    table->entries = new_entries;
    table->capacity = new_capacity;
}

//------------------------------
//      HEADER FUNCTIONS
//------------------------------

void init_table(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void free_table(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    init_table(table);
}

bool table_get(Table* table, IcoValue key, IcoValue* dest) {
    // For optimization and to not access a NULL entry array
    if (table->count == 0) return false;

    Entry* entry = find_entry(table->entries, table->capacity, key);

    // Not found
    if (IS_NULL(entry->key)) return false;

    // Found
    *dest = entry->value;
    return true;
}

bool table_set(Table* table, IcoValue key, IcoValue value) {
    // Check the load factor and grow the table as needed
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        uint32_t new_cap = GROW_CAPACITY(table->capacity);
        adjust_table_capacity(table, new_cap);
    }

    // Find the index where the entry belongs
    Entry* entry = find_entry(table->entries, table->capacity, key);

    // Check for new key
    bool is_new_key = IS_NULL(entry->key);

    // Only increment count when inserting into an EMPTY slot
    if (is_new_key && IS_NULL(entry->value)) table->count++;

    // Setup the new entry
    entry->key = key;
    entry->value = value;

    return is_new_key;
}

bool table_delete(Table* table, IcoValue key) {
    // For optimization and to not access a NULL entry array
    if (table->count == 0) return false;

    // Find the entry
    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (IS_NULL(entry->key)) return false; // Not found

    // Found -> Place a tombstone in the entry to delete
    entry->key = NULL_VAL;
    entry->value = BOOL_VAL(true);
    return true;
}

void table_add_all(Table* from, Table* to) {
    for (uint32_t i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (!IS_NULL(entry->key)) {
            table_set(to, entry->key, entry->value);
        }
    }
}

ObjString* table_find_string(Table* table, const char* str, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash & (table->capacity - 1);
    for (;;) {
        Entry* entry = table->entries + index;
        IcoValue curr_key = entry->key;

        // Empty slot or tombstone
        if (IS_NULL(curr_key)) {
            // Stop if found an actual empty slot -> String not found
            if (IS_NULL(entry->value)) return NULL;
        }
        // Slot with string -> Check for equality by comparing
        // hash and length as quick checks, then verify with
        // char-to-char comparison.
        else if (IS_STRING(curr_key)) {
            ObjString* str_key = AS_STRING(curr_key);
            if (str_key->length == length && AS_OBJ(curr_key)->hash == hash
                    && memcmp(str_key->chars, str, length) == 0) {
                // printf("Found interned string %.*s\n", length, str);
                return str_key; // Found the interned ObjString
            }
        }

        index = (index + 1) & (table->capacity - 1);
    }
}

// void table_remove_white(Table* table) {
//     for (uint32_t i = 0; i < table->capacity; i++) {
//         Entry* entry = &table->entries[i];
//         if (entry->key != NULL && !entry->key->obj.is_marked) {
//             // Author's code:
//             // table_delete(table, entry->key);
//             //
//             // My version: Copied code from table_delete()
//             // to avoid redundant check
//             entry->key = NULL;
//             entry->value = bool_val(true);
//         }
//     }
// }