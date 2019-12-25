#ifndef SETTINGS_H
#define SETTINGS_H

#include <inttypes.h>

void settings_set_kernel_path(const char *const kernel_path);

void settings_set_verbosity(const uint64_t const verbosity);

#endif