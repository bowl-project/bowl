#include "core.h"

static LimeResult lime_map_insert(LimeStack stack, LimeValue bucket, LimeValue key, LimeValue value) {
    LimeStackFrame arguments = LIME_ALLOCATE_STACK_FRAME(stack, bucket, key, value);
    LimeStackFrame variables = LIME_ALLOCATE_STACK_FRAME(&arguments, NULL, NULL, NULL);
    LimeResult result;
    bool found = false;

    variables.registers[0] = NULL;
    while (arguments.registers[0] != NULL) {
        if (!found && lime_value_equals(arguments.registers[1], arguments.registers[0]->list.head)) {
            result = lime_list(&variables, arguments.registers[2], variables.registers[0]);

            if (result.failure) {
                return result;
            }

            variables.registers[0] = result.value;
            result = lime_list(&variables, arguments.registers[1], variables.registers[0]);

            if (result.failure) {
                return result;
            }

            variables.registers[0] = result.value;
            found = true;
        } else {
            result = lime_list(&variables, arguments.registers[0]->list.tail->list.head, variables.registers[0]);

            if (result.failure) {
                return result;
            }

            variables.registers[0] = result.value;
            result = lime_list(&variables, arguments.registers[0]->list.head, variables.registers[0]);

            if (result.failure) {
                return result;
            }

            variables.registers[0] = result.value;
        }

        arguments.registers[0] = arguments.registers[0]->list.tail->list.tail;
    }

    if (!found) {
        result = lime_list(&variables, arguments.registers[2], variables.registers[0]);

        if (result.failure) {
            return result;
        }

        variables.registers[0] = result.value;
        result = lime_list(&variables, arguments.registers[1], variables.registers[0]);

        if (result.failure) {
            return result;
        }

        variables.registers[0] = result.value;
    }

    result.value = variables.registers[0];
    return result;
}

LimeValue lime_map_get_or_else(LimeValue map, LimeValue key, LimeValue otherwise) {
    const u64 index = lime_value_hash(key) % map->map.capacity;

    LimeValue bucket = map->map.buckets[index];

    while (bucket != NULL) {
        if (lime_value_equals(key, bucket->list.head)) {
            return bucket->list.tail->list.head;
        }

        bucket = bucket->list.tail->list.tail;
    }
    
    return otherwise;
}

LimeResult lime_map_put(LimeStack stack, LimeValue map, LimeValue key, LimeValue value) {
    static const double load_factor = 0.75;

    LimeStackFrame arguments = LIME_ALLOCATE_STACK_FRAME(stack, map, key, value);
    LimeStackFrame variables = LIME_ALLOCATE_STACK_FRAME(&arguments, NULL, NULL, NULL);
    LimeResult result;

    // resize capacity if it exceeds the load factor
    u64 capacity = arguments.registers[0]->map.capacity;
    if (arguments.registers[0]->map.length + 1 >= capacity * load_factor) {
        capacity = MAX(capacity * 2, (arguments.registers[0]->map.length + 1) * 2);
    }

    // copy the buckets
    result = lime_map(&variables, capacity);
    if (result.failure) {
        return result;
    }

    variables.registers[0] = result.value;
    variables.registers[0]->map.length = arguments.registers[0]->map.length;
    if (capacity != arguments.registers[0]->map.capacity) {
        for (u64 i = 0; i < arguments.registers[0]->map.capacity; ++i) {
            variables.registers[1] = arguments.registers[0]->map.buckets[i];
            while (variables.registers[1] != NULL) {
                variables.registers[2] = variables.registers[1]->list.head;
                variables.registers[1] = variables.registers[1]->list.tail;

                const u64 index = lime_value_hash(variables.registers[2]) % capacity;
                result = lime_map_insert(
                    &variables,
                    variables.registers[0]->map.buckets[index],
                    variables.registers[2],
                    variables.registers[1]->list.head
                );

                if (result.failure) {
                    return result;
                }

                variables.registers[0]->map.buckets[index] = result.value;
                variables.registers[1] = variables.registers[1]->list.tail;
            }
        }
    } else {
        for (u64 i = 0; i < capacity; ++i) {
            variables.registers[0]->map.buckets[i] = arguments.registers[0]->map.buckets[i];
        }
    }

    // insert the new key-value pair
    const u64 index = lime_value_hash(arguments.registers[1]) % capacity;
    const u64 length = lime_value_length(variables.registers[0]->map.buckets[index]);
    
    result = lime_map_insert(
        &variables, 
        variables.registers[0]->map.buckets[index], 
        arguments.registers[1], 
        arguments.registers[2]
    );

    if (result.failure) {
        return result;
    }

    variables.registers[0]->map.buckets[index] = result.value;

    if (lime_value_length(result.value) > length) {
        variables.registers[0]->map.length += 1;
    }

    result.value = variables.registers[0];
    return result;
}

u64 lime_value_hash(LimeValue value) {
    if (value == NULL) {
        return 31;
    } else if (value->hash == 0) {
        value->hash = 31;

        switch (value->type) {
            case LimeSymbolValue:
                for (u64 i = 0, end = value->symbol.length; i < end; ++i) {
                    value->hash += pow(value->symbol.bytes[i] * 31, end - (i + 1));
                }
                break;

            case LimeNumberValue:
                value->hash += *((u64 *) (&value->number.value)) * 31;
                break;

            case LimeBooleanValue:
                value->hash = value->boolean.value ? 7 : 31;
                break;

            case LimeStringValue:
                for (u64 i = 0, end = value->string.length; i < end; ++i) {
                    value->hash += pow(value->string.bytes[i] * 31, end - (i + 1));
                }
                break;

            case LimeNativeValue:
                value->hash += (u64) value->function.function * 31;
                break;

            case LimeLibraryValue:
                value->hash += (u64) value->library.handle * 31;
                break;

            case LimeListValue:
                value->hash += lime_value_hash(value->list.head) * 31;
                value->hash += lime_value_hash(value->list.tail) * 31;
                break;

            case LimeMapValue:
                for (u64 i = 0, end = value->map.capacity; i < end; ++i) {
                    value->hash += lime_value_hash(value->map.buckets[i]) * 31;
                }
                break;
        }
    }

    return value->hash;
}

bool lime_value_equals(LimeValue a, LimeValue b) {
    if (a == b) {
        return true;
    } else if (a == NULL || b == NULL) {
        return false;
    } else if (a->type != b->type) {
        return false;
    } else if (a->hash != 0 && b->hash != 0 && a->hash != b->hash) {
        return false;
    } else {
        switch (a->type) {
            case LimeSymbolValue:
                if (a->symbol.length != b->symbol.length) {
                    return false;
                }

                for (u64 i = 0, end = a->symbol.length; i < end; ++i) {
                    if (a->symbol.bytes[i] != b->symbol.bytes[i]) {
                        return false;
                    }
                }

                return true;

            case LimeNumberValue:
                return a->number.value == b->number.value;

            case LimeBooleanValue:
                return a->boolean.value == b->boolean.value;

            case LimeStringValue:
                if (a->string.length != b->string.length) {
                    return false;
                }

                for (u64 i = 0, end = a->string.length; i < end; ++i) {
                    if (a->string.bytes[i] != b->string.bytes[i]) {
                        return false;
                    }
                }

                return true;

            case LimeNativeValue:
                return a->function.function == b->function.function;

            case LimeLibraryValue:
                return a->library.handle == b->library.handle;

            case LimeListValue:
                if (a->list.length != b->list.length) {
                    return false;
                }

                do {
                    if (!lime_value_equals(a->list.head, b->list.head)) {
                        return false;
                    }

                    a = a->list.tail;
                    b = b->list.tail;
                } while (a != NULL);

                return true;

            case LimeMapValue:
                if (a->map.length != b->map.length) {
                    return false;
                }

                return lime_map_subset_of(a, b) && lime_map_subset_of(b, a);
        }
    }
}

bool lime_map_subset_of(LimeValue superset, LimeValue subset) {
    static struct lime_value marker = {
        .type = LimeSymbolValue,
        .location = NULL,
        .hash = 31,
        .symbol = {
            .length = 0
        }
    };

    if (subset->map.length > superset->map.length) {
        return false;
    }

    for (u64 i = 0, end = subset->map.capacity; i < end; ++i) {
        LimeValue bucket = subset->map.buckets[i];

        while (bucket != NULL) {
            const LimeValue result = lime_map_get_or_else(superset, bucket->list.head, &marker);

            bucket = bucket->list.tail;

            if (result == &marker) {
                return false;
            } else if (!lime_value_equals(bucket->list.head, result)) {
                return false;
            }

            bucket = bucket->list.tail;
        }
    }

    return true;
}

u64 lime_value_byte_size(LimeValue value) {
    if (value == NULL) {
        return 0;
    } else {
        switch (value->type) {
            case LimeSymbolValue:
                return sizeof(struct lime_value) + value->symbol.length * sizeof(u8);
            case LimeStringValue:
                return sizeof(struct lime_value) + value->string.length * sizeof(u8);
            case LimeMapValue:
                return sizeof(struct lime_value) + value->map.capacity * sizeof(LimeValue);
            default:
                return sizeof(struct lime_value);
        }
    }
}

void lime_value_dump(FILE *stream, LimeValue value) {
    if (value == NULL) {
        fprintf(stream, "[ ]");
    } else {
        switch (value->type) {
            case LimeSymbolValue:
                for (u64 i = 0, end = value->symbol.length; i < end; ++i) {
                    fprintf(stream, "%c", value->symbol.bytes[i]);
                }
                break;

            case LimeNumberValue:
                if (is_integer(value->number.value)) {
                    fprintf(stream, "%" PRId64, (u64) value->number.value);
                } else {
                    fprintf(stream, "%f", value->number.value);
                }
                break;
            
            case LimeBooleanValue:
                if (value->boolean.value) {
                    fprintf(stream, "true");
                } else {
                    fprintf(stream, "false");
                }
                break;

            case LimeNativeValue:
                fprintf(stream, "function#0x%08" PRIX64, (u64) value->function.function);
                break;

            case LimeLibraryValue:
                fprintf(stream, "library#0x%08" PRIX64, (u64) value->library.handle);
                break;

            case LimeListValue:
                fprintf(stream, "[ ");

                do {
                    lime_value_dump(stream, value->list.head);
                    value = value->list.tail;
                    if (value != NULL) {
                        fprintf(stream, " ");
                    }
                } while (value != NULL);

                fprintf(stream, " ]");
                break;

            case LimeStringValue:
                fprintf(stream, "\"");

                for (u64 i = 0, end = value->string.length; i < end; ++i) {
                    char const* sequence = escape(value->string.bytes[i]);
                    if (sequence != NULL) {
                        fprintf(stream, "%s", sequence);
                    } else {
                        fprintf(stream, "%c", value->string.bytes[i]);
                    }
                }

                fprintf(stream, "\"");
                break;

            case LimeMapValue:
                fprintf(stream, "[ ");

                bool first = true;
                for (u64 i = 0, end = value->map.capacity; i < end; ++i) {
                    LimeValue bucket = value->map.buckets[i];

                    if (!first && bucket != NULL) {
                        fprintf(stream, " ");
                    } else if (bucket != NULL) {
                        first = false;
                    }

                    while (bucket != NULL) {
                        fprintf(stream, "[ ");
                        lime_value_dump(stream, bucket->list.head);
                        bucket = bucket->list.tail;
                        fprintf(stream, " ");
                        lime_value_dump(stream, bucket->list.head);
                        bucket = bucket->list.tail;
                        fprintf(stream, " ]");

                        if (bucket != NULL) {
                            fprintf(stream, " ");
                        }
                    }
                }

                if (first) {
                    fprintf(stream, "] map-from-list");
                } else {
                    fprintf(stream, " ] map-from-list");
                }
                break;
        }
    }
}

static bool lime_value_printf_buffer(char **buffer, u64 *length, u64 *capacity, char *message, ...) {
    va_list list;

    va_start(list, message);
    u64 required = vsnprintf(NULL, 0, message, list);
    va_end(list);
    
    if (required < 0) {
        free(*buffer);
        *buffer = NULL;
        *length = 0;
        *capacity = 0;
        return false;
    }

    required += 1;

    if (required > *capacity - *length) {
        *capacity = MAX(*capacity * 2, *capacity + (required - (*capacity - *length)));

        char *const new_buffer = realloc(*buffer, *capacity * sizeof(char));
        if (new_buffer == NULL) {
            free(*buffer);
            *buffer = NULL;
            *length = 0;
            *capacity = 0;
            return false;
        }

        *buffer = new_buffer;
    }

    va_start(list, message);
    required = vsnprintf(*buffer + *length, *capacity - *length, message, list);
    va_end(list);

    if (required < *capacity - *length && required >= 0) {
        *length += required;
        return true;
    } else {
        // something unexpected happen
        free(*buffer);
        *buffer = NULL;
        *length = 0;
        *capacity = 0;
        return false;
    }
}

static void lime_value_show_buffer(LimeValue value, char **buffer, u64 *length, u64 *capacity) {
    if (value == NULL) {
        lime_value_printf_buffer(buffer, length, capacity, "[ ]");
    } else {
        switch (value->type) {
            case LimeSymbolValue:
                for (u64 i = 0, end = value->symbol.length; i < end; ++i) {
                    if (!lime_value_printf_buffer(buffer, length, capacity, "%c", value->symbol.bytes[i])) {
                        return;
                    }
                }
                break;

            case LimeNumberValue:
                if (is_integer(value->number.value)) {
                    if (!lime_value_printf_buffer(buffer, length, capacity, "%" PRId64, (u64) value->number.value)) {
                        return;
                    }
                } else {
                    if (!lime_value_printf_buffer(buffer, length, capacity, "%f", value->number.value)) {
                        return;
                    }
                }
                break;
            
            case LimeBooleanValue:
                if (value->boolean.value) {
                    if (!lime_value_printf_buffer(buffer, length, capacity, "true")) {
                        return;
                    }
                } else {
                    if (!lime_value_printf_buffer(buffer, length, capacity, "false")) {
                        return;
                    }
                }
                break;

            case LimeNativeValue:
                if (!lime_value_printf_buffer(buffer, length, capacity, "function#0x%08" PRIX64, (u64) value->function.function)) {
                    return;
                }
                break;

            case LimeLibraryValue:
                if (!lime_value_printf_buffer(buffer, length, capacity, "library#0x%08" PRIX64, (u64) value->library.handle)) {
                    return;
                }
                break;

            case LimeListValue:
                if (!lime_value_printf_buffer(buffer, length, capacity, "[ ")) {
                    return;
                }

                do {
                    lime_value_show_buffer(value->list.head, buffer, length, capacity);
                    if (*buffer == NULL) {
                        return;
                    }
                    value = value->list.tail;
                    if (value != NULL) {
                        if (!lime_value_printf_buffer(buffer, length, capacity, " ")) {
                            return;
                        }
                    }
                } while (value != NULL);

                if (!lime_value_printf_buffer(buffer, length, capacity, " ]")) {
                    return;
                }
                break;

            case LimeStringValue:
                if (!lime_value_printf_buffer(buffer, length, capacity, "\"")) {
                    return;
                }

                for (u64 i = 0, end = value->string.length; i < end; ++i) {
                    char const* sequence = escape(value->string.bytes[i]);
                    if (sequence != NULL) {
                        if (!lime_value_printf_buffer(buffer, length, capacity, "%s", sequence)) {
                            return;
                        }
                    } else {
                        if (!lime_value_printf_buffer(buffer, length, capacity, "%c", value->string.bytes[i])) {
                            return;
                        }
                    }
                }

                if (!lime_value_printf_buffer(buffer, length, capacity, "\"")) {
                    return;
                }
                break;

            case LimeMapValue:
                if (!lime_value_printf_buffer(buffer, length, capacity, "[ ")) {
                    return;
                }

                bool first = true;
                for (u64 i = 0, end = value->map.capacity; i < end; ++i) {
                    LimeValue bucket = value->map.buckets[i];

                    if (!first && bucket != NULL) {
                        if (!lime_value_printf_buffer(buffer, length, capacity, " ")) {
                            return;
                        }
                    } else if (bucket != NULL) {
                        first = false;
                    }

                    while (bucket != NULL) {
                        if (!lime_value_printf_buffer(buffer, length, capacity, "[ ")) {
                            return;
                        }

                        lime_value_show_buffer(bucket->list.head, buffer, length, capacity);
                        if (*buffer == NULL) {
                            return;
                        }

                        bucket = bucket->list.tail;
                        if (!lime_value_printf_buffer(buffer, length, capacity, " ")) {
                            return;
                        }
                        
                        lime_value_show_buffer(bucket->list.head, buffer, length, capacity);
                        if (*buffer == NULL) {
                            return;
                        }

                        bucket = bucket->list.tail;
                        if (!lime_value_printf_buffer(buffer, length, capacity, " ]")) {
                            return;
                        }

                        if (bucket != NULL) {
                            if (!lime_value_printf_buffer(buffer, length, capacity, " ")) {
                                return;
                            }
                        }
                    }
                }

                if (first) {
                    if (!lime_value_printf_buffer(buffer, length, capacity, "] map-from-list")) {
                        return;
                    }
                } else {
                    if (!lime_value_printf_buffer(buffer, length, capacity, " ] map-from-list")) {
                        return;
                    }
                }
                break;
        }
    }
}

void lime_value_show(LimeValue value, char **buffer, u64 *length) {
    u64 capacity = 4096;
    *length = 0;
    *buffer = malloc(capacity * sizeof(char));
    if (*buffer != NULL) {
        lime_value_show_buffer(value, buffer, length, &capacity);
    }
}

u64 lime_value_length(LimeValue value) {
    if (value == NULL) {
        return 0;
    } else {
        switch (value->type) {
            case LimeSymbolValue:
                return value->symbol.length;
            case LimeListValue:
                return value->list.length;
            case LimeMapValue:
                return value->map.length;
            case LimeStringValue: 
                return value->string.length;
            default: 
                return 0;
        }
    }
}

char *lime_value_type(LimeValue value) {
    static char *types[] = {
        [LimeSymbolValue]  = "symbol",
        [LimeListValue]    = "list",
        [LimeNativeValue]  = "function",
        [LimeMapValue]     = "map",
        [LimeBooleanValue] = "boolean",
        [LimeNumberValue]  = "number",
        [LimeStringValue]  = "string",
        [LimeLibraryValue] = "library"
    };
    
    if (value == NULL) {
        return types[LimeListValue];
    } else {
        return types[value->type];
    }
}

char *lime_string_to_null_terminated(LimeValue value) {
    char *path = malloc(value->string.length + 1);
    
    if (path != NULL) {
        memcpy(path, value->string.bytes, value->string.length);
        path[value->string.length] = '\0';    
    }

    return path;
}

LimeValue lime_exception(LimeStack stack, char *message, ...) {
    static struct lime_value exception = {
        .type = LimeStringValue,
        .location = NULL,
        .hash = 0,
        .string = {
            .length = 63,
            .bytes = "failed to format exception message in function 'lime_exception'"
        }
    };
    va_list list;

    va_start(list, message);
    const u64 required = vsnprintf(NULL, 0, message, list);
    va_end(list);

    LimeResult result = gc_allocate(stack, LimeStringValue, (required + 1) * sizeof(u8));

    if (!result.failure) {
        result.value->string.length = required;
        va_start(list, message);
        const u64 written = vsnprintf(&result.value->string.bytes[0], required + 1, message, list);
        va_end(list);

        if (written < 0 || written >= required + 1) {
            return &exception;
        } else {
            return result.value;
        }
    } else {
        return result.exception;
    }
}

LimeResult lime_symbol(LimeStack stack, u8 *bytes, u64 length) {
    LimeResult result = gc_allocate(stack, LimeSymbolValue, length * sizeof(u8));

    if (!result.failure) {
        result.value->symbol.length = length;
        memcpy(result.value->symbol.bytes, bytes, length * sizeof(u8));
        lime_value_hash(result.value);    
    }

    return result;
}

LimeResult lime_string(LimeStack stack, u8 *bytes, u64 length) {
    LimeResult result = gc_allocate(stack, LimeStringValue, length * sizeof(u8));

    if (!result.failure) {
        result.value->string.length = length;
        memcpy(result.value->string.bytes, bytes, length * sizeof(u8));
    }

    return result;
}

LimeResult lime_function(LimeStack stack, LimeValue library, LimeFunction function) {
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);
    LimeResult result = gc_allocate(&frame, LimeNativeValue, 0);
    
    if (!result.failure) {
        result.value->function.library = library;
        result.value->function.function = function;
    }

    return result;
}

LimeResult lime_list(LimeStack stack, LimeValue head, LimeValue tail) {
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, head, tail, NULL);    
    LimeResult result = gc_allocate(&frame, LimeListValue, 0);

    if (!result.failure) {
        result.value->list.head = frame.registers[0];
        result.value->list.tail = frame.registers[1];

        if (frame.registers[1] == NULL) {
            result.value->list.length = 1;
        } else {
            result.value->list.length = frame.registers[1]->list.length + 1;
        }
    }

    return result;
}

LimeResult lime_map(LimeStack stack, u64 capacity) {
    LimeResult result = gc_allocate(stack, LimeMapValue, capacity * sizeof(LimeValue));

    if (!result.failure) {
        result.value->map.capacity = capacity;
        result.value->map.length = 0;
        
        for (u64 i = 0; i < capacity; ++i) {
            result.value->map.buckets[i] = NULL;
        }
    }
    
    return result;
}

LimeResult lime_number(LimeStack stack, double value) {
    LimeResult result = gc_allocate(stack, LimeNumberValue, 0);

    if (!result.failure) {
        result.value->number.value = value;
    }
    
    return result;
}

LimeResult lime_library(LimeStack stack, char *path) {
    LimeResult result;

    result = gc_allocate(stack, LimeLibraryValue, 0);
    if (result.failure) {
        return result;
    }

    // It is important to add the library to the list of weak references before any
    // action is performed that may require us to close the handle again (e.g. in
    // case of an exception when calling the 'lime_module_initialize' function).
    // By adding the library value to the list of weak references the garbage collector
    // is able to close the library automatically if it is no longer needed. 
    result = gc_add_library(result.value);
    if (result.failure) {
        return result;
    }

    #if defined(OS_UNIX)
        result.value->library.handle = dlopen(path, RTLD_LAZY);
    #elif defined(OS_WINDOWS)
        result.value->library.handle = LoadLibrary(path);
    #else
        // setting the handle to 'NULL' here triggers the error in the next line, which is 
        // desired because native libraries are not supported on this platform.
        result.value->library.handle = NULL;
    #endif

    if (result.value->library.handle == NULL) {
        result.exception = lime_exception(stack, "failed to load library '%s' in function '%s'", path, __FUNCTION__);
        result.failure = true;
        return result;
    }

    #if defined(OS_UNIX)
        const LimeFunction initialize = (LimeFunction) dlsym(result.value->library.handle, "lime_module_initialize");
    #elif defined(OS_WINDOWS)
        const LimeFunction initialize = (LimeFunction) GetProcAddress(result.value->library.handle, "lime_module_initialize");
    #else 
        const LimeFunction initialize = NULL;
    #endif
  
    if (initialize == NULL) {
        // the library handle is closed by the garbage collector
        result.exception = lime_exception(stack, "failed to load library '%s' in function '%s'", path, __FUNCTION__);
        result.failure = true;
        return result;
    }

    LimeValue exception = initialize(stack);
    if (exception != NULL) {
        result.exception = exception;
        result.failure = true;
        return result;
    }

    return result;
}

LimeResult lime_boolean(LimeStack stack, bool value) {
    LimeResult result = gc_allocate(stack, LimeBooleanValue, 0);
    
    if (!result.failure) {
        result.value->boolean.value = value;
    }

    return result;
}

