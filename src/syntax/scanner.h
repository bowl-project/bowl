#ifndef SCANNER_H
#define SCANNER_H

#include <bowl/api.h>

typedef enum {
    BowlNumberToken,
    BowlStringToken,
    BowlBooleanToken,
    BowlSymbolToken,
    BowlEndOfStreamToken,
    BowlErrorToken
} BowlTokenType;

typedef struct {
    BowlTokenType type;
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
} BowlToken;

typedef struct {
    BowlValue *string;
    u64 current;
    u64 line;
    u64 column;
    bool initialized;
    BowlToken token;
} BowlScanner;

#define SCANNER_STARTS_WITH_LITERAL(scanner, literal, offset) ((scanner)->current - (offset) == sizeof(literal) - 1 && memcmp((*(scanner)->string)->string.bytes + (offset), (literal), sizeof(literal) - 1) == 0)

BowlScanner scanner_from(BowlValue *string);

bool scanner_has_next(BowlScanner *scanner);

BowlTokenType scanner_next(BowlScanner *scanner);

#endif