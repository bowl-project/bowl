#include "scanner.h"

// TODO: 
// - finish unicode support
// - handle erroneous utf-8 encodings by returning different error tokens.

static inline bool scanner_is_finished(BowlScanner *scanner) {
    return scanner->current >= (*scanner->string)->string.size;
}

static inline u64 scanner_read_next_codepoint(BowlScanner *scanner) {
    while (!scanner_is_finished(scanner)) {
        switch (unicode_utf8_decode(&scanner->state, &scanner->codepoint, (*scanner->string)->string.bytes[scanner->current++])) {
            case UNICODE_UTF8_ACCEPT:
                return scanner->codepoint;
            case UNICODE_UTF8_REJECT:
                return -1;
        }
    }
    
    // incomplete codepoint
    return -2;
}

static inline void scanner_skip_spaces(BowlScanner *scanner) {
    while (!scanner_is_finished(scanner) && unicode_is_space(scanner->codepoint)) {
        if (scanner->codepoint == '\n') {
            scanner->column = 1;
            ++scanner->line;
        } else {
            ++scanner->column;
        }

// TODO error handling ...
        scanner_read_next_codepoint(scanner);
    }
}

static inline u64 scanner_advance_symbol(BowlScanner *scanner) {
    const u64 start = scanner->current;
    while (!scanner_is_finished(scanner) && !unicode_is_space(scanner->codepoint)) {
        ++scanner->column;

        // TODO : error handling
        scanner_read_next_codepoint(scanner);
    }
    return start;
}

static inline u64 scanner_advance_string(BowlScanner *scanner) {
    const u64 start = ++scanner->current;

    register bool escaped = false;
    register char current;
    while (!scanner_is_finished(scanner) && ((current = scanner_peek(scanner)) != '"' || escaped)) {
        ++scanner->current;
        ++scanner->column;

        if (escaped) {
            escaped = false;
        } else if (current == '\\') {
            escaped = true;
        } else if (current == '\n') {
            ++scanner->line;
            scanner->column = 1;
        }
    }
    
    if (scanner_is_finished(scanner)) {
        return scanner_string_size
    (scanner);
    } else {
        ++scanner->current;
        ++scanner->column;
    }

    return start;
}

static void scanner_advance(BowlScanner *scanner) {
    scanner_skip_spaces(scanner);

    scanner->token.line = scanner->line;
    scanner->token.column = scanner->column;
    scanner->initialized = true;

    if (scanner_is_finished(scanner)) {
        scanner->token.type = BowlEndOfStreamToken;
    } else {
        const char current = scanner_peek(scanner);

        if (current >= '0' && current <= '9') {
            char *const start = (char *) ((*scanner->string)->string.bytes + scanner->current);
            char *end;

            scanner->token.type = BowlNumberToken;
            scanner->token.number.value = strtod(start, &end);
            if (end == start) {
                scanner->token.type = BowlErrorToken;
                scanner->token.error.message = "illegal number literal";
            } else {
                scanner->current += end - start;
                scanner->column += end - start;
            }
        } else if (current == '"') {
            const u64 start = scanner_advance_string(scanner);

            if (start == scanner_string_size(scanner)) {
                scanner->token.type = BowlErrorToken;
                scanner->token.error.message = "unexpected end of string literal";
            } else {
                scanner->token.type = BowlStringToken;
                scanner->token.string.start = start;
                scanner->token.string.size = scanner->current - 1 - start;
            }
        } else {
            const u64 start = scanner_advance_symbol(scanner);

            if (SCANNER_STARTS_WITH_LITERAL(scanner, "true", start)) {
                scanner->token.type = BowlBooleanToken;
                scanner->token.boolean.value = true;
            } else if (SCANNER_STARTS_WITH_LITERAL(scanner, "false", start)) {
                scanner->token.type = BowlBooleanToken;
                scanner->token.boolean.value = false;
            } else {
                scanner->token.type = BowlSymbolToken;
                scanner->token.symbol.start = start;
                scanner->token.symbol.length = scanner->current - start;
            }
        }
    }

}

BowlScanner scanner_from(BowlValue *string) {
    BowlScanner scanner = {
        .string = string,
        .state = UNICODE_UTF8_ACCEPT,
        .codepoint = 0,
        .current = 0,
        .line = 1,
        .column = 1,
        .initialized = false,
        .token = {
            .type = BowlErrorToken,
            .line = 1,
            .column = 1
        }
    };

    // read the first codepoint, if possible
    if ((*string)->string.size > 0) {
        scanner_read_next_codepoint(&scanner);
    }

    return scanner;
}

bool scanner_has_next(BowlScanner *scanner) {
    if (!scanner->initialized) {
        scanner_advance(scanner);
    }

    return scanner->token.type != BowlEndOfStreamToken;
}

BowlTokenType scanner_next(BowlScanner *scanner) {
    if (!scanner->initialized) {
        scanner_advance(scanner);
    }

    scanner->initialized = false;

    return scanner->token.type;
}
