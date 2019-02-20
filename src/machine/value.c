#include "value.h"

static byte *value_src_memory = NULL;
static byte *value_dst_memory = NULL;
static word value_memory_size = 0;
static word value_memory_ptr = 0;

static struct value value_internal_marker = {
    .type = SymbolValue,
    .location = NULL,
    .hash = 31,
    .symbol.length = 0
};

static Value value_list_head_register = NULL;
static Value value_list_tail_register = NULL;

static Value value_map_put_map_register = NULL;
static Value value_map_put_key_register = NULL;
static Value value_map_put_value_register = NULL;
static Value value_map_put_bucket_register = NULL;
static Value value_map_put_result_register = NULL;

static Value value_relocate(Value value) {
    if (value == NULL) {
        return NULL;
    } else if (value->location != NULL) {
        return value->location;
    } else {
        const word bytes = value_byte_size(value);
        const Value result = (Value) (value_dst_memory + value_memory_ptr);
        value_memory_ptr += bytes;
        memcpy(result, value, bytes);
        value->location = result;
        return result;
    }
}

static void value_collect_garbage(void) {
    byte *swap = value_dst_memory;
    value_dst_memory = value_src_memory;
    value_src_memory = swap;

    value_memory_ptr = 0;

    value_list_head_register = value_relocate(value_list_head_register);
    value_list_tail_register = value_relocate(value_list_tail_register);

    value_map_put_map_register = value_relocate(value_map_put_map_register);
    value_map_put_key_register = value_relocate(value_map_put_key_register);
    value_map_put_value_register = value_relocate(value_map_put_value_register);
    value_map_put_bucket_register = value_relocate(value_map_put_bucket_register);
    value_map_put_result_register = value_relocate(value_map_put_result_register);

    for (word i = 0; i < machine_instance_count; ++i) {
        Machine machine = machine_instances[i];
        
        for (word j = 0; j < MACHINE_REGISTER_COUNT; ++j) {
            machine->registers[j] = value_relocate(machine->registers[j]);
        }

        machine->dictionary = value_relocate(machine->dictionary);
        machine->callstack = value_relocate(machine->callstack);
        machine->datastack = value_relocate(machine->datastack);
    }

    word scan = 0;
    while (scan < value_memory_ptr) {
        const Value value = (Value) (value_dst_memory + scan);
        const word bytes = value_byte_size(value);

        switch (value->type) {
            case ListValue:
                value->list.head = value_relocate(value->list.head);
                value->list.tail = value_relocate(value->list.tail);
                break;
            case MapValue:
                for (word i = 0, end = value->map.bucket_count; i < end; ++i) {
                    value->map.buckets[i] = value_relocate(value->map.buckets[i]);
                }
                break;
            default:
                /* not a compound type */
                break;
        }

        scan += bytes;
    }
}

static Value value_allocate(word additional) {
    const word actual = sizeof(struct value) + additional;

    if (value_memory_ptr + actual > value_memory_size) {
        value_collect_garbage();

        if (value_memory_ptr + actual > value_memory_size) {
            value_memory_size = max(value_memory_size * 2, value_memory_size + actual);

            value_src_memory = realloc(value_src_memory, value_memory_size * sizeof(byte));
            if (value_src_memory == NULL) {
                fatal("out of heap");
            }

            value_collect_garbage();

            value_src_memory = realloc(value_src_memory, value_memory_size * sizeof(byte));
            if (value_src_memory == NULL) {
                fatal("out of heap");
            }
        }
    }

    const Value result = (Value) (value_dst_memory + value_memory_ptr);
    value_memory_ptr += actual;
    return result;
}

static void value_ensure_capacity(char **buffer, word *buffer_size, word buffer_ptr, word necessary) {
    if (buffer_ptr + necessary > *buffer_size) {
        *buffer_size = max(*buffer_size * 2, *buffer_size + necessary);
        *buffer = realloc(*buffer, *buffer_size);
        if (*buffer == NULL) {
            fatal("out of heap");
        }
    }
}

const Value value_marker = &value_internal_marker;

Value value_create(ValueType type, word additional) {
    const Value result = value_allocate(additional);
    result->type = type;
    result->location = NULL;
    result->hash = 0;
    return result;
}

Value value_symbol(word length, char *value) {
    const Value result = value_create(SymbolValue, length * sizeof(char));
    result->symbol.length = length;
    memcpy(result->symbol.value, value, length * sizeof(char));
    return result;
}

Value value_symbol_from_string(char *string) {
    const word length = strlen(string);
    return value_symbol(length, string);
}

Value value_number(double value) {
    const Value result = value_create(NumberValue, 0);
    result->number.value = value;
    return result;
}

Value value_string(word length, char *value) {
    const Value result = value_create(StringValue, length * sizeof(char));
    result->string.length = length;
    memcpy(result->string.value, value, length * sizeof(char));
    return result;
}

Value value_string_from_string(char *string) {
    const word length = strlen(string);
    return value_string(length, string);
}

Value value_native(word length, char *value, NativeFunction function) {
    const Value result = value_create(NativeValue, length * sizeof(char));
    result->native.length = length;
    result->native.function = function;
    memcpy(result->native.value, value, length * sizeof(char));
    return result;
}

Value value_native_from_string(char *string, NativeFunction function) {
    const word length = strlen(string);
    return value_native(length, string, function);
}

Value value_list(Value head, Value tail) {
    value_list_head_register = head;
    value_list_tail_register = tail;
    const Value result = value_create(ListValue, 0);
    result->list.head = value_list_head_register;
    result->list.tail = value_list_tail_register;
    result->list.length = (value_list_tail_register == NULL ? 0 : value_list_tail_register->list.length) + 1;
    value_list_head_register = value_list_tail_register = NULL;
    return result;
}

Value value_map(word bucket_count) {
    const Value result = value_create(MapValue, bucket_count * sizeof(Value));
    result->map.length = 0;
    result->map.bucket_count = bucket_count;

    for (word i = 0; i < bucket_count; ++i) {
        result->map.buckets[i] = NULL;
    }

    return result;
}

Value value_map_put(Value map, Value key, Value value) {
    value_map_put_map_register = map;
    value_map_put_key_register = key;
    value_map_put_value_register = value;

    const word index = value_hash(key) % value_map_put_map_register->map.bucket_count;
    
    value_map_put_result_register = NULL;
    value_map_put_bucket_register = value_map_put_map_register->map.buckets[index];

    bool found = false;
    while (value_map_put_bucket_register != NULL) {
        if (!found && value_equals(value_map_put_key_register, value_map_put_bucket_register->list.head)) {
            value_map_put_result_register = value_list(value_map_put_value_register, value_map_put_result_register);
            value_map_put_result_register = value_list(value_map_put_key_register, value_map_put_result_register);
            found = true;
        } else {
            value_map_put_result_register = value_list(value_map_put_bucket_register->list.tail->list.head, value_map_put_result_register);
            value_map_put_result_register = value_list(value_map_put_bucket_register->list.head, value_map_put_result_register);
        }

        value_map_put_bucket_register = value_map_put_bucket_register->list.tail->list.tail;
    }

    const word length = value_map_put_map_register->map.length + (found ? 0 : 1);

    if (!found) {
        value_map_put_result_register = value_list(value_map_put_value_register, value_map_put_result_register);
        value_map_put_result_register = value_list(value_map_put_key_register, value_map_put_result_register);
    }

    const Value result = value_map(value_map_put_map_register->map.bucket_count);
    result->map.length = length;
    memcpy(result->map.buckets, value_map_put_map_register->map.buckets, value_map_put_map_register->map.bucket_count * sizeof(Value));
    result->map.buckets[index] = value_map_put_result_register;

    value_map_put_map_register = value_map_put_key_register = value_map_put_value_register = value_map_put_result_register = value_map_put_bucket_register = NULL;

    return result;
}

Value value_map_get_or_else(Value map, Value key, Value otherwise) {
    if (map->type != MapValue) {
        fatal("illegal argument for function 'get'", map->type);
    }

    const word index = value_hash(key) % map->map.bucket_count;

    Value bucket = map->map.buckets[index];

    while (bucket != NULL) {
        if (value_equals(key, bucket->list.head)) {
            return bucket->list.tail->list.head;
        }
        bucket = bucket->list.tail->list.tail;
    }
    
    return otherwise;
}

word value_byte_size(Value value) {
    if (value == NULL) {
        return 0;
    } else {
        switch (value->type) {
            case SymbolValue:
                return sizeof(struct value) + value->symbol.length * sizeof(char);
            case StringValue:
                return sizeof(struct value) + value->string.length * sizeof(char);
            case NativeValue:
                return sizeof(struct value) + value->native.length * sizeof(char);
            case MapValue:
                return sizeof(struct value) + value->map.bucket_count * sizeof(Value);
            default:
                return sizeof(struct value);
        }
    }
}

word value_hash(Value value) {
    if (value == NULL) {
        return 31;
    } else if (value->hash == 0) {
        value->hash = 31;

        switch (value->type) {
            case SymbolValue:
                for (word i = 0, end = value->symbol.length; i < end; ++i) {
                    value->hash += pow(value->symbol.value[i] * 31, end - (i + 1));
                }
                break;

            case NumberValue:
                value->hash += *((word *) (&value->number.value)) * 31;
                break;

            case StringValue:
                for (word i = 0, end = value->string.length; i < end; ++i) {
                    value->hash += pow(value->string.value[i] * 31, end - (i + 1));
                }
                break;

            case NativeValue:
                value->hash += (word) value * 31;
                break;

            case ListValue:
                value->hash += value_hash(value->list.head) * 31;
                value->hash += value_hash(value->list.tail) * 31;
                break;

            case MapValue:
                for (word i = 0, end = value->map.bucket_count; i < end; ++i) {
                    value->hash += value_hash(value->map.buckets[i]) * 31;
                }
                break;
        }
    }

    return value->hash;
}

bool value_equals(Value a, Value b) {
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
            case SymbolValue:
                if (a->symbol.length != b->symbol.length) {
                    return false;
                }

                for (word i = 0, end = a->symbol.length; i < end; ++i) {
                    if (a->symbol.value[i] != b->symbol.value[i]) {
                        return false;
                    }
                }

                return true;

            case NumberValue:
                return a->number.value == b->number.value;

            case StringValue:
                if (a->string.length != b->string.length) {
                    return false;
                }

                for (word i = 0, end = a->string.length; i < end; ++i) {
                    if (a->string.value[i] != b->string.value[i]) {
                        return false;
                    }
                }

                return true;

            case NativeValue:
                return false;

            case ListValue:
                if (a->list.length != b->list.length) {
                    return false;
                }

                do {
                    if (!value_equals(a->list.head, b->list.head)) {
                        return false;
                    }
                    a = a->list.tail;
                    b = b->list.tail;
                } while (a != NULL);

                return true;

            case MapValue:
                if (a->map.length != b->map.length) {
                    return false;
                }

                for (word i = 0, end = a->map.bucket_count; i < end; ++i) {
                    Value bucket = a->map.buckets[i];
                    while (bucket != NULL) {
                        const Value result = value_map_get_or_else(b, bucket->list.head, value_marker);
                        bucket = bucket->list.tail;
                        if (result == value_marker) {
                            return false;
                        } else if (!value_equals(bucket->list.head, result)) {
                            return false;
                        }
                        bucket = bucket->list.tail;
                    }
                }

                return true;
        }
    }
}

char *value_to_string(Value value) {
    char *buffer;
    size_t buffer_length;
    
    FILE *stream = open_memstream(&buffer, &buffer_length);
    if (stream == NULL) {
        fatal("out of heap");
    }

    value_dump(stream, value);
    fprintf(stream, "%c", '\0');
    fflush(stream);
    fclose(stream);
    
    return buffer;
}

void value_dump(FILE *stream, Value value) {
    if (value == NULL) {
        fprintf(stream, "[ ]");
    } else {
        switch (value->type) {
            case SymbolValue:
                for (word i = 0, end = value->symbol.length; i < end; ++i) {
                    fprintf(stream, "%c", value->symbol.value[i]);
                }
                break;

            case NumberValue:
                if ((double) ((word) value->number.value) == value->number.value) {
                    fprintf(stream, "%" PRId64, (word) value->number.value);
                } else {
                    fprintf(stream, "%f", value->number.value);
                }
                break;

            case NativeValue:
                for (word i = 0, end = value->native.length; i < end; ++i) {
                    fprintf(stream, "%c", value->native.value[i]);
                }
                break;

            case ListValue:
                fprintf(stream, "[ ");
                do {
                    value_dump(stream, value->list.head);
                    value = value->list.tail;
                    if (value != NULL) {
                        fprintf(stream, " ");
                    } else {
                        break;
                    }
                } while (true);
                fprintf(stream, " ]");
                break;

            case StringValue:
                fprintf(stream, "\"");
                for (word i = 0, end = value->string.length; i < end; ++i) {
                    switch (value->string.value[i]) {
                        case '\t':
                            fprintf(stream, "\\t");
                            break;
                        case '\a':
                            fprintf(stream, "\\a");
                            break;
                        case '\r':
                            fprintf(stream, "\\r");
                            break;
                        case '\n':
                            fprintf(stream, "\\n");
                            break;
                        case '\v':
                            fprintf(stream, "\\v");
                            break;
                        case '\f':
                            fprintf(stream, "\\f");
                            break;
                        case '\0':
                            fprintf(stream, "\\0");
                            break;
                        case '\b':
                            fprintf(stream, "\\b");
                            break;
                        default:
                            fprintf(stream, "%c", value->string.value[i]);
                            break;
                    }
                }
                fprintf(stream, "\"");
                break;

            case MapValue:
                fprintf(stream, "[ ");

                bool first = true;
                for (word i = 0, end = value->map.bucket_count; i < end; ++i) {
                    Value bucket = value->map.buckets[i];

                    if (!first && bucket != NULL) {
                        fprintf(stream, " ");
                    } else if (bucket != NULL) {
                        first = false;
                    }

                    while (bucket != NULL) {
                        fprintf(stream, "[ ");
                        value_dump(stream, bucket->list.head);
                        bucket = bucket->list.tail;
                        fprintf(stream, " ");
                        value_dump(stream, bucket->list.head);
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
