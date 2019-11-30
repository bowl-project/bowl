#include "value.h"

Value value_map_get_or_else(Value map, Value key, Value otherwise) {
    const u64 index = value_hash(key) % map->map.capacity;

    Value bucket = map->map.buckets[index];

    while (bucket != NULL) {
        if (value_equals(key, bucket->list.head)) {
            return bucket->list.tail->list.head;
        }

        bucket = bucket->list.tail->list.tail;
    }
    
    return otherwise;
}

u64 value_hash(Value value) {
    if (value == NULL) {
        return 31;
    } else if (value->hash == 0) {
        value->hash = 31;

        switch (value->type) {
            case SymbolValue:
                for (u64 i = 0, end = value->symbol.length; i < end; ++i) {
                    value->hash += pow(value->symbol.bytes[i] * 31, end - (i + 1));
                }
                break;

            case NumberValue:
                value->hash += *((u64 *) (&value->number.value)) * 31;
                break;

            case BooleanValue:
                value->hash = value->boolean.value ? 7 : 31;
                break;

            case StringValue:
                for (u64 i = 0, end = value->string.length; i < end; ++i) {
                    value->hash += pow(value->string.bytes[i] * 31, end - (i + 1));
                }
                break;

            case NativeValue:
                value->hash += (u64) value->native.function * 31;
                break;

            case ListValue:
                value->hash += value_hash(value->list.head) * 31;
                value->hash += value_hash(value->list.tail) * 31;
                break;

            case MapValue:
                for (u64 i = 0, end = value->map.capacity; i < end; ++i) {
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

                for (u64 i = 0, end = a->symbol.length; i < end; ++i) {
                    if (a->symbol.bytes[i] != b->symbol.bytes[i]) {
                        return false;
                    }
                }

                return true;

            case NumberValue:
                return a->number.value == b->number.value;

            case BooleanValue:
                return a->boolean.value == b->boolean.value;

            case StringValue:
                if (a->string.length != b->string.length) {
                    return false;
                }

                for (u64 i = 0, end = a->string.length; i < end; ++i) {
                    if (a->string.bytes[i] != b->string.bytes[i]) {
                        return false;
                    }
                }

                return true;

            case NativeValue:
                return a->native.function == b->native.function;

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

                return value_map_subset_of(a, b) && value_map_subset_of(b, a);
        }
    }
}

bool value_map_subset_of(Value superset, Value subset) {
    static struct value marker = {
        .type = SymbolValue,
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
        Value bucket = subset->map.buckets[i];

        while (bucket != NULL) {
            const Value result = value_map_get_or_else(superset, bucket->list.head, &marker);

            bucket = bucket->list.tail;

            if (result == &marker) {
                return false;
            } else if (!value_equals(bucket->list.head, result)) {
                return false;
            }

            bucket = bucket->list.tail;
        }
    }

    return true;
}

u64 value_byte_size(Value value) {
    if (value == NULL) {
        return 0;
    } else {
        switch (value->type) {
            case SymbolValue:
                return sizeof(struct value) + value->symbol.length * sizeof(u8);
            case StringValue:
                return sizeof(struct value) + value->string.length * sizeof(u8);
            case MapValue:
                return sizeof(struct value) + value->map.capacity * sizeof(Value);
            default:
                return sizeof(struct value);
        }
    }
}

void value_dump(FILE *stream, Value value) {
    if (value == NULL) {
        fprintf(stream, "[ ]");
    } else {
        switch (value->type) {
            case SymbolValue:
                for (u64 i = 0, end = value->symbol.length; i < end; ++i) {
                    fprintf(stream, "%c", value->symbol.bytes[i]);
                }
                break;

            case NumberValue:
                if (is_integer(value->number.value)) {
                    fprintf(stream, "%" PRId64, (u64) value->number.value);
                } else {
                    fprintf(stream, "%f", value->number.value);
                }
                break;
            
            case BooleanValue:
                if (value->boolean.value) {
                    fprintf(stream, "true");
                } else {
                    fprintf(stream, "false");
                }
                break;

            case NativeValue:
                fprintf(stream, "0x%08" PRIX64, (u64) value->native.function);
                break;

            case ListValue:
                fprintf(stream, "[ ");

                do {
                    value_dump(stream, value->list.head);
                    value = value->list.tail;
                    if (value != NULL) {
                        fprintf(stream, " ");
                    }
                } while (value != NULL);

                fprintf(stream, " ]");
                break;

            case StringValue:
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

            case MapValue:
                fprintf(stream, "[ ");

                bool first = true;
                for (u64 i = 0, end = value->map.capacity; i < end; ++i) {
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

static bool value_print_buffer(char **buffer, u64 *length, u64 *capacity, char *message, ...) {
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

static void value_to_string(Value value, char **buffer, u64 *length, u64 *capacity) {
    if (value == NULL) {
        value_print_buffer(buffer, length, capacity, "[ ]");
    } else {
        switch (value->type) {
            case SymbolValue:
                for (u64 i = 0, end = value->symbol.length; i < end; ++i) {
                    if (!value_print_buffer(buffer, length, capacity, "%c", value->symbol.bytes[i])) {
                        return;
                    }
                }
                break;

            case NumberValue:
                if (is_integer(value->number.value)) {
                    if (!value_print_buffer(buffer, length, capacity, "%" PRId64, (u64) value->number.value)) {
                        return;
                    }
                } else {
                    if (!value_print_buffer(buffer, length, capacity, "%f", value->number.value)) {
                        return;
                    }
                }
                break;
            
            case BooleanValue:
                if (value->boolean.value) {
                    if (!value_print_buffer(buffer, length, capacity, "true")) {
                        return;
                    }
                } else {
                    if (!value_print_buffer(buffer, length, capacity, "false")) {
                        return;
                    }
                }
                break;

            case NativeValue:
                if (!value_print_buffer(buffer, length, capacity, "0x%08" PRIX64, (u64) value->native.function)) {
                    return;
                }
                break;

            case ListValue:
                if (!value_print_buffer(buffer, length, capacity, "[ ")) {
                    return;
                }

                do {
                    value_to_string(value->list.head, buffer, length, capacity);
                    if (*buffer == NULL) {
                        return;
                    }
                    value = value->list.tail;
                    if (value != NULL) {
                        if (!value_print_buffer(buffer, length, capacity, " ")) {
                            return;
                        }
                    }
                } while (value != NULL);

                if (!value_print_buffer(buffer, length, capacity, " ]")) {
                    return;
                }
                break;

            case StringValue:
                if (!value_print_buffer(buffer, length, capacity, "\"")) {
                    return;
                }

                for (u64 i = 0, end = value->string.length; i < end; ++i) {
                    char const* sequence = escape(value->string.bytes[i]);
                    if (sequence != NULL) {
                        if (!value_print_buffer(buffer, length, capacity, "%s", sequence)) {
                            return;
                        }
                    } else {
                        if (!value_print_buffer(buffer, length, capacity, "%c", value->string.bytes[i])) {
                            return;
                        }
                    }
                }

                if (!value_print_buffer(buffer, length, capacity, "\"")) {
                    return;
                }
                break;

            case MapValue:
                if (!value_print_buffer(buffer, length, capacity, "[ ")) {
                    return;
                }

                bool first = true;
                for (u64 i = 0, end = value->map.capacity; i < end; ++i) {
                    Value bucket = value->map.buckets[i];

                    if (!first && bucket != NULL) {
                        if (!value_print_buffer(buffer, length, capacity, " ")) {
                            return;
                        }
                    } else if (bucket != NULL) {
                        first = false;
                    }

                    while (bucket != NULL) {
                        if (!value_print_buffer(buffer, length, capacity, "[ ")) {
                            return;
                        }

                        value_to_string(bucket->list.head, buffer, length, capacity);
                        if (*buffer == NULL) {
                            return;
                        }

                        bucket = bucket->list.tail;
                        if (!value_print_buffer(buffer, length, capacity, " ")) {
                            return;
                        }
                        
                        value_to_string(bucket->list.head, buffer, length, capacity);
                        if (*buffer == NULL) {
                            return;
                        }

                        bucket = bucket->list.tail;
                        if (!value_print_buffer(buffer, length, capacity, " ]")) {
                            return;
                        }

                        if (bucket != NULL) {
                            if (!value_print_buffer(buffer, length, capacity, " ")) {
                                return;
                            }
                        }
                    }
                }

                if (first) {
                    if (!value_print_buffer(buffer, length, capacity, "] map-from-list")) {
                        return;
                    }
                } else {
                    if (!value_print_buffer(buffer, length, capacity, " ] map-from-list")) {
                        return;
                    }
                }
                break;
        }
    }
}

void value_show(Value value, char **buffer, u64 *length) {
    u64 capacity = 4096;

    *length = 0;
    *buffer = malloc(capacity * sizeof(char));
    if (*buffer != NULL) {
        value_to_string(value, buffer, length, &capacity);
    }
}

u64 value_length(Value value) {
    if (value == NULL) {
        return 0;
    } else {
        switch (value->type) {
            case SymbolValue:
                return value->symbol.length;
            case ListValue:
                return value->list.length;
            case MapValue:
                return value->map.length;
            case StringValue: 
                return value->string.length;
            default: 
                return 0;
        }
    }
}

char *value_type(Value value) {
    static char *types[] = {
        [SymbolValue]  = "symbol",
        [ListValue]    = "list",
        [NativeValue]  = "function",
        [MapValue]     = "map",
        [BooleanValue] = "boolean",
        [NumberValue]  = "number",
        [StringValue]  = "string"
    };
    
    if (value == NULL) {
        return types[ListValue];
    } else {
        return types[value->type];
    }
}
