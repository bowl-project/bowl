#ifndef GC_H
#define GC_H

#include "../common/utility.h"
#include <bowl/bowl.h>
#include <bowl/api.h>

#include "library.h"

BowlResult gc_allocate(BowlStack stack, BowlValueType type, u64 additional);

BowlResult gc_add_library(BowlStack stack, BowlValue library);

#endif