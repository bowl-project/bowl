#include "settings.h"

const char *lime_settings_kernel_path;

uint64_t lime_settings_verbosity;

void settings_set_kernel_path(const char *const kernel_path) {
    lime_settings_kernel_path = kernel_path;
}

void settings_set_verbosity(const uint64_t const verbosity) {
    lime_settings_verbosity = verbosity;
}
