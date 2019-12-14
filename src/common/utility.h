#ifndef UTILITY_H
#define UTILITY_H

#include "common.h"
#include <math.h>

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

bool is_integer(double value);

char *escape(char c);

void assert(bool test, char *message, ...);

#endif