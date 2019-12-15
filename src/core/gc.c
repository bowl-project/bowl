#include "gc.h"

// memory heaps
static u8 *gc_heap_dst = NULL;
static u8 *gc_heap_src = NULL;
static u64 gc_heap_ptr = 0;
static u64 gc_heap_size = 0;

// a list of all libraries which are currently loaded
static LimeValue *gc_libraries = NULL;
static u64 gc_libraries_capacity = 0;
static u64 gc_libraries_size = 0;

static inline bool gc_is_managed(LimeValue value) {
    // only objects that resize inside the 'gc_heap_src' are managed by the garbage collector
    return (u64) value >= (u64) gc_heap_src && (u64) value < (u64) (gc_heap_src + gc_heap_size);
}

static LimeValue gc_add_library_to_list(LimeValue library) {
    if (gc_libraries_size >= gc_libraries_capacity) {
        const u64 capacity = MAX(gc_libraries_capacity * 2, 16);
        
        LimeValue *const new_libraries = realloc(gc_libraries, sizeof(LimeValue) * capacity);
        if (new_libraries == NULL) {
            return lime_exception_out_of_heap;
        } else {
            gc_libraries = new_libraries;
            gc_libraries_capacity = capacity;
        }
    }

    gc_libraries[gc_libraries_size++] = library;

    return NULL;
}

static void gc_remove_library_from_list(u64 index) {
    memmove(&gc_libraries[index], &gc_libraries[index + 1], (gc_libraries_size - (index + 1)) * sizeof(LimeValue));
    --gc_libraries_size;
}

static LimeValue gc_relocate(LimeValue value) {
    if (value == NULL) {
        return NULL;
    } else if (!gc_is_managed(value)) {
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
    register LimeStack current = stack;
    while (current != NULL) {
        for (u64 i = 0; i < sizeof(current->registers) / sizeof(current->registers[0]); ++i) {
            current->registers[i] = gc_relocate(current->registers[i]);
        }

        if (current->dictionary != NULL) {
            *current->dictionary = gc_relocate(*current->dictionary);
        }

        if (current->callstack != NULL) {
            *current->callstack = gc_relocate(*current->callstack);
        }

        if (current->datastack != NULL) {
            *current->datastack = gc_relocate(*current->datastack);
        }

        current = current->previous;
    }

    // relocate all objects which are reachable from the root objects
    register u64 scan = 0;
    while (scan < gc_heap_ptr) {
        const LimeValue value = (LimeValue) (gc_heap_dst + scan);
        const u64 bytes = lime_value_byte_size(value);
        scan += bytes;

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
    }

    // update all weak references
    for (u64 i = 0; i < gc_libraries_size; ++i) {
        const LimeValue library = gc_libraries[i];

        if (library->location != NULL) {
            gc_libraries[i] = library->location;
        }
    }

    // clean up all libraries which are no longer needed
    LimeLibraryResult result = {
        .failure = false,
        .exception = NULL
    };

    for (u64 i = 0; i < gc_libraries_size; ++i) {
        const LimeValue library = gc_libraries[i];

        if (!gc_is_managed(library)) {
            continue;
        }

        if (!result.failure) {
            gc_remove_library_from_list(i--);
            result = library_close(stack, library);
        } else {
            // relocate rest of the libraries to defer their finalization
            gc_libraries[i] = gc_relocate(library);
        }
    }

    if (result.failure) {
        return result.exception;
    } else {
        return NULL;
    }
}

static bool gc_heap_reallocate(u64 new_heap_size) {
    u8 *const new_heap_src = realloc(gc_heap_src, new_heap_size * sizeof(u8));
    if (new_heap_src == NULL) {
        // if 'new_heap_src' is 'NULL' the reallocation failed, but the old heap is still intact
        return false;
    } else {
        gc_heap_src = new_heap_src;
        return true;
    }
}

static LimeValue gc_heap_resize(LimeStack stack, const u64 new_heap_size) {
    // as we cannot simply resize both heaps at once we have to resize the 'gc_heap_src' 
    // which is unused at this point
    if (!gc_heap_reallocate(new_heap_size)) {
        return lime_exception_out_of_heap;
    }
    
    // copy all objects from 'gc_heap_dst' to the new 'gc_heap_src'
    const LimeValue exception = lime_collect_garbage(stack);
    if (exception != NULL) {
        return exception;
    }
    
    // since the two heaps were swapped by the previous call we can resize 'gc_heap_src' again
    if (!gc_heap_reallocate(new_heap_size)) {
        return lime_exception_out_of_heap;
    }

    // it is important to set the 'gc_heap_size' not until both heaps were resized, because it
    // is possible for the second resize to fail in which case both heaps are unevenly large
    gc_heap_size = new_heap_size;

    return NULL;
}

LimeResult gc_allocate(LimeStack stack, LimeValueType type, u64 additional) {
    LimeResult result = {
        .failure = false,
        .value = NULL
    };

    const u64 bytes = sizeof(struct lime_value) + additional;

    if (gc_heap_ptr + bytes > gc_heap_size) {
        // try to collect garbage
        result.exception = lime_collect_garbage(stack);
        if (result.exception != NULL) {
            result.failure = true;
            return result;
        }

        // resize the heaps if there is still not enough memory available
        if (gc_heap_ptr + bytes > gc_heap_size) {
            // the new heap size is either twice as large or at least as large to contain the requested object
            const u64 minimum_heap_size = gc_heap_size + (gc_heap_ptr + bytes - gc_heap_size);
            const u64 new_heap_size = MAX(gc_heap_size * 2, minimum_heap_size);
            
            // try to resize the heap to the "best" heap size
            result.exception = gc_heap_resize(stack, new_heap_size);
            if (result.exception == lime_exception_out_of_heap && minimum_heap_size < new_heap_size) {
                // try to resize the heap to the minimum if it is truly smaller than the "best" heap size
                result.exception = gc_heap_resize(stack, minimum_heap_size);
                if (result.exception != NULL) {
                    result.failure = true;
                    return result;
                }
            } else if (result.exception != NULL) {
                result.failure = true;
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

LimeResult gc_add_library(LimeStack stack, LimeValue library) {
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);
    LimeResult result = {
        .failure = false,
        .value = NULL
    };

    LimeLibraryResult library_result = library_open(&frame, frame.registers[0]);

    if (library_result.failure) {
        result.failure = true;
        result.exception = library_result.exception;
        return result;
    }

    result.exception = gc_add_library_to_list(frame.registers[0]);

    if (result.exception != NULL) {
        result.failure = true;
    } else {
        result.value = frame.registers[0];
    }

    return result;
}
