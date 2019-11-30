#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <inttypes.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t  s8;

#ifndef MAX
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#endif

#ifndef MIN
#define MIN(x, y) ((y) > (x) ? (y) : (x))
#endif

bool is_integer(double value);

char *escape(char c);

#endif
