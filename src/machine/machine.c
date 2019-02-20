#include "machine.h"

Machine *machine_instances = NULL;
word machine_instance_count = 0;
static word machine_instance_capacity = 0;

static void machine_enter_native(Machine machine, char *name, void (*function)(Machine)) {
    machine->$0 = value_symbol_from_string(name);
    machine->$1 = value_native_from_string(name, (NativeFunction) function);
    machine->dictionary = value_map_put(machine->dictionary, machine->$0, machine->$1);
    machine->$0 = machine->$1 = NULL;
}

Machine machine_create(void) {
    Machine result = malloc(sizeof(struct machine));
    
    if (result == NULL) {
        fatal("out of heap");
    }

    for (word i = 0; i < MACHINE_REGISTER_COUNT; ++i) {
        result->registers[i] = NULL;
    }

    result->dictionary = value_map(16);
    result->callstack = NULL;
    result->datastack = NULL;

    if (machine_instance_count >= machine_instance_capacity) {
        machine_instance_capacity = max(machine_instance_capacity * 2, 16);
        machine_instances = realloc(machine_instances, machine_instance_capacity * sizeof(Machine));
        if (machine_instances == NULL) {
            free(result);
            fatal("out of heap");
        }
    }

    machine_instances[machine_instance_count++] = result;

    /* enter standard words */
    machine_enter_native(result, "run", machine_run);
    machine_enter_native(result, "step", machine_step);
    machine_enter_native(result, "dup", machine_dup);
    machine_enter_native(result, "exit", machine_exit);
    machine_enter_native(result, "swap", machine_swap);
    machine_enter_native(result, "rot", machine_rot);
    machine_enter_native(result, "drop", machine_drop);
    machine_enter_native(result, "read", machine_read);
    machine_enter_native(result, "input", machine_input);
    machine_enter_native(result, "show", machine_show);
    machine_enter_native(result, "tokens", machine_tokens);
    machine_enter_native(result, "invoke", machine_invoke);
    machine_enter_native(result, "print", machine_print);
    machine_enter_native(result, "continue", machine_continue);
    machine_enter_native(result, "equals", machine_equals);
    machine_enter_native(result, "head", machine_head);
    machine_enter_native(result, "tail", machine_tail);
    machine_enter_native(result, "split", machine_split);
    machine_enter_native(result, "nil", machine_nil);
    machine_enter_native(result, "concatenate", machine_concatenate);
    machine_enter_native(result, "prepend", machine_prepend);
    machine_enter_native(result, "put", machine_put);
    machine_enter_native(result, "get", machine_get);
    machine_enter_native(result, "empty-map", machine_empty_map);
    machine_enter_native(result, "get-or-else", machine_get_or_else);
    machine_enter_native(result, "reverse", machine_reverse);
    machine_enter_native(result, "add", machine_add);
    machine_enter_native(result, "sub", machine_sub);
    machine_enter_native(result, "mul", machine_mul);
    machine_enter_native(result, "div", machine_div);
    machine_enter_native(result, "rem", machine_rem);
    machine_enter_native(result, "library", machine_library);
    machine_enter_native(result, "change-directory", machine_change_directory);
    machine_enter_native(result, "get-directory", machine_get_directory);
    machine_enter_native(result, "execute", machine_execute);

    return result;
}

void machine_delete(Machine machine) {
    if (machine != NULL) {
        for (word i = 0; i < machine_instance_count; ++i) {
            if (machine_instances[i] == machine) {
                for (word j = i + 1; j < machine_instance_count; ++j) {
                    machine_instances[j - 1] = machine_instances[j];
                }
                machine_instance_count -= 1;
                break;
            }
        }
    }
    free(machine);
}

void machine_run(Machine machine) {
    while (machine->callstack != NULL) {
        machine_step(machine);
    }
}

void machine_step(Machine machine) {
    if (machine->callstack == NULL) {
        fatal("empty callstack");
    } else if (machine->callstack->type != ListValue) {
        fatal("illegal callstack");
    }

    #if DEBUG
        printf("callstack: ");
        value_dump(stdout, machine->callstack);
        printf("\n");
        printf("datastack: ");
        value_dump(stdout, machine->datastack);
        printf("\n");
    #endif

    machine->$0 = machine->callstack->list.head;
    machine->callstack = machine->callstack->list.tail;

    machine->$1 = value_map_get_or_else(machine->dictionary, machine->$0, value_marker);

    if (machine->$1 == value_marker) {
        machine->$1 = NULL;

        machine->datastack = value_list(machine->$0, machine->datastack);
        machine->$0 = NULL;

    } else if (machine->$1->type == ListValue) {
        /* expand word */
        while (machine->$1 != NULL) {
            machine->callstack = value_list(machine->$1->list.head, machine->callstack);
            machine->$1 = machine->$1->list.tail;
        }
    } else if (machine->$1->type == NativeValue) {
        void (*function)(void *) = machine->$1->native.function;
        machine->$0 = machine->$1 = NULL;
        function(machine);
    } else {
        fatal("illegal dictionary entry");
    }

    #if DEBUG
        printf("callstack: ");
        value_dump(stdout, machine->callstack);
        printf("\n");
        printf("datastack: ");
        value_dump(stdout, machine->datastack);
        printf("\n\n");
    #endif
}

void machine_equals(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("'equals' expects two arguments");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    } else if (machine->datastack->list.tail == NULL) {
        fatal("'equals' expects two arguments");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;
    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    const word result = value_equals(machine->$0, machine->$1) ? 1 : 0;
    machine->$0 = machine->$1 = NULL;
    machine->$0 = value_number(result);
    machine->datastack = value_list(machine->$0, machine->datastack);
    machine->$0 = NULL;
}

void machine_dup(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }
    machine->datastack = value_list(machine->datastack->list.head, machine->datastack);
}

void machine_exit(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("'exit' expects one argument");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$0 == NULL || machine->$0->type != NumberValue) {
        fatal("illegal argument for function 'exit'");
    }

    exit((word) machine->$0->number.value);
}

void machine_swap(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    } else if (machine->datastack->list.tail == NULL) {
        fatal("'swap' requires two arguments");
    }

    machine->$0 = machine->datastack->list.tail->list.head;
    machine->datastack = value_list(machine->datastack->list.head, machine->datastack->list.tail->list.tail);
    machine->datastack = value_list(machine->$0, machine->datastack);
    machine->$0 = NULL;
}

void machine_rot(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    } else if (machine->datastack->list.tail == NULL) {
        fatal("'rot' requires three arguments");
    } else if (machine->datastack->list.tail->list.tail == NULL) {
        fatal("'rot' requires three arguments");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;
    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;
    machine->$2 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->datastack = value_list(machine->$1, machine->datastack);
    machine->datastack = value_list(machine->$0, machine->datastack);
    machine->datastack = value_list(machine->$2, machine->datastack);

    machine->$0 = machine->$1 = machine->$2 = NULL;
}

void machine_drop(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->datastack = machine->datastack->list.tail;
}

void machine_read(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$0->type != StringValue) {
        fatal("'read' expects a string argument");
    }

    char *buffer = malloc((machine->$0->string.length + 1) * sizeof(char));
    
    if (buffer == NULL) {
        fatal("out of heap");
    }

    memcpy(buffer, machine->$0->string.value, machine->$0->string.length);
    buffer[machine->$0->string.length] = '\0';
    machine->$0 = NULL;

    FILE *file = fopen(buffer, "r");
    if (file == NULL) {
        fatal("failed to open file '%s'", buffer);
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fatal("failed to read file '%s'", buffer);
    }

    long int length = ftell(file);
    if (length < 0) {
        fatal("failed to read file '%s'", buffer);
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fatal("failed to read file '%s'", buffer);
    }

    machine->$0 = value_create(StringValue, length * sizeof(char));
    machine->$0->string.length = (word) length;
    if (fread(machine->$0->string.value, sizeof(char), length, file) != length) {
        fatal("failed to read file '%s'", buffer);
    }

    if (fclose(file) != 0) {
        fatal("failed to read file '%s'", buffer);
    }

    free(buffer);

    machine->datastack = value_list(machine->$0, machine->datastack);

    machine->$0 = NULL;
}

void machine_input(Machine machine) {
    static char *buffer = NULL;
    static word buffer_size = 0;
    static word buffer_capacity = 0;
    int current;

    /* reset buffer for next use */
    buffer_size = 0;

    current = fgetc(stdin);
    while (current != EOF && current != '\n') {

        if (buffer_size + 1 > buffer_capacity) {
            buffer_capacity = max(buffer_capacity * 2, buffer_size + 1);
            buffer = realloc(buffer, buffer_capacity * sizeof(char));
            if (buffer == NULL) {
                fatal("out of heap");
            }
        }

        buffer[buffer_size++] = (char) current;
        current = fgetc(stdin);
    }

    machine->datastack = value_list(value_string(buffer_size, buffer), machine->datastack);

    if (buffer_capacity > 4096) {
        buffer_capacity = 4096;
        buffer = realloc(buffer, buffer_capacity * sizeof(char));
    }
}

void machine_show(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    char *buffer = value_to_string(machine->$0);
    machine->datastack = value_list(value_string_from_string(buffer), machine->datastack);
    free(buffer);
}

static bool machine_is_escaped(char *string, word length, word position) {
    bool escaped = false;

    while (--position >= 0 && position < length && string[position] == '\\') {
        escaped = !escaped;
    }

    return escaped;
}

static void machine_escape_string(Value string, word length, char *bytes) {
    char c;

    for (word i = 0, insert = 0; i < length; ++i) {
        c = bytes[i];

        if (c == '\\' && i + 1 < length) {
            c = bytes[++i];
            switch (c) {
                case 't':
                    string->string.value[insert++] = '\t';
                    string->string.length += 1;
                    break;
                case 'a':
                    string->string.value[insert++] = '\a';
                    string->string.length += 1;
                    break;
                case 'r':
                    string->string.value[insert++] = '\r';
                    string->string.length += 1;
                    break;
                case 'n':
                    string->string.value[insert++] = '\n';
                    string->string.length += 1;
                    break;
                case 'v':
                    string->string.value[insert++] = '\v';
                    string->string.length += 1;
                    break;
                case 'f':
                    string->string.value[insert++] = '\f';
                    string->string.length += 1;
                    break;
                case '0':
                    string->string.value[insert++] = '\0';
                    string->string.length += 1;
                    break;
                case 'b':
                    string->string.value[insert++] = '\b';
                    string->string.length += 1;
                    break;
                default:
                    string->string.value[insert++] = c;
                    string->string.length += 1;
                    break;
            }
        } else {
            string->string.value[insert++] = c;
            string->string.length += 1;
        }
    }
}

void machine_tokens(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$0->type != StringValue) {
        fatal("'tokens' expects a string argument");
    }

    const word length = machine->$0->string.length;
    word position = machine->$0->string.length;
    word end;
    bool quote = false;
    bool symbol = false;

    machine->$1 = NULL;

    while (--position >= 0 && position < length) {
        const char current = machine->$0->string.value[position];

        if (is_whitespace(current)) {
            if (symbol) {
                char *nend = NULL;
                const word start = position + 1;
                double number = strtod(machine->$0->string.value + start, &nend);

                if (nend == machine->$0->string.value + end) {
                    machine->$2 = value_number(number);
                } else {
                    machine->$2 = value_create(SymbolValue, (end - start) * sizeof(char));
                    machine->$2->symbol.length = end - start;
                    memcpy(machine->$2->symbol.value, machine->$0->string.value + start, (end - start) * sizeof(char));
                }

                machine->$1 = value_list(machine->$2, machine->$1);
                machine->$2 = NULL;

                symbol = false;
            }
        } else if (!symbol && current == '"') {
            const bool escaped = machine_is_escaped(machine->$0->string.value, length, position);
            if (quote && !escaped) {
                const word start = position + 1;
                
                machine->$2 = value_create(StringValue, (end - start) * sizeof(char));
                machine->$2->string.length = 0;
                machine_escape_string(machine->$2, end - start, machine->$0->string.value + start);

                machine->$1 = value_list(machine->$2, machine->$1);
                machine->$2 = NULL;

                quote = false;
            } else if (!quote && escaped) {
                // there was no quote until now -> unterminated string
                fatal("unterminated string literal");
            } else if (!quote) {
                quote = true;
                end = position;
            }
        } else if (!quote && !symbol) {
            symbol = true;
            end = position + 1;
        }
    }

    if (quote) {
        fatal("unterminated string literal");
    } else if (symbol) {
        char *nend = NULL;
        double number = strtod(machine->$0->string.value, &nend);

        if (nend == machine->$0->string.value + end) {
            machine->$2 = value_number(number);
        } else {
            machine->$2 = value_create(SymbolValue, end * sizeof(char));
            machine->$2->symbol.length = end;
            memcpy(machine->$2->symbol.value, machine->$0->string.value, end * sizeof(char));
        }

        machine->$1 = value_list(machine->$2, machine->$1);
        machine->$2 = NULL;
    }

    machine->$0 = NULL;
    machine->datastack = value_list(machine->$1, machine->datastack);
    machine->$1 = NULL;
}

void machine_print(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("'print' expects one argument");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    } 
    
    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$0 == NULL || machine->$0->type != StringValue) {
        fatal("illegal argument for function 'print'");
    }

    for (word i = 0, end = machine->$0->string.length; i < end; ++i) {
        printf("%c", machine->$0->string.value[i]);
    }
}

void machine_invoke(Machine machine) {
    machine->$0 = machine->datastack;
    machine->datastack = value_list(machine->dictionary, NULL);
    machine->datastack = value_list(machine->callstack, machine->datastack);
    machine->datastack = value_list(machine->$0, machine->datastack);
    machine->$0 = NULL;
}

void machine_head(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    } 
    
    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$0 == NULL) {
        fatal("'head' on empty list");
    } else if (machine->$0->type != ListValue) {
        fatal("'head' expects a list argument");
    }

    machine->datastack = value_list(machine->$0->list.head, machine->datastack);
    machine->$0 = NULL;
}

void machine_split(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$0->type != NumberValue) {
        fatal("illegal argument for function 'split'");
    } else if (machine->$1->type != ListValue) {
        fatal("illegal argument for function 'split'");
    }

    word index = (word) machine->$0->number.value;
    machine->$0 = NULL;

    while (index > 0 && machine->$1 != NULL) {
        machine->$0 = value_list(machine->$1->list.head, machine->$0);
        machine->$1 = machine->$1->list.tail;
        index -= 1;
    }

    machine->$2 = NULL;
    while (machine->$0 != NULL) {
        machine->$2 = value_list(machine->$0->list.head, machine->$2);
        machine->$0 = machine->$0->list.tail;
    }

    machine->datastack = value_list(machine->$2, machine->datastack);
    machine->$2 = NULL;
    machine->datastack = value_list(machine->$1, machine->datastack);
    machine->$1 = NULL;
}

void machine_tail(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    } 
    
    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$0 == NULL) {
        fatal("'tail' on empty list");
    } else if (machine->$0->type != ListValue) {
        fatal("'tail' expects a list argument");
    }

    machine->datastack = value_list(machine->$0->list.tail, machine->datastack);
    machine->$0 = NULL;
}

void machine_nil(Machine machine) {
    machine->datastack = value_list(NULL, machine->datastack);
}

void machine_concatenate(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    } else if (machine->datastack->list.tail == NULL) {
        fatal("'concatenate' requires two arguments");
    }
    
    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;
    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->$2 = NULL;
    while (machine->$0 != NULL) {
        machine->$2 = value_list(machine->$0->list.head, machine->$2);
        machine->$0 = machine->$0->list.tail;
    }

    machine->$0 = machine->$2;
    machine->$2 = NULL;

    while (machine->$0 != NULL) {
        machine->$1 = value_list(machine->$0->list.head, machine->$1);
        machine->$0 = machine->$0->list.tail;
    }

    machine->datastack = value_list(machine->$1, machine->datastack);
    machine->$1 = NULL;
}

void machine_continue(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->datastack == NULL) {
        fatal("'continue' expects three arguments");
    }
    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->datastack == NULL) {
        fatal("'continue' expects three arguments");
    }
    machine->$2 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->datastack = machine->$0;
    machine->callstack = machine->$1;
    machine->dictionary = machine->$2;

    machine->$0 = machine->$1 = machine->$2 = NULL;
}

void machine_prepend(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("'prepend' expects two arguments");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$1 != NULL && machine->$1->type != ListValue) {
        fatal("illegal argument for function 'prepend'");
    }

    machine->$0 = value_list(machine->$0, machine->$1);
    machine->$1 = NULL;
    machine->datastack = value_list(machine->$0, machine->datastack);
    machine->$0 = NULL;
}

void machine_put(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->$2 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->$0 = value_map_put(machine->$2, machine->$1, machine->$0);
    machine->$1 = machine->$2 = NULL;
    machine->datastack = value_list(machine->$0, machine->datastack);
    machine->$0 = NULL;
}

void machine_get(Machine machine) {
    // ... map key -> value
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->$0 = value_map_get_or_else(machine->$1, machine->$0, value_marker);
    machine->$1 = NULL;

    if (machine->$0 == value_marker) {
        fatal("unknown key");
    }

    machine->datastack = value_list(machine->$0, machine->datastack);
    machine->$0 = NULL;
}

void machine_empty_map(Machine machine) {
    // ... -> map
    machine->$0 = value_map(16);
    machine->datastack = value_list(machine->$0, machine->datastack);
    machine->$0 = NULL;
}

void machine_get_or_else(Machine machine) {
    // ... map key value -> value
    if (machine->datastack == NULL) {
        fatal("'get-or-else' expects three arguments");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->datastack == NULL) {
        fatal("'get-or-else' expects three arguments");
    }

    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->datastack == NULL) {
        fatal("'get-or-else' expects three arguments");
    }

    machine->$2 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->$0 = value_map_get_or_else(machine->$2, machine->$1, machine->$0);
    machine->$1 = machine->$2;

    machine->datastack = value_list(machine->$0, machine->datastack);
    machine->$0 = NULL;
}

void machine_reverse(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("empty datastack");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->$1 = NULL;
    while (machine->$0 != NULL) {
        machine->$1 = value_list(machine->$0->list.head, machine->$1);
        machine->$0 = machine->$0->list.tail;
    }

    machine->datastack = value_list(machine->$1, machine->datastack);
    machine->$1 = NULL;
}

static void machine_binary_operation(Machine machine, char *name, double (*operator)(double, double)) {
    if (machine->datastack == NULL) {
        fatal("'%s' expects two arguments", name);
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    } else if (machine->datastack->list.tail == NULL) {
        fatal("'%s' expects two arguments", name);
    }

    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    machine->$0 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$0 == NULL || machine->$0->type != NumberValue) {
        fatal("illegal argument for function '%s'", name);
    } else if (machine->$1 == NULL || machine->$1->type != NumberValue) {
        fatal("illegal argument for function '%s'", name);
    }

    machine->$0 = value_number(operator(machine->$0->number.value, machine->$1->number.value));
    machine->$1 = NULL;
    machine->datastack = value_list(machine->$0, machine->datastack);
    machine->$0 = NULL;
}

static double machine_add_operator(double a, double b) { return a + b; }
void machine_add(Machine machine) {
    machine_binary_operation(machine, "add", machine_add_operator);
}

static double machine_sub_operator(double a, double b) { return a - b; }
void machine_sub(Machine machine) {
    machine_binary_operation(machine, "sub", machine_sub_operator);
}

static double machine_mul_operator(double a, double b) { return a * b; }
void machine_mul(Machine machine) {
    machine_binary_operation(machine, "mul", machine_mul_operator);
}

static double machine_div_operator(double a, double b) { return a / b; }
void machine_div(Machine machine) {
    machine_binary_operation(machine, "div", machine_div_operator);
}

static double machine_rem_operator(double a, double b) { return fmod(a, b); }
void machine_rem(Machine machine) {
    machine_binary_operation(machine, "rem", machine_rem_operator);
}

void machine_library(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("'library' expects one arguments");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$1 == NULL || machine->$1->type != StringValue) {
        fatal("illegal argument for function 'library'");
    }

    const word length = machine->$1->string.length;
    char *buffer = malloc((length + 1) * sizeof(char));    
    memcpy(buffer, machine->$1->string.value, length * sizeof(char));
    buffer[length] = '\0';

    void *handle = dlopen(buffer, RTLD_LAZY);

    if (handle == NULL) {
        fatal("unknown library '%s'", buffer);
    }

    // TODO: insert all native functions

}

void machine_get_directory(Machine machine) {
    char path[PATH_MAX];

    if (getcwd(path, sizeof(path)) != NULL) {
        machine->datastack = value_list(value_string_from_string(path), machine->datastack);
    } else {
        fatal("failed to get current working directory in function 'get-directory'");
    }
}

void machine_change_directory(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("'change-directory' expects one arguments");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$1 == NULL || machine->$1->type != StringValue) {
        fatal("illegal argument for function 'change-directory'");
    }

    const word length = machine->$1->string.length;
    char *buffer = malloc((length + 1) * sizeof(char));
    if (buffer == NULL) {
        fatal("out of heap");
    }
    memcpy(buffer, machine->$1->string.value, length * sizeof(char));
    buffer[length] = '\0';

    if (chdir(buffer) != 0) {
        fatal("failed to change directory to '%s' in function 'change-directory'", buffer);
    }
    free(buffer);
}

void machine_execute(Machine machine) {
    if (machine->datastack == NULL) {
        fatal("'execute' expects two arguments");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$1 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$1 == NULL || machine->$1->type != StringValue) {
        fatal("illegal argument for function 'execute'");
    }

    if (machine->datastack == NULL) {
        fatal("'execute' expects two arguments");
    } else if (machine->datastack->type != ListValue) {
        fatal("illegal datastack");
    }

    machine->$2 = machine->datastack->list.head;
    machine->datastack = machine->datastack->list.tail;

    if (machine->$2 != NULL && machine->$2->type != ListValue) {
        fatal("illegal argument for function 'execute'");
    }

    char **arguments = malloc(sizeof(char *) * ((machine->$2 == NULL ? 0 : machine->$2->list.length) + 2));
    if (arguments == NULL) {
        fatal("out of heap");
    }

    word length = machine->$1->string.length;
    char *command = malloc((length + 1) * sizeof(char));
    if (command == NULL) {
        fatal("out of heap");
    }
    memcpy(command, machine->$1->string.value, length * sizeof(char));
    command[length] = '\0';

    word index = 0;
    arguments[index++] = command;

    while (machine->$2 != NULL) {
        machine->$3 = machine->$2->list.head;

        if (machine->$3 == NULL || machine->$3->type != StringValue) {
            char *argument = value_to_string(machine->$3);
            arguments[index++] = argument;
        } else {
            length = machine->$3->string.length;
            char *argument = malloc((length + 1) * sizeof(char));
            if (argument == NULL) {
                fatal("out of heap");
            }
            memcpy(argument, machine->$3->string.value, length * sizeof(char));
            argument[length] = '\0';
            arguments[index++] = argument;
        }

        machine->$2 = machine->$2->list.tail;
    }

    arguments[index++] = NULL;
    machine->$1 = machine->$2 = machine->$3 = NULL;

    pid_t pid;
    int status;
    int link[2];

    if (pipe(link) == -1) {
        fatal("failed to execute command '%s' in function 'execute'", command);
    }

    switch (pid = fork()) {
        case -1:
            fatal("failed to execute command '%s' in function 'execute'", command);
            break;
        case 0:
            dup2(link[1], STDOUT_FILENO);
            close(link[0]);
            close(link[1]);
            execvp(command, arguments);
            fatal("failed to execute command '%s' in function 'execute'", command);
            break;
        default: 
            {
                close(link[1]);
                FILE *output = fdopen(link[0], "r");

                const word buffer_block = 4096;
                word buffer_size = 0;
                word buffer_capacity = buffer_block;
                char *buffer = malloc(buffer_capacity * sizeof(char));
                int read;

                while ((read = fread(buffer + buffer_size, sizeof(char), buffer_block, output)) == buffer_block) {
                    buffer_capacity += buffer_block;
                    buffer_size += buffer_block;
                    buffer = realloc(buffer, buffer_capacity * sizeof(char));
                }

                fclose(output);

                if (read < 0) {
                    fatal("failed to execute command '%s' in function 'execute'", command);
                }
                buffer_size += read;

                if (buffer_size >= buffer_capacity) {
                    buffer = realloc(buffer, (buffer_size + 1) * sizeof(char));
                }
                buffer[buffer_size] = '\0';

                if (waitpid(pid, &status, WUNTRACED) < 0) {
                    fatal("failed to execute command '%s' in function 'execute'", command);
                }

                while (index > 0) {
                    free(arguments[--index]);
                }

                machine->datastack = value_list(value_string_from_string(buffer), machine->datastack);
                free(buffer);
            }
            break;
    }

}
