#include "machine.h"

static u8 *machine_heap_dst = NULL;
static u8 *machine_heap_src = NULL;
static u64 machine_heap_ptr = 0;
static u64 machine_heap_size = 0;
static struct value machine_heap_exception = {
    .type = StringValue,
    .location = NULL,
    .hash = 1393540937171791872,
    .symbol = {
        .length = 13,
        .bytes = "out of memory"
    }
};

static Value machine_relocate(Value value) {
    if (value == NULL) {
        return NULL;
    } else if ((u64) value < (u64) machine_heap_src || (u64) value > (u64) (machine_heap_src + machine_heap_size)) {
        // only objects that resize inside the 'machine_heap_src' are managed by the garbage collector
        return value;
    } else if (value->location == NULL) {
        const Value copy = (Value) (machine_heap_dst + machine_heap_ptr);
        const u64 bytes = value_byte_size(value);

        machine_heap_ptr += bytes;
        memcpy(copy, value, bytes);

        value->location = copy;
    }

    return value->location;
}

void machine_collect_garbage(Stack *stack) {
    // swap heaps
    u8 *const swap = machine_heap_dst;
    machine_heap_dst = machine_heap_src;
    machine_heap_src = swap;
    machine_heap_ptr = 0;

    // mark the root objects
    while (stack != NULL) {
        for (u64 i = 0; i < sizeof(stack->registers) / sizeof(stack->registers[0]); ++i) {
            stack->registers[i] = machine_relocate(stack->registers[i]);
        }

        *stack->dictionary = machine_relocate(*stack->dictionary);
        *stack->callstack = machine_relocate(*stack->callstack);
        *stack->datastack = machine_relocate(*stack->datastack);

        stack = stack->previous;
    }

    // relocate all objects which are reachable from the root objects
    u64 scan = 0;
    while (scan < machine_heap_ptr) {
        const Value value = (Value) (machine_heap_dst + scan);
        const u64 bytes = value_byte_size(value);

        switch (value->type) {
            case ListValue:
                value->list.head = machine_relocate(value->list.head);
                value->list.tail = machine_relocate(value->list.tail);
                break;
            case MapValue:
                for (u64 i = 0, end = value->map.capacity; i < end; ++i) {
                    value->map.buckets[i] = machine_relocate(value->map.buckets[i]);
                }
                break;
            default:
                // not a compound type
                break;
        }

        scan += bytes;
    }
}

static bool machine_heap_reallocate(u64 new_heap_size) {
    if (machine_heap_src == NULL) {
        machine_heap_src = malloc(new_heap_size * sizeof(u8));
        if (machine_heap_src == NULL) {
            return false;
        } else {
            return true;
        }
    } else {
        u8 *const new_heap_src = realloc(machine_heap_src, new_heap_size * sizeof(u8));
        if (new_heap_src == NULL) {
            // if 'new_heap_src' is 'NULL' the reallocation failed, but the old heap is still intact
            return false;
        } else {
            machine_heap_src = new_heap_src;
            return true;
        }
    }
}

static bool machine_heap_resize(Stack *stack, const u64 new_heap_size) {
    // as we cannot simply resize both heaps at once we have to resize the 'machine_heap_src' 
    // which is unused at this point
    if (!machine_heap_reallocate(new_heap_size)) {
        return false;
    }
    
    // copy all objects from 'machine_heap_dst' to the new 'machine_heap_src'
    machine_collect_garbage(stack);
    
    // since the two heaps were swapped by the previous call we can resize 'machine_heap_src' again
    if (!machine_heap_reallocate(new_heap_size)) {
        return false;
    }

    // it is important to set the 'machine_heap_size' not until both heaps were resized, because it
    // is possible for the second resize to fail in which case both heaps are unevenly large
    machine_heap_size = new_heap_size;

    return true;
}

InternalResult machine_allocate(Stack *stack, ValueType type, u64 additional) {
    InternalResult result = {
        .value = NULL,
        .exception = NULL
    };

    const u64 bytes = sizeof(struct value) + additional;

    if (machine_heap_ptr + bytes > machine_heap_size) {
        // try to collect garbage
        machine_collect_garbage(stack);

        // resize the heaps if there is still not enough memory available
        if (machine_heap_ptr + bytes > machine_heap_size) {
            // the new heap size is either twice as large or at least as large to contain the requested object
            const u64 minimum_heap_size = machine_heap_size + (machine_heap_ptr + bytes - machine_heap_size);
            const u64 new_heap_size = MAX(machine_heap_size * 2, minimum_heap_size);
            
            // try to resize the heap to the "best" heap size
            if (!machine_heap_resize(stack, new_heap_size)) {

                // try to resize the heap to the minimum if it is truly smaller than the "best" heap size
                if (!(minimum_heap_size < new_heap_size && machine_heap_resize(stack, minimum_heap_size))) {
                    result.exception = &machine_heap_exception;
                    return result;  
                }
            }
        }
    }

    result.value = (Value) (machine_heap_dst + machine_heap_ptr);
    machine_heap_ptr += bytes;

    result.value->type = type;
    result.value->location = NULL;
    result.value->hash = 0;

    return result;
}

/* ***** internals ***** */

InternalResult machine_exception(Stack *stack, char *message, ...) {
    char buffer[4096];
    va_list list;
    va_start(list, message);
    vsnprintf(&buffer[0], sizeof(buffer) / sizeof(buffer[0]), message, list);
    va_end(list);
    return machine_string(stack, (u8*) &buffer[0], strlen(buffer));
}

InternalResult machine_symbol(Stack *stack, u8 *bytes, u64 length) {
    InternalResult result = machine_allocate(stack, SymbolValue, length * sizeof(u8));

    if (result.exception == NULL) {
        result.value->symbol.length = length;
        memcpy(result.value->symbol.bytes, bytes, length * sizeof(u8));
        value_hash(result.value);    
    }

    return result;
}

InternalResult machine_string(Stack *stack, u8 *bytes, u64 length) {
    InternalResult result = machine_allocate(stack, StringValue, length * sizeof(u8));

    if (result.exception == NULL) {
        result.value->string.length = length;
        memcpy(result.value->string.bytes, bytes, length * sizeof(u8));
    }

    return result;
}

InternalResult machine_native(Stack *stack, NativeFunction function) {
    InternalResult result = machine_allocate(stack, NativeValue, 0);
    
    if (result.exception == NULL) {
        result.value->native.function = function;
    }

    return result;
}

InternalResult machine_list(Stack *stack, Value head, Value tail) {
    Stack frame = MACHINE_ALLOCATE(stack, head, tail, NULL);
    InternalResult result = machine_allocate(&frame, ListValue, 0);

    if (result.exception == NULL) {
        result.value->list.head = frame.registers[0];
        result.value->list.tail = frame.registers[1];

        if (result.value->list.tail == NULL) {
            result.value->list.length = 1;
        } else {
            result.value->list.length = result.value->list.tail->list.length + 1;
        }
    }

    return result;
}

InternalResult machine_map(Stack *stack, u64 capacity) {
    InternalResult result = machine_allocate(stack, MapValue, capacity * sizeof(Value));

    if (result.exception == NULL) {
        result.value->map.capacity = capacity;
        result.value->map.length = 0;
        
        for (u64 i = 0; i < capacity; ++i) {
            result.value->map.buckets[i] = NULL;
        }
    }
    
    return result;
}

InternalResult machine_number(Stack *stack, double value) {
    InternalResult result = machine_allocate(stack, NumberValue, 0);

    if (result.exception == NULL) {
        result.value->number.value = value;
    }
    
    return result;
}

InternalResult machine_boolean(Stack *stack, bool value) {
    InternalResult result = machine_allocate(stack, BooleanValue, 0);
    
    if (result.exception == NULL) {
        result.value->boolean.value = value;
    }

    return result;
}

u64 machine_list_length(Stack *stack, Value value) {
    return value == NULL ? 0 : value->list.length;
}

static InternalResult machine_map_insert(Stack *stack, Value bucket, Value key, Value value) {
    Stack arguments = MACHINE_ALLOCATE(stack, bucket, key, value);
    Stack variables = MACHINE_ALLOCATE(&arguments, NULL, NULL, NULL);
    InternalResult result;
    bool found = false;

    variables.registers[0] = NULL;
    while (arguments.registers[0] != NULL) {
        if (!found && value_equals(arguments.registers[1], arguments.registers[0]->list.head)) {
            result = machine_list(&variables, arguments.registers[2], variables.registers[0]);

            if (result.exception != NULL) {
                return result;
            }

            variables.registers[0] = result.value;
            result = machine_list(&variables, arguments.registers[1], variables.registers[0]);

            if (result.exception != NULL) {
                return result;
            }

            variables.registers[0] = result.value;
            found = true;
        } else {
            result = machine_list(&variables, arguments.registers[0]->list.tail->list.head, variables.registers[0]);

            if (result.exception != NULL) {
                return result;
            }

            variables.registers[0] = result.value;
            result = machine_list(&variables, arguments.registers[0]->list.head, variables.registers[0]);

            if (result.exception != NULL) {
                return result;
            }

            variables.registers[0] = result.value;
        }

        arguments.registers[0] = arguments.registers[0]->list.tail->list.tail;
    }

    if (!found) {
        result = machine_list(&variables, arguments.registers[2], variables.registers[0]);

        if (result.exception != NULL) {
            return result;
        }

        variables.registers[0] = result.value;
        result = machine_list(&variables, arguments.registers[1], variables.registers[0]);

        if (result.exception != NULL) {
            return result;
        }

        variables.registers[0] = result.value;
    }

    result.exception = NULL;
    result.value = variables.registers[0];

    return result;
}

InternalResult machine_map_put(Stack *stack, Value map, Value key, Value value) {
    Stack arguments = MACHINE_ALLOCATE(stack, map, key, value);
    Stack variables = MACHINE_ALLOCATE(&arguments, NULL, NULL, NULL);
    InternalResult result;

    // resize capacity if it exceeds the load factor
    u64 capacity = arguments.registers[0]->map.capacity;
    if (arguments.registers[0]->map.length + 1 >= capacity * MACHINE_MAP_LOAD_FACTOR) {
        capacity *= 2;
    }

    // copy the buckets
    result = machine_map(&variables, capacity);
    if (result.exception != NULL) {
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

                const u64 index = value_hash(variables.registers[2]) % capacity;
                result = machine_map_insert(
                    &variables,
                    variables.registers[0]->map.buckets[index],
                    variables.registers[2],
                    variables.registers[1]->list.head
                );

                if (result.exception != NULL) {
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
    const u64 index = value_hash(arguments.registers[1]) % capacity;
    const u64 length = machine_list_length(&variables, variables.registers[0]->map.buckets[index]);
    
    result = machine_map_insert(
        &variables, 
        variables.registers[0]->map.buckets[index], 
        arguments.registers[1], 
        arguments.registers[2]
    );

    if (result.exception != NULL) {
        return result;
    }

    variables.registers[0]->map.buckets[index] = result.value;

    if (machine_list_length(&variables, result.value) > length) {
        variables.registers[0]->map.length += 1;
    }

    result.exception = NULL;
    result.value = variables.registers[0];

    return result;
}

/* ***** instructions ***** */

Value machine_instruction_type(Stack *stack) {
    // static cache for type names
    static struct value symbol_type = {
        .type = StringValue,
        .location = NULL,
        .hash = 576032489876310016,
        .symbol = {
            .length = 6,
            .bytes = "symbol"
        }
    };

    static struct value list_type = {
        .type = StringValue,
        .location = NULL,
        .hash = 37538678814,
        .symbol = {
            .length = 4,
            .bytes = "list"
        }
    };

    static struct value native_type = {
        .type = StringValue,
        .location = NULL,
        .hash = 461164099340126272,
        .symbol = {
            .length = 8,
            .bytes = "function"
        }
    };

    static struct value map_type = {
        .type = StringValue,
        .location = NULL,
        .hash = 11420680,
        .symbol = {
            .length = 3,
            .bytes = "map"
        }
    };

    static struct value boolean_type = {
        .type = StringValue,
        .location = NULL,
        .hash = 482558078189590912,
        .symbol = {
            .length = 7,
            .bytes = "boolean"
        }
    };

    static struct value number_type = {
        .type = StringValue,
        .location = NULL,
        .hash = 461248435778548160,
        .symbol = {
            .length = 6,
            .bytes = "number"
        }
    };

    static struct value string_type = {
        .type = StringValue,
        .location = NULL,
        .hash = 576001746853242496,
        .symbol = {
            .length = 6,
            .bytes = "string"
        }
    };

    static Value types[] = {
        [SymbolValue] = &symbol_type,
        [ListValue] = &list_type,
        [NativeValue] = &native_type,
        [MapValue] = &map_type,
        [BooleanValue] = &boolean_type,
        [NumberValue] = &number_type,
        [StringValue] = &string_type
    };
    
    InternalResult result;

    if (*stack->datastack == NULL) {
        result = machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    const Value value = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (value == NULL) {
        result = machine_list(stack, types[ListValue], *stack->datastack);
    } else {
        result = machine_list(stack, types[value->type], *stack->datastack);
    }

    if (result.exception != NULL) {
        return result.exception;
    } else {
        *stack->datastack = result.value;
        return NULL;
    }
}

Value machine_instruction_hash(Stack *stack) {
    InternalResult result;

    if (*stack->datastack == NULL) {
        result = machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    const Value value = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    const u64 hash = value_hash(value);
    result = machine_number(stack, (double) hash);

    if (result.exception != NULL) {
        return result.exception;
    }

    result = machine_list(stack, result.value, (*stack->datastack));

    if (result.exception != NULL) {
        return result.exception;
    } else {
        (*stack->datastack) = result.value;
        return NULL;
    }
}

Value machine_instruction_equals(Stack *stack) {
    InternalResult result;

    if (*stack->datastack == NULL) {
        result = machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    const Value a = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (*stack->datastack == NULL) {
        result = machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    const Value b = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    const bool equals = value_equals(a, b);
    result = machine_boolean(stack, equals);

    if (result.exception != NULL) {
        return result.exception;
    }

    result = machine_list(stack, result.value, *stack->datastack);
    if (result.exception != NULL) {
        return result.exception;
    } else {
        *stack->datastack = result.value;
        return NULL;
    }
}

Value machine_instruction_length(Stack *stack) {
    InternalResult result;

    if (*stack->datastack == NULL) {
        result = machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    const Value value = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    const u64 length = value_length(value);
    if (value == NULL || value->type == ListValue || value->type == MapValue || value->type == StringValue) {
        result = machine_number(stack, (double) length);
            
        if (result.exception != NULL) {
            return result.exception;
        }

        result = machine_list(stack, result.value, *stack->datastack);

        if (result.exception != NULL) {
            return result.exception;
        } else {
            *stack->datastack = result.value;
            return NULL;   
        }
    } else {
        result = machine_exception(stack, "argument of illegal type '%s' provided in function '%s' (expected 'list', 'string' or 'map')", value_type(value), __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }
}

Value machine_instruction_throw(Stack *stack) {
    if (*stack->datastack == NULL) {
        InternalResult result = machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    const Value value = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    return value;
}

Value machine_instruction_nil(Stack *stack) {
    InternalResult result = machine_list(stack, NULL, *stack->datastack);
    if (result.exception != NULL) {
        return result.exception;
    } else {
        *stack->datastack = result.value;
        return NULL;
    }
}

Value machine_instruction_push(Stack *stack) {
    InternalResult result;

    if (*stack->datastack == NULL) {
        result = machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    const Value head = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (*stack->datastack == NULL) {
        result = machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    const Value tail = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (tail != NULL && tail->type != ListValue) {
        result = machine_exception(stack, "argument of illegal type '%s' provided in function '%s' (expected 'list')", value_type(tail), __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    result = machine_list(stack, head, tail);
    if (result.exception != NULL) {
        return result.exception;
    }

    result = machine_list(stack, result.value, *stack->datastack);
    if (result.exception != NULL) {
        return result.exception;
    } else {
        *stack->datastack = result.value;
        return NULL;
    }
}

Value machine_instruction_show(Stack *stack) {
    InternalResult result;

    if (*stack->datastack == NULL) {
        result = machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    const Value value = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    char *buffer;
    u64 length;
    value_show(value, &buffer, &length);

    if (buffer == NULL) {
        return &machine_heap_exception;
    }

    result = machine_string(stack, (u8*) buffer, length);
    free(buffer);

    if (result.exception != NULL) {
        return result.exception;
    }

    result = machine_list(stack, result.value, *stack->datastack);
    if (result.exception != NULL) {
        return result.exception;
    } else {
        *stack->datastack = result.value;
        return NULL;
    }
}

Value machine_instruction_library(Stack *stack) {
    InternalResult result;

    if (*stack->datastack == NULL) {
        result = machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    const Value value = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (value == NULL || value->type != StringValue) {
        result = machine_exception(stack, "argument of illegal type '%s' provided in function '%s' (expected 'string')", value_type(value), __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    char *path = malloc(value->string.length + 1);
    if (path == NULL) {
        return &machine_heap_exception;
    }

    memcpy(path, value->string.bytes, value->string.length);
    path[value->string.length] = '\0';

    #if defined(unix) || defined(__unix__) || defined(__unix)
    void *const handle = dlopen(path, RTLD_LAZY);

    if (handle == NULL) {
        result = machine_exception(stack, "failed to open library '%s' in function '%s'", path, __FUNCTION__);
        free(path);
        return result.exception == NULL ? result.value : result.exception;
    }

    Value (*initialize)(Stack*) = dlsym(handle, "initialize");
    if (initialize == NULL) {
        dlclose(handle);
        result = machine_exception(stack, "failed to initialize library '%s' in function '%s'", path, __FUNCTION__); 
        free(path);
        return result.exception == NULL ? result.value : result.exception;
    }

    const Value exception = initialize(stack);
    if (exception != NULL) {
        return exception;
    }
    #endif

    return NULL;
}

Value machine_instruction_run(Stack *stack) {
    static struct value marker = {
        .type = SymbolValue,
        .location = NULL,
        .hash = 31,
        .symbol = {
            .length = 0
        }
    };

    Stack frame = MACHINE_ALLOCATE(stack, NULL, NULL, NULL);
    InternalResult result;

    if (*stack->datastack == NULL) {
        result = machine_exception(&frame, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    Value datastack = (*stack->datastack)->list.head;
    frame.datastack = &datastack;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (*stack->datastack == NULL) {
        result = machine_exception(&frame, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    Value callstack  = (*stack->datastack)->list.head;
    frame.callstack = &callstack;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (*stack->datastack == NULL) {
        result = machine_exception(&frame, "stack underflow in function '%s'", __FUNCTION__);
        return result.exception == NULL ? result.value : result.exception;
    }

    Value dictionary = (*stack->datastack)->list.head;
    frame.dictionary = &dictionary;
    *stack->datastack = (*stack->datastack)->list.tail;

    while (callstack != NULL) {
        const Value instruction = callstack->list.head;
        callstack = callstack->list.tail;

        #if MACHINE_PRINT_DATASTACK
        printf("\n[before]: ");
        value_dump(stdout, datastack);
        fputc('\n', stdout);
        fflush(stdout);
        #endif

        switch (instruction->type) {
            case SymbolValue:
                {
                    // look up the symbol in dictionary and push associated value to the callstack
                    frame.registers[0] = value_map_get_or_else(dictionary, instruction, &marker);
                    if (frame.registers[0] == &marker) {
                        // symbol not found => push it to the datastack
                        result = machine_list(&frame, instruction, datastack);

                        if (result.exception != NULL) {
                            return result.exception;
                        }

                        datastack = result.value;
                    } else if (frame.registers[0]->type == ListValue) {
                        // push the associated list to the callstack
                        while (frame.registers[0] != NULL) {
                            result = machine_list(&frame, frame.registers[0]->list.head, callstack);

                            if (result.exception != NULL) {
                                return result.exception;
                            }

                            callstack = result.value;
                            frame.registers[0] = frame.registers[0]->list.tail;
                        }
                    } else if (frame.registers[0]->type == NativeValue) {
                        const NativeFunction function = frame.registers[0]->native.function;
                        const Value exception = function(&frame);
                        if (exception != NULL) {
                            return exception;
                        }
                    } else {
                        result = machine_list(&frame, frame.registers[0], callstack);

                        if (result.exception != NULL) {
                            return result.exception;
                        }

                        callstack = result.value;
                    }
                }
                break;
            default:
                result = machine_list(&frame, instruction, datastack);

                if (result.exception != NULL) {
                    return result.exception;
                }

                datastack = result.value;
                break;
        }

        #if MACHINE_PRINT_DATASTACK
        printf("[after]:  ");
        value_dump(stdout, datastack);
        fputc('\n', stdout);
        fflush(stdout);
        #endif
    }

    return NULL;
}

/* ***** foreign function interface ***** */

Value lime_register_function(Stack *stack, char *name, NativeFunction function) {
    Stack frame = MACHINE_ALLOCATE(stack, NULL, NULL, NULL);
    InternalResult result;

    result = machine_symbol(&frame, (u8*) name, strlen(name));
    
    if (result.exception != NULL) {
        return result.exception;
    }
    
    frame.registers[0] = result.value;
    result = machine_native(&frame, function);
    
    if (result.exception != NULL) {
        return result.exception;
    }
    
    frame.registers[1] = result.value;
    result = machine_map_put(&frame, *frame.dictionary, frame.registers[0], frame.registers[1]);

    if (result.exception != NULL) {
        return result.exception;
    }

    *frame.dictionary = result.value;

    return NULL;
}

Value lime_exception(Stack *stack, char *message, ...) {
    InternalResult result;
    char buffer[4096];
    va_list list;

    va_start(list, message);
    vsnprintf(&buffer[0], sizeof(buffer) / sizeof(buffer[0]), message, list);
    va_end(list);
    
    result = machine_string(stack, (u8*) &buffer[0], strlen(buffer));
    return result.exception == NULL ? result.value : result.exception;
}

char *lime_value_type(Value value) {
    return value_type(value);
}
