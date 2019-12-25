#include "main.h"

static CommandLineFlag commands[] = {
    {
        .name = "version",
        .synonyms = { "v" },
        .description = "Prints the version of this virtual machine.",
        .number_of_arguments = 0,
        .function = command_version
    },
    {
        .name = "help",
        .synonyms = { "h", "?" },
        .description = "Prints this help message.",
        .number_of_arguments = 0,
        .function = command_help
    },
    {
        .name = "execute",
        .synonyms = { "x", "e" },
        .description = 
            "Executes a new machine instance by tokenizing the provided\n"
            "argument, using the result as the callstack.",
        .number_of_arguments = 1,
        .function = command_execute
    }, 
    {
        .name = "verbose",
        .synonyms = { "vl" },
        .description = 
            "Sets the verbosity level to the provided argument.\n"
            "By default, this flag is set to '0' which indicates the\n"
            "lowest level of verbosity. Starting with a level of '1'\n"
            "the datastack will be printed after each instruction when\n"
            "executing a program.",
        .number_of_arguments = 1,
        .function = command_verbose
    },
    {
        .name = "kernel",
        .synonyms = { "k" },
        .description = 
            "Specifies the shared library which should be used as the\n"
            "kernel module.\n"
            "By default this flag is set to '"
            #if defined(OS_WINDOWS) 
                "kernel.dll"
            #else 
                "kernel.so"
            #endif
            "'. That is, it is\n"
            "up to the operating system which paths are searched in\n"
            "order to load the library.",
        .number_of_arguments = 1,
        .function = command_kernel
    }
};

bool command_help(char *arguments[]) {
    cli_describe(commands, sizeof(commands) / sizeof(commands[0]));
    return true;
}

bool command_execute(char *arguments[]) {
    execute(arguments[0]);
    return true;
}

bool command_version(char *arguments[]) {
    printf("[version] lime virtual machine version v%s built on %s (%s %s)\n", LIME_VM_VERSION, __DATE__, OS_NAME, OS_ARCHITECTURE);
    return true;
}

bool command_kernel(char *arguments[]) {
    settings_set_kernel_path(arguments[0]);
    return true;
}

bool command_verbose(char *arguments[]) {
    u64 verbosity;
    if (sscanf(arguments[0], "%" PRId64, &verbosity) != 1) {
        cli_error("illegal verbosity level '%s'", arguments[0]);
        return false;
    } else {
        settings_set_verbosity(verbosity);
        return true;
    }
}

int main(int argument_count, char *arguments[]) {
    #if defined(OS_WINDOWS)
    settings_set_kernel_path("kernel.dll");
    #else
    settings_set_kernel_path("kernel.so");
    #endif

    settings_set_verbosity(0);

    cli_parse(commands, sizeof(commands) / sizeof(commands[0]), &arguments[1], argument_count - 1);
    return EXIT_SUCCESS;
}
