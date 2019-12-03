#ifndef MODULE_H
#define MODULE_H

#include "lime.h"

/**
 * This function must be implemented by the module author.
 * It is executed as soon as the virtual machine loads the native library.
 * @param stack The stack of the current environment.
 * @return Either an exception or 'NULL' if no exception occurred.
 */
LimeValue lime_module_initialize(LimeStack stack);

/**
 * This function must be implemented by the module author.
 * It is executed as soon as the virtual machine unloads the native library.
 * @param stack The stack of the current environment.
 * @return Either an exception or 'NULL' if no exception occurred.
 */
LimeValue lime_module_finalize(LimeStack stack);

#endif