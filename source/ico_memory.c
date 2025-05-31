#include <stdlib.h>
#include <stdio.h>

#include "ico_memory.h"
#include "ico_vm.h"
#include "ico_compiler.h"

#define GC_HEAP_GROW_FACTOR 2  // Arbitrarily chosen

/*
* This function works as follows.
* old_cap:    new_cap:    operation:
* 0           != 0        allocate new block
* != 0        0           free curent allocation
* != 0        < old_cap   shrink current allocation
* != 0        > old_cap   grow curent allocation
*/
void* reallocate(void* ptr, size_t old_size, size_t new_size) {
    vm.bytes_allocated += new_size - old_size;

    if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
        // Stress testing GC: Run at every possible chance
        collect_garbage();
#endif
        // Normal GC: Run when threshold reached
        if (vm.bytes_allocated > vm.next_gc_run) {
            collect_garbage();
        }
    }

    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr == NULL) { // Realloc failed due to not enough memory
        fprintf(stderr, "Error: Out of memory.");
        exit(1);
    }

    return new_ptr;
}

static void free_one_object(Obj* obj) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)obj, obj->type);
#endif

    switch (obj->type) {
        case OBJ_STRING: {
            ObjString* obj_str = (ObjString*)obj;
            FREE_ARRAY(char, obj_str->chars, obj_str->length);
            FREE(ObjString, obj);
            break;
        }

        case OBJ_UPVALUE: {
            FREE(ObjUpValue, obj);
            // Don't free the closed-over value as it can be
            // shared by multiple ObjClosure.
            break;
        }

        case OBJ_FUNCTION: {
            ObjFunction* func = (ObjFunction*)obj;
            free_chunk(&func->chunk);
            FREE(ObjFunction, obj);
            break;
        }

        case OBJ_CLOSURE: {
            // Free the array of upvalue pointers, but don't free
            // the actual ObjUpvalue objects because they can be
            // shared by multiple closures.
            ObjClosure* closure = (ObjClosure*)obj;
            FREE_ARRAY(ObjUpValue*, closure->upvalues, closure->upvalue_count);

            FREE(ObjClosure, obj);

            // Don't free the wrapped ObjFunction as it can be
            // shared by multiple ObjClosure.
            break;
        }

        case OBJ_NATIVE: {
            FREE(ObjNative, obj);
            break;
        }
    }
}

void free_objects() {
    Obj* curr_obj = vm.allocated_objs;

    // Free the objects by traversing the Obj linked list
    while (curr_obj != NULL) {
        Obj* next_obj = curr_obj->next;
        free_one_object(curr_obj);
        curr_obj = next_obj;
    }

    free(vm.gray_stack);
}

// GC function: Mark all root objects (for the marking
// phase of mark-sweep GC)
static void mark_roots() {
    // Mark all local variables on the VM stack
    for (IcoValue* slot = vm.stack; slot < vm.stack_top; slot++) {
        mark_value(*slot);
    }

    // Mark the ObjClosure's in the call stack
    for (int i = 0; i < vm.frame_count; i++) {
        mark_object((Obj*)vm.frames[i].closure);
    }

    // Mark all upvalues in the list of open upvalues
    for (ObjUpValue* u = vm.open_upvalues; u != NULL; u = u->next) {
        mark_object((Obj*)u);
    }

    // Mark all global variables
    mark_table(&vm.globals);

    // Mark objects used by the compiler
    mark_compiler_roots();
}

void mark_object(Obj* obj) {
    // Don't add null objects or gray/black objects
    if (obj == NULL || obj->is_marked) return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)obj);
    print_value(OBJ_VAL(obj));
    printf("\n");
#endif

    // Mark the object as gray (is_marked and added to gray stack)
    // if the object type has references to other Objs.
    obj->is_marked = true;
    switch (obj->type) {
        case OBJ_STRING: case OBJ_NATIVE:
            // These types don't have any reference --> Don't add to gray stack
            break;

        default:
            if (vm.gray_capacity < vm.gray_count + 1) {
                vm.gray_capacity = GROW_CAPACITY(vm.gray_capacity);
                vm.gray_stack = (Obj**)realloc(vm.gray_stack,
                    sizeof(Obj**) * vm.gray_capacity);
                // Use system's realloc() because we can't use Ico's reallocate()

                // Check for allocation error
                if (vm.gray_stack == NULL) {
                    fprintf(stderr, "Error: Out of memory.");
                    exit(1);
                }
            }
            vm.gray_stack[vm.gray_count++] = obj;
            break;
    }
}

void mark_value(IcoValue val) {
    if (IS_OBJ(val)) mark_object(AS_OBJ(val));
}

void mark_table(Table* table) {
    for (uint32_t i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        mark_value(entry->key);
        mark_value(entry->value);
    }
}

// Mark all values in a ValueArray
static void mark_value_array(ValueArray* array) {
    for (int i = 0; i < array->size; i++) {
        mark_value(array->values[i]);
    }
}

// Trace the object references of one object and mark them,
// then mark the original object black.
static void blacken_one_object(Obj* obj) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)obj);
    print_value(OBJ_VAL(obj));
    printf("\n");
#endif

    switch (obj->type) {
        case OBJ_UPVALUE:
            // Mark closed-over value that is no longer on the stack
            mark_value(((ObjUpValue*)obj)->closed);
            break;

        case OBJ_FUNCTION: {
            ObjFunction* func = (ObjFunction*)obj;

            // Mark the function's name
            mark_object((Obj*)func->name);

            // Mark all constants in the constant pool
            mark_value_array(&func->chunk.const_pool);

            break;
        }

        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)obj;

            // Mark the wrapped ObjFunction
            mark_object((Obj*)closure->function);

            // Mark all upvalues of this closure
            for (int i = 0; i < closure->upvalue_count; i++) {
                mark_object((Obj*)closure->upvalues[i]);
            }

            break;
        }

        default: {
            fprintf(stderr, "Error in ico_memory.c: This should be unreachable.");
            break;
        }
    }
}

// Trace through all object references and mark
// the reachable objects.
static void trace_references() {
    while (vm.gray_count > 0) {
        Obj* obj = vm.gray_stack[--vm.gray_count];
        blacken_one_object(obj);
    }
}

// Free all white objects
static void sweep() {
    Obj* prev = NULL;
    Obj* curr = vm.allocated_objs;

    // Traverse the intrusive linked list of allocated objects
    while (curr != NULL) {
        if (curr->is_marked) { // Marked --> Don't remove --> Continue
            curr->is_marked = false; // Reset for the next GC run
            prev = curr;
            curr = curr->next;
        }
        else { // Unmarked --> Remove from linked list then free
            Obj* to_be_freed = curr;

            // Remove from linked list
            curr = curr->next;
            if (prev != NULL) {
                prev->next = curr;
            }
            else {
                vm.allocated_objs = curr;
            }

            // Free
            free_one_object(to_be_freed);
        }
    }
}

void collect_garbage() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytes_allocated;
#endif

    // Start by marking the roots
    mark_roots();

    // Then trace the object references
    trace_references();

    // Then remove white objects from the string interning
    // table to avoid dangling pointers
    table_remove_white(&vm.strings);

    // Sweep (Free) all white objects
    sweep();

    // Update the next GC trigger threshold
    vm.next_gc_run = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
        before - vm.bytes_allocated, before, vm.bytes_allocated, vm.next_gc_run);
#endif
}