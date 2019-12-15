#ifndef LIBRARY_H
#define LIBRARY_H

#include "../common/utility.h"
#include <lime/lime.h>
#include <lime/api.h>

typedef struct {
    u64 length;
    u8 *bytes;
} LimeLibraryMapKey;

typedef struct {
    u64 references;
    LimeLibraryHandle handle;
} LimeLibraryMapValue;

typedef struct {
    LimeLibraryMapKey key;
    LimeLibraryMapValue value;
} LimeLibraryMapEntry;

typedef struct {
    u64 length;
    u64 capacity;
    LimeLibraryMapEntry *entries;
} LimeLibraryMapBucket;

typedef struct {
    u64 length;
    u64 capacity;
    LimeLibraryMapBucket buckets[];
} LimeLibraryMap;

typedef struct {
    bool failure;
    union {
        LimeValue exception;
        LimeLibraryHandle handle;
    };
} LimeLibraryResult;

/**
 * Opens the native library handle which is specified by the provided path.
 * 
 * If this library is already loaded an internal reference counter is incremented and
 * the existing handle is returned instead.
 * 
 * The 'lime_module_initialize' function is called accordingly when the native library
 * is loaded for the first time.
 * @param stack The stack of the current environment.
 * @param library The library value which should be initialized with the appropriate 
 * handle. The path of the library must be already initialized.
 * @return Either the opened native handle or an exception.
 */
LimeLibraryResult library_open(LimeStack stack, LimeValue library);

/** 
 * Closes the native library handle which is specified by the provided path.
 * 
 * If there are no other references to this native library handle it is closed and
 * and the 'lime_module_finalize' function is called accordingly.
 * @param stack The stack of the current environment.
 * @param library The library value which should be finalized.
 * @return Either the previous native handle or an exception. The handle may be 
 * invalid if there is no other reference to it.
 */
LimeLibraryResult library_close(LimeStack stack, LimeValue library);

#endif