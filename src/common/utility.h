#ifndef UTILITY_H
#define UTILITY_H

#include <lime/common.h>

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

bool is_integer(double value);

char *escape(char c);

void assert(bool test, char *message, ...);

void println(char *message, ...);

char unescape(char character);

#endif