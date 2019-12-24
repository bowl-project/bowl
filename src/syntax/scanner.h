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
    u8 *bytes;
    u64 length;
    u64 current;
    u64 line;
    u64 column;
    bool initialized;
    LimeToken token;
} LimeScanner;

LimeScanner scanner_from(u8 *bytes, u64 length);

bool scanner_has_next(LimeScanner *scanner);

LimeTokenType scanner_next(LimeScanner *scanner);

#endif