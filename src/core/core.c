#include "core.h"

static struct lime_value lime_exception_out_of_heap_internal = {
    .type = LimeStringValue,
    .location = NULL,
    .hash = 0,
    .symbol = {
        .length = 18,
        .bytes = "out of heap memory"
    }
};

static struct lime_value lime_exception_finalization_failure_internal = {
    .type = LimeStringValue,
    .location = NULL,
    .hash = 0,
    .symbol = {
        .length = 19,
        .bytes = "finalization failed"
    }
};

static struct lime_value lime_sentinel_value_internal = {
    .type = LimeStringValue,
    .location = NULL,
    .hash = 0,
    .symbol = {
        .length = 0,
        .bytes = ""
    }
};

const LimeValue lime_exception_out_of_heap = &lime_exception_out_of_heap_internal;

const LimeValue lime_exception_finalization_failure = &lime_exception_finalization_failure_internal;

const LimeValue lime_sentinel_value = &lime_sentinel_value_internal;


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

LimeValue lime_register_function(LimeStack stack, char *name, LimeValue library, LimeFunction function) {
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);
    LimeResult result;

    result = lime_symbol(&frame, (u8*) name, strlen(name));

    if (result.failure) {
        return result.exception;
    }

    frame.registers[1] = result.value;
    result = lime_function(&frame, frame.registers[0], function);

    if (result.failure) {
        return result.exception;
    }

    result = lime_map_put(&frame, *frame.dictionary, frame.registers[1], result.value);

    if (result.failure) {
        return result.exception;
    }

    *frame.dictionary = result.value;

    return NULL;
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

LimeResult lime_map_merge(LimeStack stack, LimeValue a, LimeValue b) {
    LimeStackFrame arguments = LIME_ALLOCATE_STACK_FRAME(stack, a, b, NULL);
    LimeStackFrame variables = LIME_ALLOCATE_STACK_FRAME(&arguments, NULL, NULL, NULL);
    LimeResult result = lime_map(&variables, (u64) ((arguments.registers[0]->map.capacity + arguments.registers[1]->map.capacity) * (4.0 / 3.0)));
    
    if (result.failure) {
        return result;
    }

    arguments.registers[2] = result.value;

    // add the first map
    for (u64 i = 0; i < arguments.registers[0]->map.capacity; ++i) {
        variables.registers[0] = arguments.registers[0]->map.buckets[i];

        while (variables.registers[0] != NULL) {
            result = lime_map_put(&variables, arguments.registers[2], variables.registers[0]->list.head, variables.registers[0]->list.tail->list.head);

            if (result.failure) {
                return result;
            }
            
            arguments.registers[2] = result.value;
            variables.registers[0] = variables.registers[0]->list.tail->list.tail;
        }
    }

    // add the second map
    for (u64 i = 0; i < arguments.registers[1]->map.capacity; ++i) {
        variables.registers[0] = arguments.registers[1]->map.buckets[i];

        while (variables.registers[0] != NULL) {
            result = lime_map_put(&variables, arguments.registers[2], variables.registers[0]->list.head, variables.registers[0]->list.tail->list.head);
            
            if (result.failure) {
                return result;
            }
                
            arguments.registers[2] = result.value;
            variables.registers[0] = variables.registers[0]->list.tail->list.tail;
        }
    }

    result.value = arguments.registers[2];
    result.failure = false;

    return result;
}

static LimeResult lime_map_delete_at(LimeStack stack, LimeValue map, u64 bucket, u64 index) {
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, map, NULL, NULL);
    LimeResult result;

    result = lime_value_clone(&frame, frame.registers[0]);

    if (result.failure) {
        return result;
    }

    frame.registers[0] = result.value;

    frame.registers[1] = frame.registers[0]->map.buckets[bucket];
    u64 current = 0;
    while (frame.registers[1] != NULL) {
        if (index != current) {
            result = lime_list(&frame, frame.registers[1]->list.tail->list.head, frame.registers[2]);

            if (result.failure) {
                return result;
            }

            frame.registers[2] = result.value;
            result = lime_list(&frame, frame.registers[1]->list.head, frame.registers[2]);

            if (result.failure) {
                return result;
            }

            frame.registers[2] = result.value;
        }

        frame.registers[1] = frame.registers[1]->list.tail->list.tail;
        ++current;
    }

    frame.registers[0]->map.buckets[bucket] = frame.registers[2];
    result.value = frame.registers[0];
    result.failure = false;

    return result;
}

LimeResult lime_map_delete(LimeStack stack, LimeValue map, LimeValue key) {
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, map, key, NULL);

    const u64 index = lime_value_hash(frame.registers[1]) % frame.registers[0]->map.capacity;

    frame.registers[2] = frame.registers[0]->map.buckets[index];
    u64 current = 0;
    while (frame.registers[2] != NULL) {
        if (lime_value_equals(frame.registers[2]->list.head, frame.registers[1])) {
            return lime_map_delete_at(&frame, frame.registers[0], index, current);
        }

        frame.registers[2] = frame.registers[2]->list.tail->list.tail;
        ++current;
    }

    LimeResult result = {
        .value = frame.registers[0],
        .failure = false
    };

    return result;
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

bool lime_library_is_loaded(char *path) {
    #if defined(OS_UNIX)
        void *handle = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);
        if (handle != NULL) {
            dlclose(handle);
            return true;
        } else {
            return false;
        }
    #elif defined(OS_WINDOWS)
        return GetModuleHandle(path) != NULL;
    #else
        return false;
    #endif
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
    if (subset->map.length > superset->map.length) {
        return false;
    }

    for (u64 i = 0, end = subset->map.capacity; i < end; ++i) {
        LimeValue bucket = subset->map.buckets[i];

        while (bucket != NULL) {
            const LimeValue result = lime_map_get_or_else(superset, bucket->list.head, lime_sentinel_value);

            bucket = bucket->list.tail;

            if (result == lime_sentinel_value) {
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
            case LimeLibraryValue:
                return sizeof(struct lime_value) + value->library.length * sizeof(u8);
            case LimeMapValue:
                return sizeof(struct lime_value) + value->map.capacity * sizeof(LimeValue);
            default:
                return sizeof(struct lime_value);
        }
    }
}

void lime_value_debug(LimeValue value, char *message, ...) {
    va_list list;
    printf("[debug] ");
    fflush(stdout);

    va_start(list, message);
    vprintf(message, list);
    fflush(stdout);
    va_end(list);

    lime_value_dump(stdout, value);

    printf("\n");
    fflush(stdout);
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
        // something unexpected happened
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

char *lime_type_name(LimeValueType type) {
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

    return types[type];
}

char *lime_value_type(LimeValue value) {
    return lime_type_name(value == NULL ? LimeListValue : value->type);
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

LimeResult lime_allocate(LimeStack stack, LimeValueType type, u64 additional) {
    return gc_allocate(stack, type, additional);
}

LimeResult lime_value_clone(LimeStack stack, LimeValue value) {
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, value, NULL, NULL);
    LimeResult result;

    if (frame.registers[0] == NULL) {
        result.value = NULL;
        result.failure = false;
    } else {
        const u64 size = lime_value_byte_size(frame.registers[0]);
        const u64 additional = size - sizeof(struct lime_value);
        result = gc_allocate(&frame, frame.registers[0]->type, additional);

        if (!result.failure) {
            memcpy(result.value, frame.registers[0], size);
        }
    }

    return result;
}

LimeResult lime_list_reverse(LimeStack stack, LimeValue list) {
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, list, NULL, NULL);
    LimeResult result;

    while (frame.registers[0] != NULL) {
        result = lime_list(&frame, frame.registers[0]->list.head, frame.registers[1]);

        if (result.failure) {
            return result;
        }

        frame.registers[1] = result.value;
        frame.registers[0] = frame.registers[0]->list.tail;
    }

    result.value = frame.registers[1];
    result.failure = false;

    return result;
}

LimeResult lime_tokens(LimeStack stack, LimeValue string) {
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, string, NULL, NULL);
    LimeScanner scanner = scanner_from(frame.registers[0]->string.bytes, frame.registers[0]->string.length);
    LimeResult result;

    while (scanner_has_next(&scanner)) {
        switch (scanner_next(&scanner)) {
            case LimeErrorToken:
                result.failure = true;
                result.exception = lime_exception(&frame, "%s in line %" PRId64 " at character %" PRId64, scanner.token.error.message, scanner.token.line, scanner.token.column);
                return result;

            case LimeBooleanToken:
                result = lime_boolean(&frame, scanner.token.boolean.value);
                break;

            case LimeNumberToken:
                result = lime_number(&frame, scanner.token.number.value);
                break;

            case LimeSymbolToken:
                result = lime_allocate(&frame, LimeSymbolValue, scanner.token.symbol.length);
                
                if (!result.failure) {
                    result.value->symbol.length = scanner.token.symbol.length;
                    memcpy(
                        result.value->symbol.bytes, 
                        frame.registers[0]->symbol.bytes + scanner.token.symbol.start, 
                        scanner.token.symbol.length * sizeof(u8)
                    );
                }
                break;

            case LimeStringToken:
                result = lime_allocate(&frame, LimeStringValue, scanner.token.string.length);

                if (!result.failure) {
                    const u64 length = scanner.token.string.length;
                    const u64 start = scanner.token.string.start;
                    u8 *const bytes = &frame.registers[0]->symbol.bytes[0];
                    u8 *const dst = &result.value->string.bytes[0];

                    register bool escaped = false;
                    register u64 p = 0;
                    for (register u64 i = 0; i < length; ++i) {
                        const char current = bytes[start + i];
                        if (escaped) {
                            escaped = false;
                            dst[p++] = unescape(current);
                        } else if (current == '\\') {
                            escaped = true;
                        } else {
                            dst[p++] = current;
                        }
                    }

                    result.value->string.length = p;
                }

                break;
        }

        if (result.failure) {
            return result;
        }

        result = lime_list(&frame, result.value, frame.registers[1]);

        if (result.failure) {
            return result;
        }

        frame.registers[1] = result.value;
        scanner.bytes = frame.registers[0]->string.bytes;
    }

    return lime_list_reverse(&frame, frame.registers[1]);
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
        result.value->function.library = frame.registers[0];
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
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, NULL, NULL, NULL);
    LimeResult result;
    LimeLibraryHandle handle;
    bool already_loaded;
    const u64 length = strlen(path);

    result = gc_allocate(&frame, LimeLibraryValue, length);

    if (result.failure) {
        return result;
    }

    frame.registers[0] = result.value;
    frame.registers[0]->library.handle = NULL;
    frame.registers[0]->library.length = length;
    memcpy(frame.registers[0]->library.bytes, (u8*) path, length * sizeof(u8));
    result = gc_add_library(&frame, frame.registers[0]);

    if (result.failure) {
        return result;
    }

    result.value = frame.registers[0];
    result.failure = false;

    return result;
}

LimeResult lime_boolean(LimeStack stack, bool value) {
    LimeResult result = gc_allocate(stack, LimeBooleanValue, 0);
    
    if (!result.failure) {
        result.value->boolean.value = value;
    }

    return result;
}

