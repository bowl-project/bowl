#include "common.h"

bool is_integer(double value) {
    return (double) ((u64) value) == value;
}

char *escape(char c) {
    switch (c) {
        case '\t': return "\\t";
        case '\a': return "\\a";
        case '\r': return "\\r";
        case '\n': return "\\n";
        case '\v': return "\\v";
        case '\f': return "\\f";
        case '\0': return "\\0";
        case '\b': return "\\b";
        default:   return NULL;
    }
}
