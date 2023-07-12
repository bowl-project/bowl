#include "scanner.h"

static inline bool scanner_at_end(BowlScanner *scanner) {
    return scanner->offset >= (*scanner->string)->string.length;
}

static inline u32 scanner_current(BowlScanner *scanner) {
    return (*scanner->string)->string.codepoints[scanner->offset];
}

static inline bool scanner_equals(u32 *codepoints, u64 codepoints_length, char *string) {
    u64 index = 0;

    while (*string && index < codepoints_length) {
        if (codepoints[index++] != *string++) {
            return false;
        }
    }

    return index == codepoints_length && !*string;
}

static inline void scanner_advance_offset(BowlScanner *scanner) {
    if (!scanner_at_end(scanner)) {
        if (scanner_current(scanner) == '\n') {
            ++scanner->line;
            scanner->column = 1;
        } else {
            ++scanner->column;
        }
        ++scanner->offset;
    }
} 

static inline void scanner_skip_spaces(BowlScanner *scanner) {
    // in case the end-of-source is hit this loop terminates since the codepoint is set to 0 (which is not a space)
    while (!scanner_at_end(scanner) && unicode_is_space(scanner_current(scanner))) { 
        scanner_advance_offset(scanner);
    }
}

static inline void scanner_advance_symbol(BowlScanner *scanner) {
    scanner->token.type = BowlSymbolToken;
    scanner->token.column = scanner->column;
    scanner->token.line = scanner->line;
    scanner->token.symbol.start = scanner->offset;

    while (!scanner_at_end(scanner) && !unicode_is_space(scanner_current(scanner))) {
        scanner_advance_offset(scanner);
    }

    scanner->token.symbol.length = scanner->offset - scanner->token.symbol.start;
}

static inline void scanner_advance_string(BowlScanner *scanner) {
    scanner->token.type = BowlStringToken;
    scanner->token.column = scanner->column;
    scanner->token.line = scanner->line;

    if (scanner_current(scanner) != '"') {
        scanner->token.type = BowlErrorToken;
        scanner->token.error.message = "illegal start of string literal";
        return;
    }

    scanner_advance_offset(scanner);
    scanner->token.string.start = scanner->offset;
    
    register bool escaped = false;
    while (!scanner_at_end(scanner) && (scanner_current(scanner) != '"' || escaped)) {
        if (escaped) {
            escaped = false;
        } else if (scanner_current(scanner) == '\\') {
            escaped = true;
        }

        scanner_advance_offset(scanner);
    }

    if (scanner_at_end(scanner) || scanner_current(scanner) != '"') {
        scanner->token.type = BowlErrorToken;
        scanner->token.column = scanner->column;
        scanner->token.line = scanner->line;
        scanner->token.error.message = "illegal end of string literal";
        return;
    }

    scanner->token.string.length = scanner->offset - scanner->token.string.start;

    scanner_advance_offset(scanner);
}

static inline void scanner_advance_number(BowlScanner *scanner) {
    scanner->token.type = BowlNumberToken;
    scanner->token.column = scanner->column;
    scanner->token.line = scanner->line;

    bool negative = false;

    if (scanner_current(scanner) == '-') {
        negative = true;
        scanner_advance_offset(scanner);
    } else if (scanner_current(scanner) == '+') {
        scanner_advance_offset(scanner);
    }

    if (scanner_at_end(scanner) || scanner_current(scanner) < '0' || scanner_current(scanner) > '9') {
        scanner->token.type = BowlErrorToken;
        scanner->token.column = scanner->column;
        scanner->token.line = scanner->line;
        scanner->token.error.message = "illegal number literal";
    }

    double result = 0;
    do {
        result *= 10;
        result += scanner_current(scanner) - '0';
        scanner_advance_offset(scanner);
    } while (!scanner_at_end(scanner) && scanner_current(scanner) >= '0' && scanner_current(scanner) <= '9');

    if (!scanner_at_end(scanner) && scanner_current(scanner) == '.') {
        scanner_advance_offset(scanner);

        if (scanner_at_end(scanner) || scanner_current(scanner) < '0' || scanner_current(scanner) > '9') {
            scanner->token.type = BowlErrorToken;
            scanner->token.column = scanner->column;
            scanner->token.line = scanner->line;
            scanner->token.error.message = "illegal number literal";
            return;
        }

        double fraction = 0;
        double magnitude = 1;
        do {
            fraction *= 10;
            magnitude *= 10;
            fraction += scanner_current(scanner) - '0';
            scanner_advance_offset(scanner);
        } while (!scanner_at_end(scanner) && scanner_current(scanner) >= '0' && scanner_current(scanner) <= '9');

        result += fraction / magnitude;
    }

    if (!scanner_at_end(scanner) && (scanner_current(scanner) == 'e' || scanner_current(scanner) == 'E')) {
        scanner_advance_offset(scanner);

        if (scanner_at_end(scanner)) {
            scanner->token.type = BowlErrorToken;
            scanner->token.column = scanner->column;
            scanner->token.line = scanner->line;
            scanner->token.error.message = "illegal number literal";
            return;
        }

        bool exponent_negative = false;
        if (scanner_current(scanner) == '-') {
            exponent_negative = true;
            scanner_advance_offset(scanner);
        } else if (scanner_current(scanner) == '+') {
            scanner_advance_offset(scanner);
        }

        if (scanner_at_end(scanner) || scanner_current(scanner) < '0' || scanner_current(scanner) > '9') {
            scanner->token.type = BowlErrorToken;
            scanner->token.column = scanner->column;
            scanner->token.line = scanner->line;
            scanner->token.error.message = "illegal number literal";
            return;
        }

        double exponent = 0;
        do {
            exponent *= 10;
            exponent += scanner_current(scanner) - '0';
            scanner_advance_offset(scanner);
        } while (!scanner_at_end(scanner) && scanner_current(scanner) >= '0' && scanner_current(scanner) <= '9');

        if (exponent_negative) {
            exponent = -exponent;
        }

        result += pow(10, exponent);
    }

    if (negative) {
        result = -result;        
    }

    scanner->token.number.value = result;
}

static void scanner_advance(BowlScanner *scanner) {
    scanner_skip_spaces(scanner);
    scanner->token_available = true;

    if (scanner_at_end(scanner)) {
        scanner->token.column = scanner->column;
        scanner->token.line = scanner->line;
        scanner->token.type = BowlEndOfStreamToken;
        return;
    }

    if (scanner_current(scanner) >= '0' && scanner_current(scanner) <= '9') {
        scanner_advance_number(scanner);
        return;
    }

    if ((scanner_current(scanner) == '+' || scanner_current(scanner) == '-')) {
        ++scanner->offset;
        if (!scanner_at_end(scanner) && scanner_current(scanner) >= '0' && scanner_current(scanner) <= '9') {
            --scanner->offset;
            scanner_advance_number(scanner);
            return;
        } else {
            --scanner->offset;
        }
    }
    
    if (scanner_current(scanner) == '"') {
        scanner_advance_string(scanner);
        return;
    }

    scanner_advance_symbol(scanner);
    if (scanner->token.type == BowlSymbolToken) {
        if (scanner_equals(&(*scanner->string)->string.codepoints[scanner->token.symbol.start], scanner->token.symbol.length, "true")) {
            scanner->token.type = BowlBooleanToken;
            scanner->token.boolean.value = true;
        } else if (scanner_equals(&(*scanner->string)->string.codepoints[scanner->token.symbol.start], scanner->token.symbol.length, "false")) {
            scanner->token.type = BowlBooleanToken;
            scanner->token.boolean.value = false;
        }
    }
}

BowlScanner scanner_from(BowlValue *string) {
    BowlScanner scanner = {
        .string = string,
        .offset = 0,
        .line = 1,
        .column = 1,
        .token_available = false,
        .token = {
            .type = BowlErrorToken,
            .line = 1,
            .column = 1
        }
    };

    if ((*string)->string.length == 0) {
        scanner.token.type = BowlEndOfStreamToken;
        scanner.token_available = true;
    }

    return scanner;
}

bool scanner_has_next(BowlScanner *scanner) {
    if (!scanner->token_available) {
        scanner_advance(scanner);
    }

    return scanner->token.type != BowlEndOfStreamToken;
}

BowlTokenType scanner_next(BowlScanner *scanner) {
    if (!scanner->token_available) {
        scanner_advance(scanner);
    }

    scanner->token_available = false;

    return scanner->token.type;
}
