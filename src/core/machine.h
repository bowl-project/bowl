#ifndef MACHINE_H
#define MACHINE_H

#include "../common/common.h"
#include "value.h"

#define MACHINE_MAP_LOAD_FACTOR ((double) 0.75)
#define MACHINE_PRINT_DATASTACK true

typedef struct stack {
    struct stack *previous;
    Value registers[3];
    Value *dictionary;
    Value *callstack;
    Value *datastack;
} Stack;

#define MACHINE_ALLOCATE(stack, a, b, c) {\
    .previous = (stack),\
    .registers = { (a), (b), (c) },\
    .dictionary = (stack)->dictionary,\
    .callstack = (stack)->callstack,\
    .datastack = (stack)->datastack\
}

typedef struct {
    Value value;
    Value exception;
} InternalResult;

Value machine_collect_garbage(Stack *stack);

InternalResult machine_allocate(Stack *stack, ValueType type, u64 additional);

/* ***** internals ***** */

Value machine_exception(Stack *stack, char *message, ...);

InternalResult machine_symbol(Stack *stack, u8 *bytes, u64 length);

InternalResult machine_string(Stack *stack, u8 *bytes, u64 length);

InternalResult machine_native(Stack *stack, Value library, NativeFunction function);

InternalResult machine_list(Stack *stack, Value head, Value tail);

InternalResult machine_map(Stack *stack, u64 bucket_count);

InternalResult machine_number(Stack *stack, double value);

InternalResult machine_library(Stack *stack, char *path);

InternalResult machine_boolean(Stack *stack, bool value);

InternalResult machine_map_put(Stack *stack, Value map, Value key, Value value);

/* ***** instructions ***** */

Value machine_instruction_type(Stack *stack);

Value machine_instruction_hash(Stack *stack);

Value machine_instruction_equals(Stack *stack);

Value machine_instruction_show(Stack *stack);

Value machine_instruction_throw(Stack *stack);

Value machine_instruction_length(Stack *stack);

Value machine_instruction_nil(Stack *stack);

Value machine_instruction_push(Stack *stack);

Value machine_instruction_library(Stack *stack);

Value machine_instruction_native(Stack *stack);

Value machine_instruction_run(Stack *stack);

/* ***** foreign function interface ***** */

Value lime_exception(Stack *stack, char *message, ...);

char *lime_value_type(Value value);

#endif
