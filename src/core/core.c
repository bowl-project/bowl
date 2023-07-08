#include "core.h"

BOWL_STATIC_STRING(bowl_exception_out_of_heap_message, "out of heap memory");
BOWL_STATIC_STRING(bowl_exception_finalization_failure_message, "finalization failed");
BOWL_STATIC_STRING(bowl_exception_malformed_utf8_message, "malformed UTF-8 sequence");
BOWL_STATIC_STRING(bowl_exception_incomplete_utf8_message, "incomplete UTF-8 sequence");
BOWL_STATIC_STRING(bowl_sentinel_value_internal, "");

static struct bowl_value bowl_exception_out_of_heap_value = {
    .type = BowlExceptionValue,
    .location = NULL,
    .hash = 0,
    .exception = {
        .cause = NULL,
        .message = &bowl_exception_out_of_heap_message.value
    }
};

static struct bowl_value bowl_exception_finalization_failure_value = {
    .type = BowlExceptionValue,
    .location = NULL,
    .hash = 0,
    .exception = {
        .cause = NULL,
        .message = &bowl_exception_finalization_failure_message.value
    }
};

static struct bowl_value bowl_exception_malformed_utf8_value = {
    .type = BowlExceptionValue,
    .location = NULL,
    .hash = 0,
    .exception = {
        .cause = NULL,
        .message = &bowl_exception_malformed_utf8_message.value
    }
};

static struct bowl_value bowl_exception_incomplete_utf8_value = {
    .type = BowlExceptionValue,
    .location = NULL,
    .hash = 0,
    .exception = {
        .cause = NULL,
        .message = &bowl_exception_incomplete_utf8_message.value
    }
};

const BowlValue bowl_exception_out_of_heap = &bowl_exception_out_of_heap_value;
const BowlValue bowl_exception_finalization_failure = &bowl_exception_finalization_failure_value;
const BowlValue bowl_exception_malformed_utf8 = &bowl_exception_malformed_utf8_value;
const BowlValue bowl_exception_incomplete_utf8 = &bowl_exception_incomplete_utf8_value;
const BowlValue bowl_sentinel_value = &bowl_sentinel_value_internal.value;

static BowlResult bowl_map_insert(BowlStack stack, BowlValue bucket, BowlValue key, BowlValue value) {
    BowlStackFrame arguments = BOWL_ALLOCATE_STACK_FRAME(stack, bucket, key, value);
    BowlStackFrame variables = BOWL_ALLOCATE_STACK_FRAME(&arguments, NULL, NULL, NULL);
    BowlResult result;
    bool found = false;

    variables.registers[0] = NULL;
    while (arguments.registers[0] != NULL) {
        if (!found && bowl_value_equals(arguments.registers[1], arguments.registers[0]->list.head)) {
            result = bowl_list(&variables, arguments.registers[2], variables.registers[0]);

            if (result.failure) {
                return result;
            }

            variables.registers[0] = result.value;
            result = bowl_list(&variables, arguments.registers[1], variables.registers[0]);

            if (result.failure) {
                return result;
            }

            variables.registers[0] = result.value;
            found = true;
        } else {
            result = bowl_list(&variables, arguments.registers[0]->list.tail->list.head, variables.registers[0]);

            if (result.failure) {
                return result;
            }

            variables.registers[0] = result.value;
            result = bowl_list(&variables, arguments.registers[0]->list.head, variables.registers[0]);

            if (result.failure) {
                return result;
            }

            variables.registers[0] = result.value;
        }

        arguments.registers[0] = arguments.registers[0]->list.tail->list.tail;
    }

    if (!found) {
        result = bowl_list(&variables, arguments.registers[2], variables.registers[0]);

        if (result.failure) {
            return result;
        }

        variables.registers[0] = result.value;
        result = bowl_list(&variables, arguments.registers[1], variables.registers[0]);

        if (result.failure) {
            return result;
        }

        variables.registers[0] = result.value;
    }

    result.value = variables.registers[0];
    return result;
}

BowlValue bowl_register_function(BowlStack stack, char *name, char *documentation, BowlValue library, BowlFunction function) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);

    BOWL_TRY(&frame.registers[1], bowl_string(&frame, (u8*) documentation, strlen(documentation)));
    BOWL_TRY(&frame.registers[1], bowl_list(&frame, frame.registers[1], NULL));
    BOWL_TRY(&frame.registers[2], bowl_function(&frame, frame.registers[0], function));
    BOWL_TRY(&frame.registers[1], bowl_list(&frame, frame.registers[2], frame.registers[1]));

    BOWL_TRY(&frame.registers[2], bowl_symbol(&frame, (u8*) name, strlen(name)));
    BOWL_TRY(frame.dictionary, bowl_map_put(&frame, *frame.dictionary, frame.registers[2], frame.registers[1]));

    return NULL;
}

BowlValue bowl_register(BowlStack stack, BowlValue library, BowlFunctionEntry entry) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);
    return bowl_register_function(&frame, entry.name, entry.documentation, frame.registers[0], entry.function);
}

BowlValue bowl_register_all(BowlStack stack, BowlValue library, BowlFunctionEntry entries[], u64 entries_length) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);
    
    for (u64 i = 0; i < entries_length; ++i) {
        const BowlValue exception = bowl_register(&frame, frame.registers[0], entries[i]);
        if (exception != NULL) {
            return exception;
        }
    }

    return NULL;
}


BowlValue bowl_map_get_or_else(BowlValue map, BowlValue key, BowlValue otherwise) {
    const u64 index = bowl_value_hash(key) % map->map.capacity;
    BowlValue bucket = map->map.buckets[index];

    while (bucket != NULL) {
        if (bowl_value_equals(key, bucket->list.head)) {
            return bucket->list.tail->list.head;
        }

        bucket = bucket->list.tail->list.tail;
    }
    
    return otherwise;
}

BowlResult bowl_map_merge(BowlStack stack, BowlValue a, BowlValue b) {
    BowlStackFrame arguments = BOWL_ALLOCATE_STACK_FRAME(stack, a, b, NULL);
    BowlStackFrame variables = BOWL_ALLOCATE_STACK_FRAME(&arguments, NULL, NULL, NULL);
    BowlResult result = bowl_map(&variables, (u64) ((arguments.registers[0]->map.capacity + arguments.registers[1]->map.capacity) * (4.0 / 3.0)));
    
    if (result.failure) {
        return result;
    }

    arguments.registers[2] = result.value;

    // add the first map
    for (u64 i = 0; i < arguments.registers[0]->map.capacity; ++i) {
        variables.registers[0] = arguments.registers[0]->map.buckets[i];

        while (variables.registers[0] != NULL) {
            result = bowl_map_put(&variables, arguments.registers[2], variables.registers[0]->list.head, variables.registers[0]->list.tail->list.head);

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
            result = bowl_map_put(&variables, arguments.registers[2], variables.registers[0]->list.head, variables.registers[0]->list.tail->list.head);
            
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

static BowlResult bowl_map_delete_at(BowlStack stack, BowlValue map, u64 bucket, u64 index) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, map, NULL, NULL);
    BowlResult result;

    result = bowl_value_clone(&frame, frame.registers[0]);

    if (result.failure) {
        return result;
    }

    frame.registers[0] = result.value;

    frame.registers[1] = frame.registers[0]->map.buckets[bucket];
    u64 current = 0;
    while (frame.registers[1] != NULL) {
        if (index != current) {
            result = bowl_list(&frame, frame.registers[1]->list.tail->list.head, frame.registers[2]);

            if (result.failure) {
                return result;
            }

            frame.registers[2] = result.value;
            result = bowl_list(&frame, frame.registers[1]->list.head, frame.registers[2]);

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

BowlResult bowl_map_delete(BowlStack stack, BowlValue map, BowlValue key) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, map, key, NULL);

    const u64 index = bowl_value_hash(frame.registers[1]) % frame.registers[0]->map.capacity;

    frame.registers[2] = frame.registers[0]->map.buckets[index];
    u64 current = 0;
    while (frame.registers[2] != NULL) {
        if (bowl_value_equals(frame.registers[2]->list.head, frame.registers[1])) {
            return bowl_map_delete_at(&frame, frame.registers[0], index, current);
        }

        frame.registers[2] = frame.registers[2]->list.tail->list.tail;
        ++current;
    }

    BowlResult result = {
        .value = frame.registers[0],
        .failure = false
    };

    return result;
}

BowlResult bowl_map_put(BowlStack stack, BowlValue map, BowlValue key, BowlValue value) {
    static const double load_factor = 0.75;

    BowlStackFrame arguments = BOWL_ALLOCATE_STACK_FRAME(stack, map, key, value);
    BowlStackFrame variables = BOWL_ALLOCATE_STACK_FRAME(&arguments, NULL, NULL, NULL);
    BowlResult result;

    // resize capacity if it exceeds the load factor
    u64 capacity = arguments.registers[0]->map.capacity;
    if (arguments.registers[0]->map.length + 1 >= capacity * load_factor) {
        capacity = MAX(capacity * 2, (arguments.registers[0]->map.length + 1) * 2);
    }

    // copy the buckets
    result = bowl_map(&variables, capacity);
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

                const u64 index = bowl_value_hash(variables.registers[2]) % capacity;
                result = bowl_map_insert(
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
    const u64 index = bowl_value_hash(arguments.registers[1]) % capacity;
    const u64 length = bowl_value_length(variables.registers[0]->map.buckets[index]);

    result = bowl_map_insert(
        &variables, 
        variables.registers[0]->map.buckets[index], 
        arguments.registers[1], 
        arguments.registers[2]
    );

    if (result.failure) {
        return result;
    }

    variables.registers[0]->map.buckets[index] = result.value;

    if (bowl_value_length(result.value) > length) {
        variables.registers[0]->map.length += 1;
    }

    result.value = variables.registers[0];

    return result;
}

bool bowl_library_is_loaded(char *path) {
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

u64 bowl_value_hash(BowlValue value) {
    if (value == NULL) {
        return 31;
    } else if (value->hash == 0) {
        value->hash = 31;

        switch (value->type) {
            case BowlSymbolValue:
                for (u64 i = 0, end = value->symbol.length; i < end; ++i) {
                    value->hash += pow(value->symbol.bytes[i] * 31, end - (i + 1));
                }
                break;

            case BowlNumberValue:
                value->hash += *((u64 *) (&value->number.value)) * 31;
                break;

            case BowlBooleanValue:
                value->hash = value->boolean.value ? 7 : 31;
                break;

            case BowlStringValue:
                for (u64 i = 0, end = value->string.size; i < end; ++i) {
                    value->hash += pow(value->string.bytes[i] * 31, end - (i + 1));
                }
                break;

            case BowlNativeValue:
                value->hash += (u64) value->function.function * 31;
                break;

            case BowlLibraryValue:
                value->hash += (u64) value->library.handle * 31;
                break;

            case BowlListValue:
                value->hash += bowl_value_hash(value->list.head) * 31;
                value->hash += bowl_value_hash(value->list.tail) * 31;
                break;

            case BowlMapValue:
                for (u64 i = 0, end = value->map.capacity; i < end; ++i) {
                    value->hash += bowl_value_hash(value->map.buckets[i]) * 31;
                }
                break;

            case BowlVectorValue:
                for (u64 i = 0, end = value->vector.length; i < end; ++i) {
                    value->hash += bowl_value_hash(value->vector.elements[i]) * 31;
                }
                break;

            case BowlExceptionValue:
                value->hash += bowl_value_hash(value->exception.cause) * 31;
                value->hash += bowl_value_hash(value->exception.message) * 31;
                break;
        }
    }

    return value->hash;
}

bool bowl_value_equals(BowlValue a, BowlValue b) {
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
            case BowlSymbolValue:
                if (a->symbol.length != b->symbol.length) {
                    return false;
                }

                for (u64 i = 0, end = a->symbol.length; i < end; ++i) {
                    if (a->symbol.bytes[i] != b->symbol.bytes[i]) {
                        return false;
                    }
                }

                return true;

            case BowlNumberValue:
                return a->number.value == b->number.value;

            case BowlBooleanValue:
                return a->boolean.value == b->boolean.value;

            case BowlStringValue:
                if (a->string.size != b->string.size) {
                    return false;
                }

                for (u64 i = 0, end = a->string.size; i < end; ++i) {
                    if (a->string.bytes[i] != b->string.bytes[i]) {
                        return false;
                    }
                }

                return true;

            case BowlNativeValue:
                return a->function.function == b->function.function;

            case BowlLibraryValue:
                return a->library.handle == b->library.handle;

            case BowlVectorValue:
                if (a->vector.length != b->vector.length) {
                    return false;
                }

                {
                    const u64 length = a->vector.length;
                    for (register u64 i = 0; i < length; ++i) {
                        if (!bowl_value_equals(a->vector.elements[i], b->vector.elements[i])) {
                            return false;
                        }
                    }
                }

                return true;

            case BowlListValue:
                if (a->list.length != b->list.length) {
                    return false;
                }

                do {
                    if (!bowl_value_equals(a->list.head, b->list.head)) {
                        return false;
                    }

                    a = a->list.tail;
                    b = b->list.tail;
                } while (a != NULL);

                return true;

            case BowlMapValue:
                if (a->map.length != b->map.length) {
                    return false;
                }

                return bowl_map_subset_of(a, b) && bowl_map_subset_of(b, a);

            case BowlExceptionValue:
                return bowl_value_equals(a->exception.message, b->exception.message) 
                    && bowl_value_equals(a->exception.cause, b->exception.cause);
        }
    }
}

bool bowl_map_subset_of(BowlValue superset, BowlValue subset) {
    if (subset->map.length > superset->map.length) {
        return false;
    }

    for (u64 i = 0, end = subset->map.capacity; i < end; ++i) {
        BowlValue bucket = subset->map.buckets[i];

        while (bucket != NULL) {
            const BowlValue result = bowl_map_get_or_else(superset, bucket->list.head, bowl_sentinel_value);

            bucket = bucket->list.tail;

            if (result == bowl_sentinel_value) {
                return false;
            } else if (!bowl_value_equals(bucket->list.head, result)) {
                return false;
            }

            bucket = bucket->list.tail;
        }
    }

    return true;
}

u64 bowl_value_byte_size(BowlValue value) {
    if (value == NULL) {
        return 0;
    } else {
        switch (value->type) {
            case BowlSymbolValue:
                return sizeof(struct bowl_value) + value->symbol.length * sizeof(u8);
            case BowlStringValue:
                return sizeof(struct bowl_value) + value->string.size * sizeof(u8);
            case BowlLibraryValue:
                return sizeof(struct bowl_value) + value->library.length * sizeof(u8);
            case BowlMapValue:
                return sizeof(struct bowl_value) + value->map.capacity * sizeof(BowlValue);
            case BowlVectorValue:
                return sizeof(struct bowl_value) + value->vector.length * sizeof(BowlValue);
            default:
                return sizeof(struct bowl_value);
        }
    }
}

void bowl_value_debug(BowlValue value, char *message, ...) {
    va_list list;
    printf("[debug] ");
    fflush(stdout);

    va_start(list, message);
    vprintf(message, list);
    fflush(stdout);
    va_end(list);

    bowl_value_dump(stdout, value);

    printf("\n");
    fflush(stdout);
}

void bowl_value_dump(FILE *stream, BowlValue value) {
    if (value == NULL) {
        fprintf(stream, "[ ]");
    } else {
        switch (value->type) {
            case BowlSymbolValue:
                for (u64 i = 0, end = value->symbol.length; i < end; ++i) {
                    fprintf(stream, "%c", value->symbol.bytes[i]);
                }
                break;

            case BowlNumberValue:
                if (is_integer(value->number.value)) {
                    fprintf(stream, "%" PRId64, (u64) value->number.value);
                } else {
                    fprintf(stream, "%f", value->number.value);
                }
                break;
            
            case BowlBooleanValue:
                if (value->boolean.value) {
                    fprintf(stream, "true");
                } else {
                    fprintf(stream, "false");
                }
                break;

            case BowlNativeValue:
                fprintf(stream, "function#0x%08" PRIX64, (u64) value->function.function);
                break;

            case BowlLibraryValue:
                fprintf(stream, "library#0x%08" PRIX64, (u64) value->library.handle);
                break;

            case BowlVectorValue:
                fprintf(stream, "(");
                for (u64 i = 0; i < value->vector.length; ++i) {
                    fprintf(stream, " ");
                    bowl_value_dump(stream, value->vector.elements[i]);
                }
                fprintf(stream, " )");
                break;

            case BowlListValue:
                fprintf(stream, "[ ");

                do {
                    bowl_value_dump(stream, value->list.head);
                    value = value->list.tail;
                    if (value != NULL) {
                        fprintf(stream, " ");
                    }
                } while (value != NULL);

                fprintf(stream, " ]");
                break;

            case BowlExceptionValue:
                bowl_value_dump(stream, value->exception.message);
                fprintf(stream, " exception");
                break;

            case BowlStringValue:
                fprintf(stream, "\"");
                
                {
                    u32 state = UNICODE_UTF8_ACCEPT;
                    u32 codepoint;

                    for (u64 i = 0, last = 0, end = value->string.size; i < end; ++i) {
                        if (unicode_utf8_decode(&state, &codepoint, value->string.bytes[i]) == UNICODE_UTF8_ACCEPT) {
                            if (codepoint < 0x7F) {
                                // TODO: handle unicode escape sequences
                                // handle ascii characters and escape sequences
                                char const* sequence = escape(value->string.bytes[i]);
                                if (sequence != NULL) {
                                    fprintf(stream, "%s", sequence);
                                } else {
                                    fwrite(&value->string.bytes[last], i - last + 1, 1, stream);
                                }
                            } else {
                                fwrite(&value->string.bytes[last], i - last + 1, 1, stream); 
                            }
                            last = i;
                        } else if (state == UNICODE_UTF8_REJECT) {
                            const u8 replacement_character[2] = { 0xFF, 0xFD };
                            fwrite(&replacement_character[0], sizeof(replacement_character), 1, stream);
                            state = UNICODE_UTF8_ACCEPT;
                            last = i;
                        }
                    }

                    if (state != UNICODE_UTF8_ACCEPT) {
                        const u8 replacement_character[2] = { 0xFF, 0xFD };
                        fwrite(&replacement_character[0], sizeof(replacement_character), 1, stream);
                    }
                }

                fprintf(stream, "\"");
                break;

            case BowlMapValue:
                fprintf(stream, "{ ");

                bool first = true;
                for (u64 i = 0, end = value->map.capacity; i < end; ++i) {
                    BowlValue bucket = value->map.buckets[i];

                    if (!first && bucket != NULL) {
                        fprintf(stream, " ");
                    } else if (bucket != NULL) {
                        first = false;
                    }

                    while (bucket != NULL) {
                        bowl_value_dump(stream, bucket->list.head);
                        bucket = bucket->list.tail;
                        fprintf(stream, " : ");
                        bowl_value_dump(stream, bucket->list.head);
                        bucket = bucket->list.tail;

                        if (bucket != NULL) {
                            fprintf(stream, " ");
                        }
                    }
                }

                if (first) {
                    fprintf(stream, "}");
                } else {
                    fprintf(stream, " }");
                }
                break;
        }
    }
}

static bool bowl_value_printf_buffer(char **buffer, u64 *length, u64 *capacity, char *message, ...) {
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

static void bowl_value_show_buffer(BowlValue value, char **buffer, u64 *length, u64 *capacity) {
    if (value == NULL) {
        bowl_value_printf_buffer(buffer, length, capacity, "[ ]");
    } else {
        switch (value->type) {
            case BowlSymbolValue:
                for (u64 i = 0, end = value->symbol.length; i < end; ++i) {
                    if (!bowl_value_printf_buffer(buffer, length, capacity, "%c", value->symbol.bytes[i])) {
                        return;
                    }
                }
                break;

            case BowlNumberValue:
                if (is_integer(value->number.value)) {
                    if (!bowl_value_printf_buffer(buffer, length, capacity, "%" PRId64, (u64) value->number.value)) {
                        return;
                    }
                } else {
                    if (!bowl_value_printf_buffer(buffer, length, capacity, "%f", value->number.value)) {
                        return;
                    }
                }
                break;

            case BowlVectorValue:
                if (!bowl_value_printf_buffer(buffer, length, capacity, "(")) {
                    return;
                }

                for (u64 i = 0; i < value->vector.length; ++i) {
                    if (!bowl_value_printf_buffer(buffer, length, capacity, " ")) {
                        return;
                    }

                    bowl_value_show_buffer(value->vector.elements[i], buffer, length, capacity);
                    if (*buffer == NULL) {
                        return;
                    }
                }

                if (!bowl_value_printf_buffer(buffer, length, capacity, " )")) {
                    return;
                }
                break;
            
            case BowlBooleanValue:
                if (value->boolean.value) {
                    if (!bowl_value_printf_buffer(buffer, length, capacity, "true")) {
                        return;
                    }
                } else {
                    if (!bowl_value_printf_buffer(buffer, length, capacity, "false")) {
                        return;
                    }
                }
                break;

            case BowlNativeValue:
                if (!bowl_value_printf_buffer(buffer, length, capacity, "function#0x%08" PRIX64, (u64) value->function.function)) {
                    return;
                }
                break;

            case BowlLibraryValue:
                if (!bowl_value_printf_buffer(buffer, length, capacity, "library#0x%08" PRIX64, (u64) value->library.handle)) {
                    return;
                }
                break;

            case BowlListValue:
                if (!bowl_value_printf_buffer(buffer, length, capacity, "[ ")) {
                    return;
                }

                do {
                    bowl_value_show_buffer(value->list.head, buffer, length, capacity);
                    if (*buffer == NULL) {
                        return;
                    }
                    value = value->list.tail;
                    if (value != NULL) {
                        if (!bowl_value_printf_buffer(buffer, length, capacity, " ")) {
                            return;
                        }
                    }
                } while (value != NULL);

                if (!bowl_value_printf_buffer(buffer, length, capacity, " ]")) {
                    return;
                }
                break;

            case BowlExceptionValue:
                bowl_value_show_buffer(value->exception.message, buffer, length, capacity);

                if (*buffer == NULL) {
                    return;
                }

                if (!bowl_value_printf_buffer(buffer, length, capacity, " exception")) {
                    return;
                }
                break;

            case BowlStringValue:
                if (!bowl_value_printf_buffer(buffer, length, capacity, "\"")) {
                    return;
                }

                 {
                    u32 state = UNICODE_UTF8_ACCEPT;
                    u32 codepoint;

                    for (u64 i = 0, last = 0, end = value->string.size; i < end; ++i) {
                        if (unicode_utf8_decode(&state, &codepoint, value->string.bytes[i]) == UNICODE_UTF8_ACCEPT) {
                            if (codepoint < 0x7F) {
                                // TODO: handle unicode escape sequences
                                // handle ascii characters and escape sequences
                                char const* sequence = escape(value->string.bytes[i]);
                                if (sequence != NULL) {
                                    if (!bowl_value_printf_buffer(buffer, length, capacity, "%s", sequence)) {
                                        return;
                                    }
                                } else {
                                    if (!bowl_value_printf_buffer(buffer, length, capacity, "%c", value->string.bytes[i])) {
                                        return;
                                    }
                                }
                            } else {
                                for (u64 j = last; j <= i; ++j) {
                                    if (!bowl_value_printf_buffer(buffer, length, capacity, "%c", value->string.bytes[j])) {
                                        return;
                                    }
                                }
                            }

                            last = i;
                        } else if (state == UNICODE_UTF8_REJECT) {
                            state = UNICODE_UTF8_ACCEPT;
                            last = i;
                            if (!bowl_value_printf_buffer(buffer, length, capacity, "%c%c", 0xFF, 0xFD)) {
                                return;
                            }
                        }
                    }

                    if (state != UNICODE_UTF8_ACCEPT) {
                        if (!bowl_value_printf_buffer(buffer, length, capacity, "%c%c", 0xFF, 0xFD)) {
                            return;
                        }
                    }
                }

                if (!bowl_value_printf_buffer(buffer, length, capacity, "\"")) {
                    return;
                }
                break;

            case BowlMapValue:
                if (!bowl_value_printf_buffer(buffer, length, capacity, "[ ")) {
                    return;
                }

                bool first = true;
                for (u64 i = 0, end = value->map.capacity; i < end; ++i) {
                    BowlValue bucket = value->map.buckets[i];

                    if (!first && bucket != NULL) {
                        if (!bowl_value_printf_buffer(buffer, length, capacity, " ")) {
                            return;
                        }
                    } else if (bucket != NULL) {
                        first = false;
                    }

                    while (bucket != NULL) {
                        if (!bowl_value_printf_buffer(buffer, length, capacity, "[ ")) {
                            return;
                        }

                        bowl_value_show_buffer(bucket->list.head, buffer, length, capacity);
                        if (*buffer == NULL) {
                            return;
                        }

                        bucket = bucket->list.tail;
                        if (!bowl_value_printf_buffer(buffer, length, capacity, " ")) {
                            return;
                        }
                        
                        bowl_value_show_buffer(bucket->list.head, buffer, length, capacity);
                        if (*buffer == NULL) {
                            return;
                        }

                        bucket = bucket->list.tail;
                        if (!bowl_value_printf_buffer(buffer, length, capacity, " ]")) {
                            return;
                        }

                        if (bucket != NULL) {
                            if (!bowl_value_printf_buffer(buffer, length, capacity, " ")) {
                                return;
                            }
                        }
                    }
                }

                if (first) {
                    if (!bowl_value_printf_buffer(buffer, length, capacity, "] map-from-list")) {
                        return;
                    }
                } else {
                    if (!bowl_value_printf_buffer(buffer, length, capacity, " ] map-from-list")) {
                        return;
                    }
                }
                break;
        }
    }
}

void bowl_value_show(BowlValue value, char **buffer, u64 *length) {
    u64 capacity = 4096;
    *length = 0;
    *buffer = malloc(capacity * sizeof(char));
    if (*buffer != NULL) {
        bowl_value_show_buffer(value, buffer, length, &capacity);
    }
}

u64 bowl_value_length(BowlValue value) {
    if (value == NULL) {
        return 0;
    } else {
        switch (value->type) {
            case BowlSymbolValue:
                return value->symbol.length;
            case BowlListValue:
                return value->list.length;
            case BowlMapValue:
                return value->map.length;
            case BowlStringValue: 
                return value->string.length;
            case BowlVectorValue: 
                return value->vector.length;
            default: 
                return 0;
        }
    }
}

char *bowl_type_name(BowlValueType type) {
    static char *types[] = {
        [BowlSymbolValue]  = "symbol",
        [BowlListValue]    = "list",
        [BowlNativeValue]  = "function",
        [BowlMapValue]     = "map",
        [BowlBooleanValue] = "boolean",
        [BowlNumberValue]  = "number",
        [BowlStringValue]  = "string",
        [BowlLibraryValue] = "library",
        [BowlVectorValue]  = "vector",
        [BowlExceptionValue]  = "exception"
    };

    return types[type];
}

char *bowl_value_type(BowlValue value) {
    return bowl_type_name(value == NULL ? BowlListValue : value->type);
}

char *bowl_string_to_null_terminated(BowlValue value) {
    char *path = malloc(value->string.size + 1);
    
    if (path != NULL) {
        memcpy(path, &value->string.bytes[0], value->string.size);
        path[value->string.size] = '\0';    
    }

    return path;
}

BowlResult bowl_format_exception(BowlStack stack, char *message, ...) {
    BOWL_STATIC_STRING(format_exception_message, "failed to format exception message in function 'bowl_format_exception'");
    static struct bowl_value format_exception = {
        .type = BowlExceptionValue,
        .location = NULL,
        .hash = 0,
        .exception = {
            .cause = NULL,
            .message = &format_exception_message.value
        }
    };

    va_list list;

    va_start(list, message);
    const u64 required = vsnprintf(NULL, 0, message, list);
    va_end(list);

    BowlResult result = gc_allocate(stack, BowlStringValue, (required + 1) * sizeof(u8));

    if (!result.failure) {
        result.value->string.size = required;
        result.value->string.utf8.index = 0;
        result.value->string.utf8.offset = 0;
        va_start(list, message);
        const u64 written = vsnprintf(&result.value->string.bytes[0], required + 1, message, list);
        va_end(list);

        if (written < 0 || written >= required + 1) {
            result.exception = &format_exception;
            result.failure = true;
            return result;
        } else {
            result.value->string.length = unicode_utf8_count(&result.value->string.bytes[0], required);

            if (result.value->string.length == (u64) -1) {
                result.exception = bowl_exception_malformed_utf8;
                result.failure = true;
                return result;
            } else if (result.value->string.length == (u64) -2) {
                result.value = bowl_exception_incomplete_utf8;
                result.failure = true;
                return result;
            }
        }

        result = bowl_exception(stack, NULL, result.value);
    }

    return result;
}

BowlResult bowl_allocate(BowlStack stack, BowlValueType type, u64 additional) {
    return gc_allocate(stack, type, additional);
}

BowlResult bowl_value_clone(BowlStack stack, BowlValue value) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, value, NULL, NULL);
    BowlResult result;

    if (frame.registers[0] == NULL) {
        result.value = NULL;
        result.failure = false;
    } else {
        const u64 size = bowl_value_byte_size(frame.registers[0]);
        const u64 additional = size - sizeof(struct bowl_value);
        result = gc_allocate(&frame, frame.registers[0]->type, additional);

        if (!result.failure) {
            memcpy(result.value, frame.registers[0], size);
        }
    }

    return result;
}

BowlResult bowl_list_reverse(BowlStack stack, BowlValue list) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, list, NULL, NULL);
    BowlResult result;

    while (frame.registers[0] != NULL) {
        result = bowl_list(&frame, frame.registers[0]->list.head, frame.registers[1]);

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

// TODO: Unicode scanner
BowlResult bowl_tokens(BowlStack stack, BowlValue string) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, string, NULL, NULL);
    BowlScanner scanner = scanner_from(&frame.registers[0]);
    BowlResult result;

    while (scanner_has_next(&scanner)) {
        switch (scanner_next(&scanner)) {
            case BowlErrorToken:
                result = bowl_format_exception(&frame, "%s in line %" PRId64 " at character %" PRId64, scanner.token.error.message, scanner.token.line, scanner.token.column);
                result.failure = true;
                return result;

            case BowlBooleanToken:
                result = bowl_boolean(&frame, scanner.token.boolean.value);
                break;

            case BowlNumberToken:
                result = bowl_number(&frame, scanner.token.number.value);
                break;

            case BowlSymbolToken:
                result = bowl_allocate(&frame, BowlSymbolValue, scanner.token.symbol.length);
                
                if (!result.failure) {
                    result.value->symbol.length = scanner.token.symbol.length;
                    memcpy(
                        result.value->symbol.bytes, 
                        (*scanner.string)->string.bytes + scanner.token.symbol.start, 
                        scanner.token.symbol.length * sizeof(u8)
                    );
                }
                break;

            case BowlStringToken:
                result = bowl_allocate(&frame, BowlStringValue, scanner.token.string.length);
                if (!result.failure) {
                    const u64 length = scanner.token.string.length;
                    const u64 start = scanner.token.string.start;
                    u8 *const bytes = &((*scanner.string)->string.bytes[0]);
                    u8 *const dst = &result.value->string.bytes[0];

                    register bool escaped = false;
                    register u64 p = 0;
                    for (register u64 i = 0; i < length; ++i) {
                        const char current = bytes[start + i];
                        if (escaped) {
                // TODO : Unicode escaping
                            escaped = false;
                            dst[p++] = unescape(current);
                        } else if (current == '\\') {
                            escaped = true;
                        } else {
                            dst[p++] = current;
                        }
                    }

                    result.value->string.size = p;
                    result.value->string.length = p;
                    result.value->string.utf8.index = 0;
                    result.value->string.utf8.offset = 0;
                }

                break;
        }

        if (result.failure) {
            return result;
        }

        result = bowl_list(&frame, result.value, frame.registers[1]);

        if (result.failure) {
            return result;
        }

        frame.registers[1] = result.value;
    }

    return bowl_list_reverse(&frame, frame.registers[1]);
}

BowlResult bowl_symbol(BowlStack stack, u8 *bytes, u64 length) {
    BowlResult result = gc_allocate(stack, BowlSymbolValue, length * sizeof(u8));

    if (!result.failure) {
        result.value->symbol.length = length;
        memcpy(result.value->symbol.bytes, bytes, length * sizeof(u8));
        bowl_value_hash(result.value);    
    }

    return result;
}

BowlResult bowl_string(BowlStack stack, u8 *bytes, u64 length) {
    BowlResult result = gc_allocate(stack, BowlStringValue, length * sizeof(u8));

    if (!result.failure) {
        result.value->string.size = length;
        result.value->string.length = unicode_utf8_count(bytes, length);
        result.value->string.utf8.index = 0;
        result.value->string.utf8.offset = 0;

        if (result.value->string.length == (u64) -1) {
            result.failure = true;
            result.exception = bowl_exception_malformed_utf8;
            return result;
        } else if (result.value->string.length == (u64) -2) {
            result.failure = true;
            result.exception = bowl_exception_incomplete_utf8;
            return result;
        }

        memcpy(&result.value->string.bytes[0], bytes, length * sizeof(u8));
    }

    return result;
}

BowlResult bowl_function(BowlStack stack, BowlValue library, BowlFunction function) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);
    BowlResult result = gc_allocate(&frame, BowlNativeValue, 0);
    
    if (!result.failure) {
        result.value->function.library = frame.registers[0];
        result.value->function.function = function;
    }

    return result;
}

BowlResult bowl_list(BowlStack stack, BowlValue head, BowlValue tail) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, head, tail, NULL);    
    BowlResult result = gc_allocate(&frame, BowlListValue, 0);

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

BowlResult bowl_map(BowlStack stack, u64 capacity) {
    BowlResult result = gc_allocate(stack, BowlMapValue, capacity * sizeof(BowlValue));

    if (!result.failure) {
        result.value->map.capacity = capacity;
        result.value->map.length = 0;
        
        for (u64 i = 0; i < capacity; ++i) {
            result.value->map.buckets[i] = NULL;
        }
    }
    
    return result;
}

BowlResult bowl_number(BowlStack stack, double value) {
    BowlResult result = gc_allocate(stack, BowlNumberValue, 0);

    if (!result.failure) {
        result.value->number.value = value;
    }
    
    return result;
}

BowlResult bowl_library(BowlStack stack, char *path) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, NULL, NULL, NULL);
    BowlResult result;
    BowlLibraryHandle handle;
    bool already_loaded;
    const u64 length = strlen(path);

    result = gc_allocate(&frame, BowlLibraryValue, length);

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

BowlResult bowl_boolean(BowlStack stack, bool value) {
    BowlResult result = gc_allocate(stack, BowlBooleanValue, 0);
    
    if (!result.failure) {
        result.value->boolean.value = value;
    }

    return result;
}

BowlResult bowl_vector(BowlStack stack, BowlValue value, u64 const length) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, value, NULL, NULL);
    BowlResult result = gc_allocate(&frame, BowlVectorValue, length * sizeof(BowlValue));

    if (!result.failure) {
        result.value->vector.length = length;
        
        for (register u64 i = 0; i < length; ++i) {
            result.value->vector.elements[i] = frame.registers[0];
        }
    }

    return result;
}

BowlResult bowl_exception(BowlStack stack, BowlValue cause, BowlValue message) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, cause, message, NULL);
    BowlResult result = gc_allocate(&frame, BowlExceptionValue, 0);

    if (!result.failure) {
        result.value->exception.cause = frame.registers[0];
        result.value->exception.message = frame.registers[1];
    }

    return result;
}
