#ifndef LIME_H
#define LIME_H

#include <stdbool.h>
#include <inttypes.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t  s8;

typedef enum {
    SymbolValue  = 0,
    ListValue    = 1,
    NativeValue  = 2,
    MapValue     = 3,
    BooleanValue = 4,
    NumberValue  = 5,
    StringValue  = 6,
    LibraryValue = 7
} ValueType;

struct value;

typedef struct stack {
    struct stack *previous;
    struct value *registers[3];
    struct value **dictionary;
    struct value **callstack;
    struct value **datastack;
} Stack;

typedef struct value *(*NativeFunction)(Stack *stack);

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
            struct value *library;
            NativeFunction function;
        } native;

        struct {
            #if defined(OS_UNIX)
                void *handle;
            #elif defined(OS_WINDOWS)
                HINSTANCE handle;
            #else
                void *handle;
            #endif
        } library;
    };
} *Value;

extern Value lime_exception(Stack *stack, char *message, ...);

extern char *lime_value_type(Value value);

#endif
