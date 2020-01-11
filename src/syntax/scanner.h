#ifndef SCANNER_H
#define SCANNER_H

#include <lime/api.h>

typedef enum {
    LimeNumberToken,
    LimeStringToken,
    LimeBooleanToken,
    LimeSymbolToken,
    LimeEndOfStreamToken,
    LimeErrorToken
} LimeTokenType;

typedef struct {
    LimeTokenType type;
    u64 line;
    u64 column;
    union {
        struct {
            double value;
        } number;

        struct {
            bool value;
        } boolean;

        struct {
            u64 start;
            u64 length;
        } symbol;

        struct {
            u64 start;
            u64 length;
        } string;

        struct {
            char *message;
        } error;
    };
} LimeToken;

typedef struct {
    LimeValue *string;
    u64 current;
    u64 line;
    u64 column;
    bool initialized;
    LimeToken token;
} LimeScanner;

#define SCANNER_STARTS_WITH_LITERAL(scanner, literal, offset) ((scanner)->current - (offset) == sizeof(literal) - 1 && memcmp((*(scanner)->string)->string.bytes + (offset), (literal), sizeof(literal) - 1) == 0)

LimeScanner scanner_from(LimeValue *string);

bool scanner_has_next(LimeScanner *scanner);

LimeTokenType scanner_next(LimeScanner *scanner);

#endif