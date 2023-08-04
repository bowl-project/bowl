#include "main.h"

const char *bowl_settings_boot_path = "boot.bowl";

const char *bowl_settings_kernel_path =
    #if defined(OS_WINDOWS)
        "kernel.dll"
    #else
        "kernel.so"
    #endif
;

u64 bowl_settings_verbosity = 0;

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
    },
    {
        .name = "boot",
        .synonyms = { "b" },
        .description = 
            "Specifies the path to the boot file. By default this\n"
            "flag is set to 'boot.bowl'.\n",
        .number_of_arguments = 1,
        .function = command_boot
    }
};

bool command_help(char *arguments[]) {
    cli_describe(commands, sizeof(commands) / sizeof(commands[0]));
    return true;
}

bool command_execute(char *arguments[]) {
    static const char *const handle_sandbox_return = 
        "drop " // the datastack is not needed, just drop it
        "dup list:empty equals " // check if the exception is not 'NULL' (the empty list)
        "\\\"drop\\\" tokens " // if the exception is null drop the saved exception value and do nothing
        "\\\"trigger\\\" tokens " // if the exception is not null rethrow it
        "boolean:choose " // choose the correct continuation
        // there are 26 tokens between the 'lift' and 'continue' (excluding 'lift', including 'continue')
        "lift rot rot list:empty swap list:push swap list:pop rot list:push rot " // pop the continuation and save the datastack and dictionary for later use
        "swap dup list:length 26 number:subtract 26 list:slice list:concat " // prepare the new callstack
        "swap list:pop swap list:pop swap drop rot continue " // prepare the continuation and execute it
        // overwrite the dictionary
        "lift rot rot drop list:pop rot swap dup list:length 14 number:subtract 14 list:slice swap continue"
    ;

    static const char *const bootloader = 
        "\"../bowl-io/io.so\" library drop\n" // load the io-library
        // prepend code that deletes the entire rest of the callstack 
        "lift swap \"lift swap drop list:empty swap continue\" tokens swap list:concat\n"
        // prepare the sandbox call
        "rot rot dup \"run %s\" tokens list:empty list:push \"%s\" io:read tokens list:push swap list:push\n"
        // prepend the sandbox call to the callstack and continue the execution
        "swap rot swap list:concat swap rot swap continue"
    ;

    // TODO: 1) escape user code (such that it is safe to use it inside a string) 2) set it as the new callstack
    char *const user_code = arguments[0];
    
    char buffer[4096 + sizeof(bootloader) - 1 + sizeof(handle_sandbox_return)];
    sprintf(buffer, bootloader, handle_sandbox_return, bowl_settings_boot_path);

    execute(buffer);

    return true;
}

bool command_version(char *arguments[]) {
    printf("[version] bowl virtual machine version v%s built on %s (%s %s)\n", BOWL_VM_VERSION, __DATE__, OS_NAME, OS_ARCHITECTURE);
    return true;
}

bool command_kernel(char *arguments[]) {
    bowl_settings_kernel_path = arguments[0];
    return true;
}

bool command_boot(char *arguments[]) {
    bowl_settings_boot_path = arguments[0];
    return true;
}

bool command_verbose(char *arguments[]) {
    u64 verbosity;
    if (sscanf(arguments[0], "%" PRId64, &verbosity) != 1) {
        cli_error("illegal verbosity level '%s'", arguments[0]);
        return false;
    } else {
        bowl_settings_verbosity = verbosity;
        return true;
    }
}

int main(int argument_count, char *arguments[]) {
    cli_parse(commands, sizeof(commands) / sizeof(commands[0]), &arguments[1], argument_count - 1);
    return EXIT_SUCCESS;
}
