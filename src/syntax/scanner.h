#ifndef SCANNER_H
#define SCANNER_H

#include <bowl/api.h>
#include <bowl/unicode.h>

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
    /** A reference to the underlying source of this scanner (this reference may be managed by the garbage collector). */
    BowlValue *string;
    /** The current offset in the source. */
    u64 offset;
    /** The current line number. */
    u64 line;
    /** The current column number. */
    u64 column;
    bool token_available;
    BowlToken token;
} BowlScanner;

BowlScanner scanner_from(BowlValue *string);

bool scanner_has_next(BowlScanner *scanner);

BowlTokenType scanner_next(BowlScanner *scanner);

#endif