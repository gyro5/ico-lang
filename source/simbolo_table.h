/*
This module implements a hash table for clox. The hash
table will contain entries, each of which will have an
ObjString as its key and a Value as its value.
*/

#ifndef CLOX_TABLE_H
#define CLOX_TABLE_H

#include "clox_common.h"
#include "clox_value.h"

// The struct for each entry
typedef struct {
    ObjString* key;
    Value value;
} Entry;

// The struct for the hash table
typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

// Initialize the passed hash table
void init_table(Table* table);

// Free a table and its entries
void free_table(Table* table);

// Get the value of a key in the table into the "dest"
// Value pointer and return true if it exists. Otherwise
// return false.
bool table_get(Table* table, ObjString* key, Value* dest);

// Add or set an entry in the table. Return true if it
// is a new entry, and false if it is an existing entry.
bool table_set(Table* table, ObjString* key, Value value);

// Delete the entry with the passed key from the table and
// return true if successful.
bool table_delete(Table* table, ObjString* key);

// Copy all entries from one table to another
void table_add_all(Table* from, Table* to);

// Find a raw C string in the hash table. This function is
// used for string interning in the interpreter.
ObjString* table_find_string(Table* table, const char* str, int length, uint32_t hash);

// Remove all entries that are marked white (aka. unreachable)
void table_remove_white(Table* table);

#endif