#ifndef VALUE_H
#define VALUE_H

#include "../common/common.h"

typedef enum {
    SymbolValue  = 0,
    ListValue    = 1,
    NativeValue  = 2,
    MapValue     = 3,
    BooleanValue = 4,
    NumberValue  = 5,
    StringValue  = 6
} ValueType;

struct stack;
struct value;

typedef struct value *(*NativeFunction)(struct stack *stack);

typedef struct value {
    ValueType type;
    struct value *location;
    u64 hash;

    union {
        struct {
            u64 length;
            u8  bytes[];
        } symbol;

        struct {
            double value;
        } number;

        struct {
            bool value;
        } boolean;

        struct {
            u64 length;
            u8  bytes[];
        } string;

        struct {
            u64 length;
            struct value *head;
            struct value *tail;
        } list;

        struct {
            u64 length;
            u64 capacity;
            struct value *buckets[];
        } map;

        struct {
            NativeFunction function;
        } native;
    };
} *Value;

Value value_map_get_or_else(Value map, Value key, Value otherwise);

bool value_map_subset_of(Value superset, Value subset);

u64 value_hash(Value value);

bool value_equals(Value a, Value b);

u64 value_byte_size(Value value);

void value_dump(FILE *stream, Value value);

void value_show(Value value, char **buffer, u64 *length);

u64 value_length(Value value);

char *value_type(Value value);

#endif
