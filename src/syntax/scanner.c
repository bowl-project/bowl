#include "scanner.h"

static inline u64 scanner_string_length(BowlScanner *scanner) {
    return (*scanner->string)->string.length;
}

static inline bool scanner_is_finished(BowlScanner *scanner) {
    return scanner->current >= scanner_string_length(scanner);
}

static inline char scanner_peek(BowlScanner *scanner) {
    return (*scanner->string)->string.bytes[scanner->current];
}

static inline void scanner_skip_spaces(BowlScanner *scanner) {
    register char current;
    while (!scanner_is_finished(scanner) && isspace(current = scanner_peek(scanner))) {
        if (current == '\n') {
            scanner->column = 1;
            ++scanner->line;
        } else {
            ++scanner->column;
        }
        ++scanner->current;
    }
}

static inline u64 scanner_advance_symbol(BowlScanner *scanner) {
    const u64 start = scanner->current;
    while (!scanner_is_finished(scanner) && !isspace(scanner_peek(scanner))) {
        ++scanner->current;
        ++scanner->column;
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
        return scanner_string_length(scanner);
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

            if (start == scanner_string_length(scanner)) {
                scanner->token.type = BowlErrorToken;
                scanner->token.error.message = "unexpected end of string literal";
            } else {
                scanner->token.type = BowlStringToken;
                scanner->token.string.start = start;
                scanner->token.string.length = scanner->current - 1 - start;
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
    return (BowlScanner) {
        .string = string,
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
