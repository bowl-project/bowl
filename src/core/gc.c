#include "gc.h"

// memory heaps
static u8 *gc_heap_dst = NULL;
static u8 *gc_heap_src = NULL;
static u64 gc_heap_ptr = 0;
static u64 gc_heap_size = 0;

static GarbageCollectorLibraryEntry *gc_libraries = NULL;
static u64 gc_libraries_capacity = 0;
static u64 gc_libraries_length = 0;

static inline bool gc_is_managed(LimeValue value) {
    // only objects that resize inside the 'gc_heap_src' are managed by the garbage collector
    return (u64) value >= (u64) gc_heap_src && (u64) value < (u64) (gc_heap_src + gc_heap_size);
}

static void gc_remove_library(u64 index) {
    free(gc_libraries[index].libraries);
    memmove(&gc_libraries[index], &gc_libraries[index + 1], (gc_libraries_length - (index + 1)) * sizeof(GarbageCollectorLibraryEntry));
    gc_libraries_length -= 1;
}

static void gc_remove_library_from_entry(GarbageCollectorLibraryEntry *entry, u64 index) {
    memmove(&entry->libraries[index], &entry->libraries[index + 1], (entry->length - (index + 1)) * sizeof(LimeValue));
    entry->length -= 1;
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

    // clean up all libraries which are no longer needed
    LimeResult result = {
        .failure = false,
        .value = NULL
    };

    for (u64 i = 0; i < gc_libraries_length; ++i) {
        GarbageCollectorLibraryEntry *const entry = &gc_libraries[i];
   
        for (u64 j = 0; j < entry->length; ++j) {
            const LimeValue library = entry->libraries[j];

            if (gc_is_managed(library)) {
                if (library->location == NULL) {
                    // this library value is marked as dirty and will be cleaned up
                    
                    gc_remove_library_from_entry(entry, j);
                    j -= 1;

                    if (entry->length == 0) {
                        // this was the last library value, i.e. finalize and close the library
                        #if defined(OS_UNIX)
                            LimeValue (*const finalize)(LimeStack, LimeValue) = (LimeValue (*)(LimeStack, LimeValue)) dlsym(library->library.handle, "lime_module_finalize");
                            void *handle;
                        #elif defined(OS_WINDOWS)
                            LimeValue (*const finalize)(LimeStack, LimeValue) = (LimeValue (*)(LimeStack, LimeValue)) GetProcAddress(library->library.handle, "lime_module_finalize");
                            HINSTANCE handle;
                        #else 
                            LimeValue (*const finalize)(LimeStack, LimeValue) = NULL;
                            void *handle;
                        #endif
                    
                        // remember the handle before calling userland
                        handle = library->library.handle;

                        // call finalize if possible
                        if (finalize == NULL) {
                            result.failure = true;
                            result.exception = lime_exception_finalization_failure;
                        } else {
                            result.exception = finalize(stack, library);
                            if (result.exception != NULL) {
                                result.failure = true;
                            }
                        }

                        // close the handle
                        #if defined(OS_UNIX)
                        if (dlclose(handle) != 0 && !result.failure) {
                        #elif defined(OS_WINDOWS)
                        if (FreeLibrary(handle) == 0 && !result.failure) {
                        #else
                        if (!result.failure) {
                        #endif
                            result.failure = true;
                            result.exception = lime_exception_finalization_failure;
                        }

                        gc_remove_library(i);
                        i -= 1;

                        if (result.failure) {
                            return result.exception;
                        }
                    }
                } else if (library != NULL) {
                    entry->libraries[j] = library->location;
                }
            }
        }
    }

    return NULL;
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

static LimeValue gc_add_library_to_entry(GarbageCollectorLibraryEntry *entry, LimeValue library) {
    if (entry->length >= entry->capacity) {
        const u64 capacity = MAX(entry->capacity * 2, 16);
        LimeValue *libraries = realloc(entry->libraries, capacity * sizeof(LimeValue));

        if (libraries == NULL) {
            return lime_exception_out_of_heap;
        }

        entry->capacity = capacity;
        entry->libraries = libraries;
    }

    entry->libraries[entry->length++] = library;
    return NULL;
}

LimeResult gc_add_library(LimeValue library) {
    LimeResult result = {
        .failure = false,
        .value = library
    };

    GarbageCollectorLibraryEntry *entry = NULL;

    // check if the library was already added
    for (u64 i = 0; i < gc_libraries_length; ++i) {
        GarbageCollectorLibraryEntry *const current = &gc_libraries[i];

        if (current->length > 0 && current->libraries[0]->library.handle == library->library.handle) {
            entry = current;
            break;
        }
    }

    if (entry == NULL) {
        if (gc_libraries_length >= gc_libraries_capacity) {
            const u64 capacity = MAX(gc_libraries_capacity * 2, 16);
            GarbageCollectorLibraryEntry *const libraries = realloc(gc_libraries, capacity * sizeof(GarbageCollectorLibraryEntry));

            if (libraries == NULL) {
                result.failure = true;
                result.exception = lime_exception_out_of_heap;
                return result;
            }

            gc_libraries = libraries;
            gc_libraries_capacity = capacity;
        }

        entry = &gc_libraries[gc_libraries_length++];
        entry->capacity = 0;
        entry->length = 0;
        entry->libraries = NULL;
    }

    const LimeValue exception = gc_add_library_to_entry(entry, library);

    if (exception != NULL) {
        result.failure = true;
        result.exception = exception;
    }

    return result;
}
