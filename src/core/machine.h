#ifndef MACHINE_H
#define MACHINE_H

#include "../common/utility.h"
#include "../interface/lime.h"
#include "../interface/api.h"
#include "value.h"

#define MACHINE_MAP_LOAD_FACTOR ((double) 0.75)
#define MACHINE_PRINT_DATASTACK true

typedef struct {
    LimeValue value;
    LimeValue exception;
} InternalResult;

LimeValue machine_collect_garbage(LimeStack stack);

LimeResult machine_allocate(LimeStack stack, LimeValueType type, u64 additional);

/* ***** internals ***** */



InternalResult machine_map_put(LimeStack stack, LimeValue  map, LimeValue  key, LimeValue  value);

/* ***** instructions ***** */

LimeValue machine_instruction_type(LimeStack stack);

LimeValue machine_instruction_hash(LimeStack stack);

LimeValue machine_instruction_equals(LimeStack stack);

LimeValue machine_instruction_show(LimeStack stack);

LimeValue machine_instruction_throw(LimeStack stack);

LimeValue machine_instruction_length(LimeStack stack);

LimeValue machine_instruction_nil(LimeStack stack);

LimeValue machine_instruction_push(LimeStack stack);

LimeValue machine_instruction_library(LimeStack stack);

LimeValue machine_instruction_native(LimeStack stack);

LimeValue machine_instruction_run(LimeStack stack);

#endif
