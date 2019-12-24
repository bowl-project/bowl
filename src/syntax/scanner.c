#include "scanner.h"

static inline void scanner_skip_spaces(LimeScanner *scanner) {
    register char current;
    while (scanner->current < scanner->length && isspace(current = scanner->bytes[scanner->current])) {
        if (current == '\n') {
            scanner->column = 1;
            ++scanner->line;
        } else {
            ++scanner->column;
        }
        ++scanner->current;
    }
}

static u64 scanner_advance_symbol(LimeScanner *scanner) {
    const u64 start = scanner->current;
    while (scanner->current < scanner->length && !isspace(scanner->bytes[scanner->current])) {
        ++scanner->current;
        ++scanner->column;
    }
    return start;
}

static u64 scanner_advance_string(LimeScanner *scanner) {
    const u64 start = ++scanner->current;

    register bool escaped = false;
    register char current;
    while (scanner->current < scanner->length && ((current = scanner->bytes[scanner->current]) != '"' || escaped)) {
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
    
    if (scanner->current >= scanner->length) {
        // TODO: error
    } else {
        ++scanner->current;
        ++scanner->column;
    }

    return start;
}

static void scanner_advance(LimeScanner *scanner) {
    scanner_skip_spaces(scanner);

    scanner->token.line = scanner->line;
    scanner->token.column = scanner->column;
    scanner->initialized = true;

    if (scanner->current >= scanner->length) {
        scanner->token.type = LimeEndOfStreamToken;
    } else {
        const char current = scanner->bytes[scanner->current];
        
        if (current >= '0' && current <= '9') {
            char *const start = (char *) (scanner->bytes + scanner->current);
            char *end;

            scanner->token.type = LimeNumberToken;
            scanner->token.number.value = strtod(start, &end);
            scanner->current += end - start;
            scanner->column += end - start;
        } else if (current == '"') {
            const u64 start = scanner_advance_string(scanner);
            scanner->token.type = LimeStringToken;
            scanner->token.string.start = start;
            scanner->token.string.length = scanner->current - 1 - start;
        } else {
            const u64 start = scanner_advance_symbol(scanner);

            if (scanner->current - start == sizeof("true") - 1 && !memcmp(scanner->bytes + start, "true", sizeof("true") - 1)) {
                scanner->token.type = LimeBooleanToken;
                scanner->token.boolean.value = true;
            } else if (scanner->current - start == sizeof("false") - 1 && !memcmp(scanner->bytes + start, "false", sizeof("false") - 1)) {
                scanner->token.type = LimeBooleanToken;
                scanner->token.boolean.value = false;
            } else {
                scanner->token.type = LimeSymbolToken;
                scanner->token.symbol.start = start;
                scanner->token.symbol.length = scanner->current - start;
            }
        }
    }

}

LimeScanner scanner_from(u8 *bytes, u64 length) {
    LimeScanner result = {
        .bytes = bytes,
        .length = length,
        .current = 0,
        .line = 1,
        .column = 1,
        .initialized = false,
        .token = {
            .type = LimeErrorToken,
            .line = 1,
            .column = 1
        }
    };

    return result;
}

bool scanner_has_next(LimeScanner *scanner) {
    if (!scanner->initialized) {
        scanner_advance(scanner);
    }

    return scanner->token.type != LimeEndOfStreamToken;
}

LimeTokenType scanner_next(LimeScanner *scanner) {
    if (!scanner->initialized) {
        scanner_advance(scanner);
    }

    scanner->initialized = false;

    return scanner->token.type;
}
