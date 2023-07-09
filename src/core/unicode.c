#include "unicode.h"

/**
 * This source is based on the flexible and economical UTF-8 decoder from Björn Höhrmann.
 * Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 * See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
 */

static const u8 utf8_transitions[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    10, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 3, 3, 11, 6, 6, 6, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    0, 12, 24, 36, 60, 96, 84, 12, 12, 12, 48, 72, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 0, 12, 12, 12, 12, 12, 0, 12, 0, 12, 12, 12, 24, 12, 12, 12, 12, 12, 24, 12, 24, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, 12, 12, 12, 24, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12, 12, 36, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12,
    12, 36, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12
};

u32 unicode_utf8_decode(u32 *state, u32 *codepoint, u32 byte) {
    const u32 type = utf8_transitions[byte];
    *codepoint = (*state != UNICODE_UTF8_STATE_ACCEPT) ? (byte & 0x3fu) | (*codepoint << 6) : (0xff >> type) & (byte);
    *state = utf8_transitions[256 + *state + type];
    return *state;
}

u64 unicode_utf8_count(u8 *bytes, u64 length) {
    u32 state = UNICODE_UTF8_STATE_ACCEPT;
    u32 codepoint;
    u64 result = 0;

    for (u64 index = 0; index < length; ++index) {
        if (unicode_utf8_decode(&state, &codepoint, bytes[index]) == UNICODE_UTF8_STATE_ACCEPT) {
            ++result;
        } else if (state == UNICODE_UTF8_STATE_ACCEPT) {
            return (u64) -1;
        }
    }

    if (state != UNICODE_UTF8_STATE_ACCEPT) {
        return (u64) -2;
    }

    return result;
}

bool unicode_is_space(u32 codepoint) {
    switch (codepoint) {
        case 0x0009:
        case 0x000A:
        case 0x000B:
        case 0x000C:
        case 0x000D:
        case 0x0020:
        case 0x0085:
        case 0x00A0:
        case 0x1680:
        case 0x2000: 
        case 0x2001: 
        case 0x2002:
        case 0x2003:
        case 0x2004:
        case 0x2005:
        case 0x2006:
        case 0x2007:
        case 0x2008:
        case 0x2009:
        case 0x200A:
        case 0x202F:
        case 0x205F:
        case 0x3000:
            return true;
        default: 
            return false;
    }
}

static inline u8 unicode_interpret_hex_digit(u32 codepoint) {
    if (codepoint >= '0' && codepoint <= '9') {
        return codepoint - '0';
    } else if (codepoint >= 'a' && codepoint <= 'z') {
        return codepoint - 'a';
    } else if (codepoint >= 'A' && codepoint <= 'Z') {
        return codepoint - 'A';
    } else {
        return 0xFF;
    }
}

u32 unicode_interpret_escape_sequence(u32 **codepoints, u64 length) {
    if (length == 0) {
        return 0;
    }

    if (**codepoints != '\\') {
        return **codepoints;
    }

    if (length == 1) {
        return UNICODE_REPLACEMENT_CHARACTER;
    }

    *codepoints = *codepoints + 1;

    if (**codepoints != 'u' && **codepoints != 'U') {
        const u32 current = **codepoints;
        *codepoints = *codepoints + 1;

        // ASCII escape sequence
        switch (current) {
            case 't': return '\t';
            case 'f': return '\f';
            case 'v': return '\v';
            case 'b': return '\b';
            case 'r': return '\r';
            case 'n': return '\n';
            case 'a': return '\a';
            default: return current;
        }
    }

    // at this point the escape sequence starts with \u or \U
    // the sequence has to have an even number of digits (i.e., only full bytes can be described) and 
    // must contain at least one byte (i.e., two hex digits).
    if (length % 2 != 0 || length < 4) {
        return UNICODE_REPLACEMENT_CHARACTER;
    }

    u32 codepoint = 0;
    for (u64 i = 2; i < MIN(length, 10); i += 2) {
        const u8 first = unicode_interpret_hex_digit(**codepoints);
        *codepoints = *codepoints + 1;
        const u8 second = unicode_interpret_hex_digit(**codepoints);
        *codepoints = *codepoints + 1;

        if (first == 0xFF || second == 0xFF) {
            return UNICODE_REPLACEMENT_CHARACTER;
        }

        codepoint <<= 8; 
        codepoint |= (((first & 0xF) << 4) | (second & 0xF));
    }

    return codepoint;
}
