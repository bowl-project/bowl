#ifndef CLI_H
#define CLI_H

#include "utility.h"
#include <lime/common.h>

typedef bool (*CommandLineFunction)(char *arguments[]);

typedef struct {
    char *name;
    char *synonyms[4];
    char *description;
    u64 number_of_arguments;
    CommandLineFunction function;
} CommandLineFlag;

void cli_error(char *message, ...);

void cli_parse(CommandLineFlag *flags, u64 flag_count, char *arguments[], u64 argument_count);

void cli_describe(CommandLineFlag *flags, u64 flag_count);

#endif