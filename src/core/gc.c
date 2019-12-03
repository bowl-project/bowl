#include "gc.h"

// memory heaps
static u8 *gc_heap_dst = NULL;
static u8 *gc_heap_src = NULL;
static u64 gc_heap_ptr = 0;
static u64 gc_heap_size = 0;

// list of weak references to all loaded libraries
static LimeValue *gc_libraries = NULL;
static u64 gc_libraries_capacity = 0;
static u64 gc_libraries_size = 0;

// cached version of the out of heap exception
static struct lime_value gc_heap_exception = {
    .type = LimeStringValue,
    .location = NULL,
    .hash = 1393540937171791872,
    .symbol = {
        .length = 13,
        .bytes = "out of memory"
    }
};

static struct lime_value gc_library_finalize_exception = {
    .type = LimeStringValue,
    .location = NULL,
    .hash = 365473802495556352,
    .symbol = {
        .length = 26,
        .bytes = "failed to finalize library"
    }
};

static LimeValue gc_relocate(LimeValue value) {
    if (value == NULL) {
        return NULL;
    } else if ((u64) value < (u64) gc_heap_src || (u64) value >= (u64) (gc_heap_src + gc_heap_size)) {
        // only objects that resize inside the 'gc_heap_src' are managed by the garbage collector
        return value;
    } else if (value->location == NULL) {
        const LimeValue copy = (LimeValue) (gc_heap_dst + gc_heap_ptr);
        const u64 bytes = lime_value_byte_size(value);

        gc_heap_ptr += bytes;
        memcpy(copy, value, bytes);

        value->location = copy;
    }

    return value->location;
}

LimeValue lime_collect_garbage(LimeStack stack) {
    // swap heaps
    u8 *const swap = gc_heap_dst;
    gc_heap_dst = gc_heap_src;
    gc_heap_src = swap;
    gc_heap_ptr = 0;

    // mark the root objects
    while (stack != NULL) {
        for (u64 i = 0; i < sizeof(stack->registers) / sizeof(stack->registers[0]); ++i) {
            stack->registers[i] = gc_relocate(stack->registers[i]);
        }

        *stack->dictionary = gc_relocate(*stack->dictionary);
        *stack->callstack = gc_relocate(*stack->callstack);
        *stack->datastack = gc_relocate(*stack->datastack);

        stack = stack->previous;
    }

    // relocate all objects which are reachable from the root objects
    u64 scan = 0;
    while (scan < gc_heap_ptr) {
        const LimeValue value = (LimeValue) (gc_heap_dst + scan);
        const u64 bytes = lime_value_byte_size(value);

        switch (value->type) {
            case LimeNativeValue:
                value->function.library = gc_relocate(value->function.library);
                break;
            case LimeListValue:
                value->list.head = gc_relocate(value->list.head);
                value->list.tail = gc_relocate(value->list.tail);
                break;
            case LimeMapValue:
                for (u64 i = 0, end = value->map.capacity; i < end; ++i) {
                    value->map.buckets[i] = gc_relocate(value->map.buckets[i]);
                }
                break;
            default:
                // not a compound type
                break;
        }

        scan += bytes;
    }

    // finalize all libraries which are no longer needed
    bool failed_to_finalize = false;
    u64 new_libraries_size = 0;
    u64 previous_libraries_size = 0;
    for (u64 i = 0; i < gc_libraries_size; ++i) {
        const LimeValue value = gc_libraries[i];

        if ((u64) value >= (u64) gc_heap_src && (u64) value < (u64) (gc_heap_src + gc_heap_size)) {
            // value is not 'NULL' and thus marks the new end of the list
            previous_libraries_size = new_libraries_size;
            new_libraries_size = i + 1;

            // TODO: 
            // - call 'lime_module_finalize'
            // - remember that it was already called
            // - if the dlclose fails, don't call 'lime_module_finalize' again for this module

            // this value resides inside the dirty heap
            if (value->location == NULL) {
                // this value was not relocated by the garbage collector and thus must be garbage
                #if defined(OS_UNIX)
                if (dlclose(value->library.handle) != 0) {
                #elif defined(OS_WINDOWS)
                if (FreeLibrary(value->library.handle) == 0) {
                #else
                if (false) {
                #endif
                    failed_to_finalize = true;
                } else {
                    // delete the weak reference
                    gc_libraries[i] = NULL;
                    new_libraries_size = previous_libraries_size;
                }
            } else {
                // remember the new location since this library is still in use
                gc_libraries[i] = value->location;
            }
        }

        // swap finalized values to pack the list of libraries over time
        if (gc_libraries[i] == NULL && i + 1 < gc_libraries_size && gc_libraries[i + 1] != NULL) {
            gc_libraries[i] = gc_libraries[i + 1];
            gc_libraries[i + 1] = NULL;
            i -= 1;
        }
    }

    // remember the new size to shorten future executions of the loop above
    gc_libraries_size = new_libraries_size;

    if (failed_to_finalize) {
        return &gc_library_finalize_exception;
    } else {
        return NULL;
    }
}

static bool gc_heap_reallocate(u64 new_heap_size) {
    if (gc_heap_src == NULL) {
        gc_heap_src = malloc(new_heap_size * sizeof(u8));
        return gc_heap_src != NULL;
    } else {
        u8 *const new_heap_src = realloc(gc_heap_src, new_heap_size * sizeof(u8));
        if (new_heap_src == NULL) {
            // if 'new_heap_src' is 'NULL' the reallocation failed, but the old heap is still intact
            return false;
        } else {
            gc_heap_src = new_heap_src;
            return true;
        }
    }
}

static LimeValue gc_heap_resize(LimeStack stack, const u64 new_heap_size) {
    // as we cannot simply resize both heaps at once we have to resize the 'gc_heap_src' 
    // which is unused at this point
    if (!gc_heap_reallocate(new_heap_size)) {
        return &gc_heap_exception;
    }
    
    // copy all objects from 'gc_heap_dst' to the new 'gc_heap_src'
    const LimeValue exception = lime_collect_garbage(stack);
    if (exception != NULL) {
        return exception;
    }
    
    // since the two heaps were swapped by the previous call we can resize 'gc_heap_src' again
    if (!gc_heap_reallocate(new_heap_size)) {
        return &gc_heap_exception;
    }

    // it is important to set the 'gc_heap_size' not until both heaps were resized, because it
    // is possible for the second resize to fail in which case both heaps are unevenly large
    gc_heap_size = new_heap_size;

    return NULL;
}

LimeResult gc_allocate(LimeStack stack, LimeValueType type, u64 additional) {
    LimeResult result = {
        .value = NULL,
        .exception = NULL
    };

    const u64 bytes = sizeof(struct lime_value) + additional;

    if (gc_heap_ptr + bytes > gc_heap_size) {
        // try to collect garbage
        result.exception = lime_collect_garbage(stack);
        if (result.exception != NULL) {
            return result;
        }

        // resize the heaps if there is still not enough memory available
        if (gc_heap_ptr + bytes > gc_heap_size) {
            // the new heap size is either twice as large or at least as large to contain the requested object
            const u64 minimum_heap_size = gc_heap_size + (gc_heap_ptr + bytes - gc_heap_size);
            const u64 new_heap_size = MAX(gc_heap_size * 2, minimum_heap_size);
            
            // try to resize the heap to the "best" heap size
            result.exception = gc_heap_resize(stack, new_heap_size);
            if (result.exception == &gc_heap_exception && minimum_heap_size < new_heap_size) {
                // try to resize the heap to the minimum if it is truly smaller than the "best" heap size
                result.exception = gc_heap_resize(stack, minimum_heap_size);
                if (result.exception != NULL) {
                    return result;
                }
            } else if (result.exception != NULL) {
                return result;
            }
        }
    }

    result.value = (LimeValue) (gc_heap_dst + gc_heap_ptr);
    gc_heap_ptr += bytes;

    result.value->type = type;
    result.value->location = NULL;
    result.value->hash = 0;

    return result;
}

LimeResult gc_add_library(LimeValue library) {
    LimeResult result = {
        .failure = false,
        .value = library
    };

    if (gc_libraries_size >= gc_libraries_capacity) {
        gc_libraries_capacity = MAX(gc_libraries_capacity * 2, 16);

        if (gc_libraries == NULL) {
            gc_libraries = malloc(gc_libraries_capacity * sizeof(LimeValue));
            if (gc_libraries == NULL) {
                result.failure = true;
                result.exception = &gc_heap_exception;
                return result;
            }
        } else {
            LimeValue *const new_libraries = realloc(gc_libraries, gc_libraries_capacity * sizeof(LimeValue));
            if (new_libraries == NULL) {
                result.failure = true;
                result.exception = &gc_heap_exception;
                return result;
            } else {
                gc_libraries = new_libraries;
            }
        }
    }

    gc_libraries[gc_libraries_size++] = result.value;

    return result;
}
