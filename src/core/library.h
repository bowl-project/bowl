#ifndef LIBRARY_H
#define LIBRARY_H

#include "../common/utility.h"
#include <bowl/bowl.h>
#include <bowl/api.h>

typedef struct {
    u64 length;
    u8 *bytes;
} BowlLibraryMapKey;

typedef struct {
    u64 references;
    BowlLibraryHandle handle;
} BowlLibraryMapValue;

typedef struct {
    BowlLibraryMapKey key;
    BowlLibraryMapValue value;
} BowlLibraryMapEntry;

typedef struct {
    u64 length;
    u64 capacity;
    BowlLibraryMapEntry *entries;
} BowlLibraryMapBucket;

typedef struct {
    u64 length;
    u64 capacity;
    BowlLibraryMapBucket buckets[];
} BowlLibraryMap;

typedef struct {
    bool failure;
    union {
        BowlValue exception;
        BowlLibraryHandle handle;
    };
} BowlLibraryResult;

/**
 * Opens the native library handle which is specified by the provided path.
 * 
 * If this library is already loaded an internal reference counter is incremented and
 * the existing handle is returned instead.
 * 
 * The 'bowl_module_initialize' function is called accordingly when the native library
 * is loaded for the first time.
 * @param stack The stack of the current environment.
 * @param library The library value which should be initialized with the appropriate 
 * handle. The path of the library must be already initialized.
 * @return Either the opened native handle or an exception.
 */
BowlLibraryResult library_open(BowlStack stack, BowlValue library);

/** 
 * Closes the native library handle which is specified by the provided path.
 * 
 * If there are no other references to this native library handle it is closed and
 * and the 'bowl_module_finalize' function is called accordingly.
 * @param stack The stack of the current environment.
 * @param library The library value which should be finalized.
 * @return Either the previous native handle or an exception. The handle may be 
 * invalid if there is no other reference to it.
 */
BowlLibraryResult library_close(BowlStack stack, BowlValue library);

#endif