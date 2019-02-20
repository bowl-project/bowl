#include "utility.h"

void fatal(char *message, ...) {
    va_list list;
    va_start(list, message);
    fprintf(stderr, "[fatal error] ");
    vfprintf(stderr, message, list);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(list);
    exit(EXIT_FAILURE);
}

bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v' || c == '\b' || c == '\0';
}
