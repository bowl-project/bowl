#include "cli.h"

void cli_error(char *message, ...) {
    va_list list;
    fprintf(stderr, "[error] ");
    va_start(list, message);
    vfprintf(stderr, message, list);
    va_end(list);
    fprintf(stderr, "\n");
    fflush(stderr);
    exit(EXIT_FAILURE);
}

static inline bool cli_flag_matches(CommandLineFlag *flag, char *argument) {
    if (argument == NULL || *argument == '\0') {
        // empty string
        return false;
    } else if (*argument != '-') {
        // flag does not start with a dash
        return false;
    }

    ++argument;
    if (strcmp(flag->name, argument) == 0) {
        return true;
    }

    for (u64 i = 0; i < sizeof(flag->synonyms) / sizeof(flag->synonyms[0]) && flag->synonyms[i] != NULL; ++i) {
        if (strcmp(flag->synonyms[i], argument) == 0) {
            return true;
        }
    }

    return false;
}

void cli_parse(CommandLineFlag *flags, u64 flag_count, char *arguments[], u64 argument_count) {
    for (u64 i = 0; i < argument_count; ++i) {
        char *const argument = arguments[i];
        CommandLineFlag *flag = NULL;

        for (u64 j = 0; j < flag_count; ++j) {
            if (cli_flag_matches(&flags[j], argument)) {
                flag = &flags[j];
                break;
            }
        }

        if (flag == NULL) {
            cli_error("unknown command line flag '%s' (try '-help' for more information)", argument);
        } else if (argument_count - i - 1 < flag->number_of_arguments) {
            cli_error("missing arguments for flag '%s' (expected %" PRId64 " but got only %" PRId64 ")", argument, flag->number_of_arguments, argument_count - i - 1);
        }

        char *const previous = arguments[i + 1 + flag->number_of_arguments];
        arguments[i + 1 + flag->number_of_arguments] = NULL;
        const bool continue_parsing = flag->function(&arguments[i + 1]);
        arguments[i + 1 + flag->number_of_arguments] = previous;

        if (!continue_parsing) {
            return;
        }

        i += flag->number_of_arguments;
    }
}

static void cli_print_border(u64 max_name_length, u64 max_synonyms_length, u64 max_description_length) {
    printf("+-");
    for (u64 j = 0; j < max_name_length; ++j) {
        printf("-");
    }
    printf("-+-");
    for (u64 j = 0; j < max_synonyms_length; ++j) {
        printf("-");
    }
    printf("-+-");
    for (u64 j = 0; j < max_description_length; ++j) {
        printf("-");
    }
    printf("-+\n");
}

static char *cli_token(char *string, char delimiter) {
    static char buffer[4096];
    static char *current = NULL;
    static bool end = true;

    if (string != NULL) {
        end = false;
        strcpy(buffer, string);
        current = &buffer[0];
    } else if (current == NULL) {
        return NULL;
    }

    char *start = current;
    while (*current != '\0' && *current != delimiter) {
        ++current;
    }

    if (*current != '\0') {
        *current = '\0';
        ++current;
    } else {
        current = NULL;
    }

    return start;
}

void cli_describe(CommandLineFlag *flags, u64 flag_count) {
    char buffer[4096];
    u64 max_name_length = strlen("name (# arguments)");
    u64 max_synonyms_length = strlen("synonyms");
    u64 max_description_length = strlen("description");

    for (u64 i = 0; i < flag_count; ++i) {
        CommandLineFlag *flag = &flags[i];
        
        u64 synonyms_length = 0;
        for (u64 j = 0; j < sizeof(flag->synonyms) / sizeof(flag->synonyms[0]) && flag->synonyms[j] != NULL; ++j) {
            if (j > 0) {
                synonyms_length += strlen(" ");
            }
            synonyms_length += 1 + strlen(flag->synonyms[j]);
        }

        u64 description_length = 0;

        char *line = cli_token(flag->description, '\n');
        while (line != NULL) {
            description_length = MAX(description_length, strlen(line));
            line = cli_token(NULL, '\n');
        }

        snprintf(buffer, sizeof(buffer), "-%s (%" PRId64 ")", flag->name, flag->number_of_arguments);
        max_name_length = MAX(max_name_length, strlen(buffer));
        max_synonyms_length = MAX(max_synonyms_length, synonyms_length);
        max_description_length = MAX(max_description_length, description_length);
    }

    cli_print_border(max_name_length, max_synonyms_length, max_description_length);

    printf(
        "| %-*s | %-*s | %-*s |\n", 
        (int) max_name_length, "name (# arguments)", 
        (int) max_synonyms_length, "synonyms", 
        (int) max_description_length, "description"
    );

    for (u64 i = 0; i < flag_count; ++i) {
        CommandLineFlag *flag = &flags[i];

        cli_print_border(max_name_length, max_synonyms_length, max_description_length);

        snprintf(buffer, sizeof(buffer), "-%s (%" PRId64 ")", flag->name, flag->number_of_arguments);
        printf(
            "| %-*s | ", 
            (int) max_name_length, buffer
        );

        char *line = cli_token(flag->description, '\n');
        buffer[0] = '\0';

        for (u64 j = 0; j < sizeof(flag->synonyms) / sizeof(flag->synonyms[0]) && flag->synonyms[j] != NULL; ++j) {
            if (j > 0) {
                strcat(buffer, " ");
            }
            strcat(buffer, "-");
            strcat(buffer, flag->synonyms[j]);
        }

        printf(
            "%-*s | %-*s |\n", 
            (int) max_synonyms_length, buffer, 
            (int) max_description_length, line
        );

        line = cli_token(NULL, '\n');
        while (line != NULL) {
            printf(
            "| %-*s | %-*s | %-*s |\n", 
                (int) max_name_length, "", 
                (int) max_synonyms_length, "", 
                (int) max_description_length, line
            );
            line = cli_token(NULL, '\n');
        }
    }

    cli_print_border(max_name_length, max_synonyms_length, max_description_length);

    fflush(stdout);
}
