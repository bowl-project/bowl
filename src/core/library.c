#include "library.h"

static LimeLibraryMap *library_cache = NULL;

static LimeLibraryResult library_initialize_cache(void) {
    LimeLibraryResult result = {
        .failure = false,
        .handle = NULL
    };

    if (library_cache == NULL) {
        const u64 capacity = 16;
        library_cache = malloc(sizeof(LimeLibraryMap) + capacity * sizeof(LimeLibraryMapBucket));

        if (library_cache == NULL) {
            result.failure = true;
            result.exception = lime_exception_out_of_heap;
        } else {
            library_cache->capacity = capacity;
            library_cache->length = 0;
            for (u64 i = 0; i < capacity; ++i) {
                library_cache->buckets[i].length = 0;
                library_cache->buckets[i].capacity = 0;
                library_cache->buckets[i].entries = NULL;
            }
        }
    }

    return result;
}

static inline void library_remove_entry(LimeLibraryMapBucket *bucket, u64 index) {
    free(bucket->entries[index].key.bytes);
    memmove(&bucket->entries[index], &bucket->entries[index + 1], (bucket->length - (index + 1)) * sizeof(LimeLibraryMapEntry));
    --bucket->length;
}

static inline u64 library_hash(u8 *bytes, u64 length) {
    register u64 hash = 31;

    for (u64 i = 0; i < length; ++i) {
        hash += pow(bytes[i] * 31, length - (i + 1));
    }

    return hash;
}

static inline u64 library_equals(u8 *a, u64 a_length, u8 *b, u64 b_length) {
    return a_length == b_length && memcmp(a, b, a_length) == 0;
}

static LimeValue library_enlarge_bucket(LimeLibraryMapBucket *bucket) {
    // resize the bucket if it is not large enough
    if (bucket->length >= bucket->capacity) {
        const u64 new_capacity = MAX(bucket->capacity * 2, bucket->length + 1);
        LimeLibraryMapEntry *const new_entries = realloc(bucket->entries, sizeof(LimeLibraryMapEntry) * new_capacity);
        
        if (new_entries == NULL) {
            return lime_exception_out_of_heap;
        }

        bucket->entries = new_entries;
        bucket->capacity = new_capacity;
    }

    return NULL;
}

static LimeValue library_enlarge_cache(void) {
    const u64 new_capacity = MAX(library_cache->capacity * 2, 16);
    LimeLibraryMap *const new_cache = realloc(library_cache, sizeof(LimeLibraryMap) + new_capacity * sizeof(LimeLibraryMapBucket));

    if (new_cache == NULL) {
        return lime_exception_out_of_heap;
    }

    library_cache = new_cache;

    for (u64 i = 0; i < library_cache->capacity; ++i) {
        LimeLibraryMapBucket *bucket = &library_cache->buckets[i];

        for (u64 j = 0; j < bucket->length; ++j) {
            LimeLibraryMapEntry *entry = &bucket->entries[j];
            const u64 hash = library_hash(entry->key.bytes, entry->key.length);
            const u64 new_index = hash % new_capacity;
            
            if (new_index != i) {
                LimeLibraryMapBucket *const destination = &library_cache->buckets[new_index];
                const LimeValue exception = library_enlarge_bucket(destination);

                if (exception != NULL) {
                    return exception;
                }

                memcpy(&destination->entries[destination->length], entry, sizeof(LimeLibraryMapEntry));
                ++destination->length;

                // prevent the path from deletion
                entry->key.bytes = NULL;
                library_remove_entry(bucket, j);
                --j;
            }
        }
    }

    library_cache->capacity = new_capacity;
    return NULL;
}

static inline LimeLibraryMapBucket *library_get_bucket(LimeValue library) {
    return &library_cache->buckets[library_hash(library->library.bytes, library->library.length) % library_cache->capacity];
}

static u64 library_get_entry_index(LimeLibraryMapBucket *bucket, LimeValue library) {
    for (u64 i = 0; i < bucket->length; ++i) {
        LimeLibraryMapEntry *entry = &bucket->entries[i];

        if (library_equals(library->library.bytes, library->library.length, entry->key.bytes, entry->key.length)) {
            return i;
        }
    }

    return (u64) -1;
}

static LimeLibraryMapEntry *library_get_entry(LimeLibraryMapBucket *bucket, LimeValue library)  {
    const u64 index = library_get_entry_index(bucket, library);
    if (index == (u64) -1) {
        return NULL;
    } else {
        return &bucket->entries[index];
    }
}

static void library_dump(void) {

    printf("library cache := {\n");
    
    bool first = true;
    for (u64 i = 0; i < library_cache->capacity; ++i) {
        LimeLibraryMapBucket *bucket = &library_cache->buckets[i];

        for (u64 j = 0; j < bucket->length; ++j) {
            LimeLibraryMapEntry *entry = &bucket->entries[j];

            if (first) {
                first = false;
            } else {
                printf(",\n");
            }

            printf("  '%s' -> 0x%08" PRIX64 " (references: %" PRId64 ")", entry->key.bytes, (u64) entry->value.handle, entry->value.references);
        }
    }

    if (!first) {
        printf("\n");
    }

    printf("}\n");
    fflush(stdout);

}

LimeLibraryResult library_open(LimeStack stack, LimeValue library) {
    static const double load_factor = 0.75;

    LimeLibraryResult result = library_initialize_cache();

    if (result.failure) {
        return result;
    }

    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);
    LimeValue exception;

    frame.registers[0]->library.handle = NULL;

    const u64 length = frame.registers[0]->library.length;
    LimeLibraryMapBucket *bucket = library_get_bucket(frame.registers[0]);
    LimeLibraryMapEntry *entry = library_get_entry(bucket, frame.registers[0]);

    if (entry != NULL) {
        result.handle = entry->value.handle;
        ++entry->value.references;
        return result;
    }

    // resize the hash map if it exceeds its load factor
    if (library_cache->length + 1 >= library_cache->capacity * load_factor) {
        library_enlarge_cache();
        // the bucket may have changed => update it
        bucket = library_get_bucket(frame.registers[0]);
    }

    exception = library_enlarge_bucket(bucket);
    if (exception != NULL) {
        result.failure = true;
        result.exception = exception;
        return result;
    }

    // create a null-terminated string from the byte array
    u8 *path = malloc(sizeof(u8) * (length + 1));

    if (path == NULL) {
        result.failure = true;
        result.exception = lime_exception_out_of_heap;
        return result;
    }

    memcpy(path, frame.registers[0]->library.bytes, length);
    path[length] = '\0';

    // open the library handle
    #if defined(OS_UNIX)
        // check if the library is already loaded
        LimeLibraryHandle handle = dlopen((char *) path, RTLD_LAZY);
    #elif defined(OS_WINDOWS)
        LimeLibraryHandle handle = LoadLibrary((char *) path);
    #else
        // setting the handle to 'NULL' here triggers the error in the next line, which is 
        // desired because native libraries are not supported on this platform.
        handle = NULL;
    #endif

    if (handle == NULL) {
        result.exception = lime_exception(&frame, "failed to load library '%s'", (char *) path);
        result.failure = true;
        free(path);
        return result;
    }

    entry = &bucket->entries[bucket->length];
    entry->key.length = length;
    entry->key.bytes = path;
    entry->value.handle = handle;
    entry->value.references = 1;

    bucket->length++;

    frame.registers[0]->library.handle = handle;

    #if defined(OS_UNIX)
        LimeModuleFunction initialize = (LimeModuleFunction) dlsym(handle, "lime_module_initialize");
    #elif defined(OS_WINDOWS)
        LimeModuleFunction initialize = (LimeModuleFunction) GetProcAddress(handle, "lime_module_initialize");
    #else
        LimeModuleFunction initialize = NULL;
    #endif

    if (initialize == NULL) {
        result.exception = lime_exception(&frame, "failed to load library '%s'", path);
        result.failure = true;
        return result;
    }
    
    exception = initialize(&frame, frame.registers[0]);

    if (exception != NULL) {
        result.exception = exception;
        result.failure = true;
    } else {
        result.handle = handle;
        result.failure = false;
    }

    return result;
}

LimeLibraryResult library_close(LimeStack stack, LimeValue library) {
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);
    LimeLibraryResult result = library_initialize_cache();

    if (result.failure) {
        return result;
    }

    const u64 length = frame.registers[0]->library.length;
    LimeLibraryMapBucket *bucket = library_get_bucket(frame.registers[0]);
    const u64 index = library_get_entry_index(bucket, frame.registers[0]);

    if (index == (u64) -1) {
        result.exception = lime_exception_finalization_failure;
        result.failure = true;
        return result;
    } else {
        LimeLibraryMapEntry *entry = &bucket->entries[index];

        if (--entry->value.references <= 0) {
            LimeLibraryHandle handle = entry->value.handle;
            
            // overwrite the handle of the library to prevent it from future use
            frame.registers[0]->library.handle = NULL;

            // remove the entry before calling finalize to prevent duplicate removals
            library_remove_entry(bucket, index);

            #if defined(OS_UNIX)
                LimeModuleFunction finalize = (LimeModuleFunction) dlsym(handle, "lime_module_finalize");
            #elif defined(OS_WINDOWS)
                LimeModuleFunction finalize = (LimeModuleFunction) GetProcAddress(handle, "lime_module_finalize");
            #else
                LimeModuleFunction finalize = NULL;
            #endif

            if (finalize == NULL) {
                result.exception = lime_exception_finalization_failure;
                result.failure = true;
            } else {
                const LimeValue exception = finalize(&frame, frame.registers[0]);

                if (exception != NULL) {
                    result.exception = exception;
                    result.failure = true;
                }
            }

            // close the native library handle
            #if defined(OS_UNIX)
                if (dlclose(handle) != 0 && !result.failure) {
                    result.exception = lime_exception_finalization_failure;
                    result.failure = true;
                }
            #elif defined(OS_WINDOWS)
                if (FreeLibrary(handle) == 0 && !result.failure) {
                    result.exception = lime_exception_finalization_failure;
                    result.failure = true;
                }
            #endif

            if (!result.failure) {
                result.handle = handle;
            }

            return result;
        } else {
            result.failure = false;
            result.handle = entry->value.handle;
            return result;
        }
    }
}
