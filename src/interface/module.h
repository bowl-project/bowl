#ifndef MODULE_H
#define MODULE_H

#include "lime.h"

/**
 * The interface of module functions.
 * 
 * A module function accepts the stack of the current environment and
 * the library value which is associated with the module. It returns
 * either an exception or 'NULL' otherwise.
 */
typedef LimeValue (*LimeModuleFunction)(LimeStack, LimeValue);

/**
 * This function must be implemented by the module author.
 * It is executed as soon as the virtual machine loads the native library.
 * @param stack The stack of the current environment.
 * @param library The library value which represents this module.
 * @return Either an exception or 'NULL' if no exception occurred.
 */
LimeValue lime_module_initialize(LimeStack stack, LimeValue library);

/**
 * This function must be implemented by the module author.
 * It is executed as soon as the virtual machine unloads the native library.
 * @param stack The stack of the current environment.
 * @param library The library value which represents this module.
 * @return Either an exception or 'NULL' if no exception occurred.
 */
LimeValue lime_module_finalize(LimeStack stack, LimeValue library);

#endif