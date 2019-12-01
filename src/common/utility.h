#ifndef UTILITY_H
#define UTILITY_H

#include "common.h"
#include <math.h>

#ifndef MAX
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#endif

#ifndef MIN
#define MIN(x, y) ((y) > (x) ? (y) : (x))
#endif

bool is_integer(double value);

char *escape(char c);

#endif