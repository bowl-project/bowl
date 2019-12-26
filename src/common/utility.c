#include "utility.h"

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
        case '\"': return "\\\"";
        case '\\': return "\\\\";
        default:   return NULL;
    }
}

void assert(bool test, char *message, ...) {
    va_list list;

    if (!test) {
        fprintf(stderr, "[assertion failed] ");
        va_start(list, message);
        vfprintf(stderr, message, list);
        va_end(list);
        fprintf(stderr, "\n");
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
}

void println(char *message, ...) {
    va_list list;

    va_start(list, message);
    vfprintf(stdout, message, list);
    va_end(list);

    fprintf(stdout, "\n");
    fflush(stdout);
}

char unescape(char character) {
    switch (character) {
        case 't': return '\t';
        case 'f': return '\f';
        case 'v': return '\v';
        case 'b': return '\b';
        case '0': return '\0';
        case 'r': return '\r';
        case 'n': return '\n';
        case 'a': return '\a';
        default: return character;
    }
}
