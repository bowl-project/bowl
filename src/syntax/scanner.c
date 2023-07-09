#include "scanner.h"

static inline bool scanner_sequence_equals_cstr(u8 *bytes, u64 bytes_length, char *string) {
    u32 state = UNICODE_UTF8_STATE_ACCEPT;
    u32 codepoint;
    u64 offset = 0;

    while (*string) {
        const char c = *string++;

        while (offset < bytes_length) {
            if (unicode_utf8_decode(&state, &codepoint, bytes[offset++]) == UNICODE_UTF8_STATE_ACCEPT) {
                break;
            } else if (state == UNICODE_UTF8_STATE_REJECT) {
                return false;
            }
        }

        if (state != UNICODE_UTF8_STATE_ACCEPT) {
            return false;
        }

        if (codepoint != c) {
            return false;
        }
    }

    return offset == bytes_length;
}

static inline u64 scanner_peek_next_codepoint(BowlScanner *scanner) {
    u32 state = UNICODE_UTF8_STATE_ACCEPT;
    u32 codepoint;
    u64 offset = scanner->offset;

    while (offset < (*scanner->string)->string.size) {
        switch (unicode_utf8_decode(&state, &codepoint, (*scanner->string)->string.bytes[offset++])) {
            case UNICODE_UTF8_STATE_ACCEPT:
                return codepoint;
            case UNICODE_UTF8_STATE_REJECT:
                return (u64) -1;
        }
    }

    return (u64) -1;
}

/**
 * Reads the next codepoint in the underlying source and advances the internal column and line counters. 
 * Afterwards, the codepoint_offset as well as the column- and line-pointers point at the read codepoint. 
 * The offset points to the codepoint after the one which was read or at the end of the source.
 * When this function is called at the end of the stream, nothing is read and the codepoint is set to 0.
 * @param scanner The scanner to use.
 * @return Whether the next codepoint could be successfully read.
 */
static inline bool scanner_read_next_codepoint(BowlScanner *scanner) {
    if (scanner->codepoint != 0) {
        if (scanner->codepoint == '\n') {
            scanner->column = 1;
            ++scanner->line;
        } else {
            ++scanner->column;
        }
    }

    scanner->codepoint_offset = scanner->offset;
    while (scanner->offset < (*scanner->string)->string.size) {
        switch (unicode_utf8_decode(&scanner->state, &scanner->codepoint, (*scanner->string)->string.bytes[scanner->offset++])) {
            case UNICODE_UTF8_STATE_ACCEPT:
                return true;
            case UNICODE_UTF8_STATE_REJECT:
                scanner->token.type = BowlErrorToken;
                scanner->token.error.message = "malformed UTF-8 sequence";
                scanner->token.line = scanner->line;
                scanner->token.column = scanner->column;
                return false;
        }
    }
    
    if (scanner->state != UNICODE_UTF8_STATE_ACCEPT) {
        scanner->token.type = BowlErrorToken;
        scanner->token.error.message = "incomplete UTF-8 sequence";
        scanner->token.line = scanner->line;
        scanner->token.column = scanner->column;
        return false;
    } else {
        scanner->codepoint = 0;
        return true;
    }
}

static inline bool scanner_skip_spaces(BowlScanner *scanner) {
    // in case the end-of-source is hit this loop terminates since the codepoint is set to 0 (which is not a space)
    while (unicode_is_space(scanner->codepoint)) { 
        if (!scanner_read_next_codepoint(scanner)) {
            return false;
        }
    }

    return true;
}

static inline void scanner_advance_symbol(BowlScanner *scanner) {
    scanner->token.type = BowlSymbolToken;
    scanner->token.column = scanner->column;
    scanner->token.line = scanner->line;
    scanner->token.symbol.start = scanner->codepoint_offset;

    while (scanner->codepoint != 0 && !unicode_is_space(scanner->codepoint)) {
        if (!scanner_read_next_codepoint(scanner)) {
            return;
        }
    }

    scanner->token.symbol.length = scanner->offset - scanner->token.symbol.start;
}

static inline void scanner_advance_string(BowlScanner *scanner) {
    scanner->token.type = BowlStringToken;
    scanner->token.column = scanner->column;
    scanner->token.line = scanner->line;

    if (scanner->codepoint != '"') {
        scanner->token.type = BowlErrorToken;
        scanner->token.error.message = "illegal start of string literal";
        return;
    }

    if (!scanner_read_next_codepoint(scanner)) {
        return;
    }

    scanner->token.string.start = scanner->codepoint_offset;
    
    register bool escaped = false;
    while (!scanner->codepoint != 0 && (scanner->codepoint != '"' || escaped)) {
        if (escaped) {
            escaped = false;
        } else if (scanner->codepoint == '\\') {
            escaped = true;
        }

        if (!scanner_read_next_codepoint(scanner)) {
            return;
        }
    }

    if (scanner->codepoint != '"') {
        scanner->token.type = BowlErrorToken;
        scanner->token.column = scanner->column;
        scanner->token.line = scanner->line;
        scanner->token.error.message = "illegal end of string literal";
        return;
    }

    scanner->token.string.length = scanner->codepoint_offset - scanner->token.string.start;

    if (!scanner_read_next_codepoint(scanner)) {
        return;
    }
}

static inline void scanner_advance_number(BowlScanner *scanner) {
    scanner->token.type = BowlNumberToken;
    scanner->token.column = scanner->column;
    scanner->token.line = scanner->line;

    bool negative = false;

    if (scanner->codepoint == '-') {
        negative = true;
        if (!scanner_read_next_codepoint(scanner)) {
            return;
        }
    } else if (scanner->codepoint == '+') {
        if (!scanner_read_next_codepoint(scanner)) {
            return;
        }
    }

    if (scanner->codepoint < '0' || scanner->codepoint > '9') {
        scanner->token.type = BowlErrorToken;
        scanner->token.column = scanner->column;
        scanner->token.line = scanner->line;
        scanner->token.error.message = "illegal number literal";
        return;
    }

    double result = 0;
    do {
        result *= 10;
        result += scanner->codepoint - '0';
        if (!scanner_read_next_codepoint(scanner)) {
            return;
        }
    } while (scanner->codepoint >= '0' && scanner->codepoint <= '9');

    if (scanner->codepoint == '.') {
        if (!scanner_read_next_codepoint(scanner)) {
            return;
        }

        if (scanner->codepoint < '0' || scanner->codepoint > '9') {
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
            fraction += scanner->codepoint - '0';
            if (!scanner_read_next_codepoint(scanner)) {
                return;
            }
        } while (scanner->codepoint >= '0' && scanner->codepoint <= '9');

        result += fraction / magnitude;
    }

    if (scanner->codepoint == 'e' || scanner->codepoint == 'E') {
        if (!scanner_read_next_codepoint(scanner)) {
            return;
        }

        bool exponent_negative = false;
        if (scanner->codepoint == '-') {
            exponent_negative = true;
            if (!scanner_read_next_codepoint(scanner)) {
                return;
            }
        } else if (scanner->codepoint == '+') {
            if (!scanner_read_next_codepoint(scanner)) {
                return;
            }
        }

        if (scanner->codepoint < '0' || scanner->codepoint > '9') {
            scanner->token.type = BowlErrorToken;
            scanner->token.column = scanner->column;
            scanner->token.line = scanner->line;
            scanner->token.error.message = "illegal number literal";
            return;
        }

        double exponent = 0;
        do {
            exponent *= 10;
            exponent += scanner->codepoint - '0';
            if (!scanner_read_next_codepoint(scanner)) {
                return;
            }
        } while (scanner->codepoint >= '0' && scanner->codepoint <= '9');

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

    if (scanner->codepoint == 0) {
        scanner->token.column = scanner->column;
        scanner->token.line = scanner->line;
        scanner->token.type = BowlEndOfStreamToken;
        return;
    }

    if (scanner->codepoint >= '0' && scanner->codepoint <= '9') {
        scanner_advance_number(scanner);
        return;
    }

    if (scanner->codepoint == '+' || scanner->codepoint == '-') {
        const u64 next_codepoint = scanner_peek_next_codepoint(scanner);
        if (next_codepoint >= '0' && next_codepoint <= '9') {
            scanner_advance_number(scanner);
            return;
        }
    }
    
    if (scanner->codepoint == '"') {
        scanner_advance_string(scanner);
        return;
    }

    scanner_advance_symbol(scanner);
    if (scanner->token.type == BowlSymbolToken) {
        u8 *const bytes = &(*scanner->string)->string.bytes[scanner->token.symbol.start];
        if (scanner_sequence_equals_cstr(bytes, scanner->token.symbol.length, "true")) {
            scanner->token.type = BowlBooleanToken;
            scanner->token.boolean.value = true;
        } else if (scanner_sequence_equals_cstr(bytes, scanner->token.symbol.length, "false")) {
            scanner->token.type = BowlBooleanToken;
            scanner->token.boolean.value = false;
        }
    }
}

BowlScanner scanner_from(BowlValue *string) {
    BowlScanner scanner = {
        .string = string,
        .state = UNICODE_UTF8_STATE_ACCEPT,
        .codepoint = 0,
        .codepoint_offset = 0,
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

    // read the first codepoint, if possible
    if ((*string)->string.size > 0) {
        scanner_read_next_codepoint(&scanner);
    } else {
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
