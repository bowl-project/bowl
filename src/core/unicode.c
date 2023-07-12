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

u8 unicode_utf8_replacement_character[3] = { 0xEF, 0xBF, 0xBD };

u64 unicode_utf8_decode_codepoint(u8 *bytes, u64 length, u32 *state, u32 *codepoint) {
    u64 offset = 0;

    while (offset < length) {
        if (unicode_utf8_decode(state, codepoint, bytes[offset++]) == UNICODE_UTF8_STATE_ACCEPT || *state == UNICODE_UTF8_STATE_REJECT) {
            break;
        }
    }

    return offset;
}

u32 unicode_utf8_decode(u32 *state, u32 *codepoint, u32 byte) {
    const u32 type = utf8_transitions[byte];
    *codepoint = (*state != UNICODE_UTF8_STATE_ACCEPT) ? (byte & 0x3fu) | (*codepoint << 6) : (0xff >> type) & (byte);
    *state = utf8_transitions[256 + *state + type];
    return *state;
}

u64 unicode_utf8_encode(u32 codepoint, u8 *bytes) {
    if (codepoint <= 0x7F) {
        bytes[0] = (u8) codepoint;
        return 1;
    } else if (codepoint <= 0x07FF) {
        bytes[0] = (u8) (((codepoint >> 6) & 0x1F) | 0xC0);
        bytes[1] = (u8) ((codepoint & 0x3F) | 0x80);
        return 2;
    } else if (codepoint <= 0xFFFF) {
        bytes[0] = (u8) (((codepoint >> 12) & 0x0F) | 0xE0);
        bytes[1] = (u8) (((codepoint >> 6) & 0x3F) | 0x80);
        bytes[2] = (u8) ((codepoint & 0x3F) | 0x80);
        return 3;
    } else if (codepoint <= 0x10FFFF) {
        bytes[0] = (u8) (((codepoint >> 18) & 0x07) | 0xF0);
        bytes[1] = (u8) (((codepoint >> 12) & 0x3F) | 0x80);
        bytes[2] = (u8) (((codepoint >> 6) & 0x3F) | 0x80);
        bytes[3] = (u8) ((codepoint & 0x3F) | 0x80);
        return 4;
    } else {
        bytes[0] = (u8) 0xEF;
        bytes[1] = (u8) 0xBF;
        bytes[2] = (u8) 0xBD;
        return 0;
    }
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

u64 unicode_escape_sequence(u32 *codepoints, u64 length, u32 *codepoint) {
    u64 offset = 0;

    if (offset >= length) {
        *codepoint = 0;
        return offset;
    }

    if (codepoints[offset] != '\\') {
        *codepoint = codepoints[offset];
        return offset + 1;
    }

    if (++offset >= length) {
        *codepoint = UNICODE_REPLACEMENT_CHARACTER;
        return offset;
    }

    if (codepoints[offset] != 'u' && codepoints[offset] != 'U') {
        // ASCII escape sequence
        switch (codepoints[offset]) {
            case 't': *codepoint = '\t'; break;
            case 'f': *codepoint = '\f'; break;
            case 'v': *codepoint = '\v'; break;
            case 'b': *codepoint = '\b'; break;
            case 'r': *codepoint = '\r'; break;
            case 'n': *codepoint = '\n'; break;
            case 'a': *codepoint = '\a'; break;
            default: *codepoint = codepoints[offset]; break;
        }

        return offset + 1;
    }

    if (offset + 1 >= length) {
        // there are no more codepoints => just return the 'u' or 'U'
        *codepoint = codepoints[offset];
        return offset + 1;
    }

    // skip the 'u' or 'U'
    ++offset;

    u32 result = 0;
    u32 bytes_read = 0;
    do {
        if (offset >= length) {
            if (bytes_read > 0) {
                *codepoint = result;
                return offset;
            } else {
                *codepoint = UNICODE_REPLACEMENT_CHARACTER;
                return offset;
            }
        }

        // get the higher 4 bits
        const u8 higher = unicode_interpret_hex_digit(codepoints[offset++]);

        if (offset >= length) {
            *codepoint = UNICODE_REPLACEMENT_CHARACTER;
            return offset;
        }

        // get the lower 4 bits
        const u8 lower = unicode_interpret_hex_digit(codepoints[offset++]);
    
        // illegal hex digits
        if (higher == 0xFF || lower == 0xFF) {
            *codepoint = UNICODE_REPLACEMENT_CHARACTER;
            return offset;
        }

        ++bytes_read;
        result <<= 8; 
        result |= (((higher & 0xF) << 4) | (lower & 0xF));
    } while (offset < length && bytes_read < 4); // a unicode escape sequence may consist of 4 bytes at most

    *codepoint = result;
    return offset;
}

u64 unicode_utf8_escape_sequence(u8 *bytes, u64 length, u32 *codepoint) {
    u32 state = UNICODE_UTF8_STATE_ACCEPT;
    u8 *const start = bytes;
    u64 offset = 0;

    if (offset >= length) {
        *codepoint = 0;
        return offset;
    }

    // read next codepoint
    offset += unicode_utf8_decode_codepoint(&bytes[offset], length, &state, codepoint);
    if (state != UNICODE_UTF8_STATE_ACCEPT) {
        *codepoint = UNICODE_REPLACEMENT_CHARACTER;
        return offset;
    }

    if (*codepoint != '\\') {
        return offset;
    }

    if (offset >= length) {
        // codepoint expected after backslash
        *codepoint = UNICODE_REPLACEMENT_CHARACTER;
        return offset;
    }

    // read next codepoint
    offset += unicode_utf8_decode_codepoint(&bytes[offset], length, &state, codepoint);
    if (state != UNICODE_UTF8_STATE_ACCEPT) {
        *codepoint = UNICODE_REPLACEMENT_CHARACTER;
        return offset;
    }

    if (*codepoint != 'u' && *codepoint != 'U') {
        // ASCII escape sequence
        switch (*codepoint) {
            case 't': *codepoint = '\t'; break;
            case 'f': *codepoint = '\f'; break;
            case 'v': *codepoint = '\v'; break;
            case 'b': *codepoint = '\b'; break;
            case 'r': *codepoint = '\r'; break;
            case 'n': *codepoint = '\n'; break;
            case 'a': *codepoint = '\a'; break;
            default: break; // leave this codepoint untouched
        }

        return offset;
    }

    // at this point the escape sequence starts with \u or \U
    if (offset >= length) {
        // there are no more bytes => just return the 'u' or 'U'
        return offset;
    }

    u32 result = 0;
    u32 bytes_read = 0;
    do {
        // read next codepoint
        const u64 read = unicode_utf8_decode_codepoint(&bytes[offset], length, &state, codepoint);
        if (state != UNICODE_UTF8_STATE_ACCEPT) {
            if (bytes_read > 0) {
                // unicode escape sequence consists of at least one byte, so it is fine that there are no more bytes available
                *codepoint = result;
                // return the old offset in this case (to mark these bytes as "unread")
                return offset;
            } else {
                *codepoint = UNICODE_REPLACEMENT_CHARACTER;
                return offset + read;
            }
        }
        offset += read;

        // get the higher 4 bits
        const u8 higher = unicode_interpret_hex_digit(*codepoint);

        // illegal unicode escape sequence
        if (offset >= length) {
            *codepoint = UNICODE_REPLACEMENT_CHARACTER;
            return offset;
        }
        
        // read next codepoint
        offset += unicode_utf8_decode_codepoint(&bytes[offset], length, &state, codepoint);
        if (state != UNICODE_UTF8_STATE_ACCEPT) {
            *codepoint = UNICODE_REPLACEMENT_CHARACTER;
            return offset;
        }

        // get the lower 4 bits
        const u8 lower = unicode_interpret_hex_digit(*codepoint);
    
        // illegal hex digits
        if (higher == 0xFF || lower == 0xFF) {
            *codepoint = UNICODE_REPLACEMENT_CHARACTER;
            return offset;
        }

        ++bytes_read;
        result <<= 8; 
        result |= (((higher & 0xF) << 4) | (lower & 0xF));
    } while (offset < length && bytes_read < 4); // a unicode escape sequence may consist of 4 bytes at most

    return offset;
}

u32 *unicode_from_string(char *string) {
    const u64 length = strlen(string);
    u32 *const result = malloc(length * sizeof(u32));

    if (result != NULL) {
        for (u64 i = 0; i < length; ++i) {
            result[i] = (u32) string[i];
        }
    }

    return result;
}

char *unicode_to_string(u32 *codepoints, u64 length) {
    char *result = malloc(length * sizeof(u32) + 1);
    
    if (result != NULL) {
        u64 p = 0;
        for (u64 i = 0; i < length; ++i) {
            const u64 written = unicode_utf8_encode(codepoints[i], &result[p]);
            if (written == 0) {
                p += 3;
            } else  {
                p += written;
            }
        }
        result[p] = '\0';
    }

    return result;
}
