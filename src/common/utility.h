#ifndef COMMON_UTILITY_H
#define COMMON_UTILITY_H

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <dlfcn.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

typedef uint64_t word;
typedef uint8_t byte;

void fatal(char *message, ...);

bool is_whitespace(char c);

#endif
