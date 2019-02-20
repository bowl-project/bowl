#ifndef MACHINE_COMMON_H
#define MACHINE_COMMON_H

#include "../common/utility.h"

/* value structure */

typedef enum {
    SymbolValue,
    ListValue,
    MapValue,
    NativeValue,
    StringValue,
    NumberValue
} ValueType;

typedef struct value {
    ValueType type;
    struct value *location;
    word hash;
    union {
        struct {
            word length;
            char value[];
        } symbol;

        struct {
            double value;
        } number;

        struct {
            // TODO: cache nul-terminated string
            word length;
            char value[];
        } string;

        struct {
            word length;
            struct value *head;
            struct value *tail;
        } list;

        struct {
            word length;
            word bucket_count;
            struct value *buckets[];
        } map;

        struct {
            word length;
            void (*function)(void *);
            char value[];
        } native;
    };
} *Value;

/* machine structure */

#define MACHINE_REGISTER_COUNT 32

typedef struct machine {
    Value registers[MACHINE_REGISTER_COUNT];
    Value dictionary;
    Value callstack;
    Value datastack;
} *Machine;

#define $0 registers[0]
#define $1 registers[1]
#define $2 registers[2]
#define $3 registers[3]
#define $4 registers[4]
#define $5 registers[5]
#define $6 registers[6]
#define $7 registers[7]
#define $8 registers[8]
#define $9 registers[9]
#define $10 registers[10]
#define $11 registers[11]
#define $12 registers[12]
#define $13 registers[13]
#define $14 registers[14]
#define $15 registers[15]
#define $16 registers[16]
#define $17 registers[17]
#define $18 registers[18]
#define $19 registers[19]
#define $20 registers[20]
#define $21 registers[21]
#define $22 registers[22]
#define $23 registers[23]
#define $24 registers[24]
#define $25 registers[25]
#define $26 registers[26]
#define $27 registers[27]
#define $28 registers[28]
#define $29 registers[29]
#define $30 registers[30]
#define $31 registers[31]

typedef void (*NativeFunction)(void *);

#endif
