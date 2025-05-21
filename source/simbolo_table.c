#include <stdlib.h>
#include <string.h>

#include "clox_memory.h"
#include "clox_table.h"
#include "clox_value.h"

// The maximum load factor of a table
#define TABLE_MAX_LOAD 0.75

//------------------------------
//      STATIC FUNCTIONS
//------------------------------

// Find an entry slot for the new key in the backing array,
// and return a pointer to that slot. Input is "entries" instead
// of the table so that newly allocated entry array can be used.
static Entry* find_entry(Entry* entries, int capacity, ObjString* key) {
    // Calculate the supposed index from the hash
    uint32_t index = key->hash & (capacity - 1);

    Entry* tombstone = NULL;

    // Linear probing to find the index where the entry belongs
    for (;;) {
        Entry* entry = entries + index;

        // Found empty slot or tombstone
        if (entry->key == NULL) {
            /*
            IMPORTANT: to not break the probing sequence,
            we only return when we reach an EMPTY slot,
            not a tombstone. This guarantees that we searched
            pass all tombstones.

            The tombstone (instead of the empty slot) is returned
            to reuse the tombstone slot and save space.
            */

            if (is_nil(entry->value)) { // Empty slot
                return tombstone != NULL ? tombstone : entry;
            }
            else { // Tombstone
                if (tombstone == NULL) tombstone = entry;
            }
        }
        // Found the key
        else if (entry->key == key) {
            return entry;
        }

        index = (index + 1) & (capacity - 1);

        // Note: infinite loop CAN'T happen thanks to
        // the load factor being maintained.
    }
}

// Resize the table's backing array to the new capacity
static void adjust_table_capacity(Table* table, int new_capacity) {
    Entry* new_entries = allocate(Entry, new_capacity);

    // Initialize the new memory block, as C doesn't
    // guarantee the allocated block is clean
    for (int i = 0; i < new_capacity; i++) {
        new_entries[i].key = NULL;
        new_entries[i].value = nil_val;
    }

    // Re-inserting all existing elements.
    // Recalculate count to exclude tombstones.
    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = find_entry(new_entries, new_capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;

        table->count++;
    }

    // Free the old entry array
    free_array(Entry, table->entries, table->capacity);

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
    free_array(Entry, table->entries, table->capacity);
    init_table(table);
}

bool table_get(Table* table, ObjString* key, Value* dest) {
    // For optimization and to not access a NULL entry array
    if (table->count == 0) return false;

    Entry* entry = find_entry(table->entries, table->capacity, key);

    // Not found
    if (entry->key == NULL) return false;

    // Found
    *dest = entry->value;
    return true;
}

bool table_set(Table* table, ObjString* key, Value value) {
    // Check the load factor and grow the table as needed
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int new_cap = grow_capacity(table->capacity);
        adjust_table_capacity(table, new_cap);
    }

    // Find the index where the entry belongs
    Entry* entry = find_entry(table->entries, table->capacity, key);

    // Check for new key
    bool is_new_key = entry->key == NULL;

    // Only increment count when inserting into an EMPTY slot
    if (is_new_key && is_nil(entry->value)) table->count++;

    // Setup the new entry
    entry->key = key;
    entry->value = value;

    return is_new_key;
}

bool table_delete(Table* table, ObjString* key) {
    // For optimization and to not access a NULL entry array
    if (table->count == 0) return false;

    // Find the entry
    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false; // Not found

    // Found -> Place a tombstone in the entry to delete
    entry->key = NULL;
    entry->value = bool_val(true);
    return true;
}

void table_add_all(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            table_set(to, entry->key, entry->value);
        }
    }
}

ObjString* table_find_string(Table* table, const char* str, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash & (table->capacity - 1);
    for (;;) {
        Entry* entry = table->entries + index;

        // Empty slot or tombstone
        if (entry->key == NULL) {
            // Stop if found an actual empty slot -> String not found
            if (is_nil(entry->value)) return NULL;
        }
        // Slot with string -> Check for equality by comparing
        // hash and length as quick checks, then verify with
        // char-to-char comparison.
        else if (entry->key->length == length &&
                 entry->key->hash == hash &&
                 memcmp(entry->key->chars, str, length) == 0) {
            return entry->key; // Found the interned ObjString
        }

        index = (index + 1) & (table->capacity - 1);
    }
}

void table_remove_white(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.is_marked) {
            // Author's code:
            // table_delete(table, entry->key);
            //
            // My version: Copied code from table_delete()
            // to avoid redundant check
            entry->key = NULL;
            entry->value = bool_val(true);
        }
    }
}