#include "machine.h"

// memory heaps
static u8 *machine_heap_dst = NULL;
static u8 *machine_heap_src = NULL;
static u64 machine_heap_ptr = 0;
static u64 machine_heap_size = 0;

// list of weak references to all loaded libraries
static Value *machine_heap_libraries = NULL;
static u64 machine_heap_libraries_capacity = 0;
static u64 machine_heap_libraries_size = 0;

// cached version of the out of heap exception
static struct value machine_heap_exception = {
    .type = StringValue,
    .location = NULL,
    .hash = 1393540937171791872,
    .symbol = {
        .length = 13,
        .bytes = "out of memory"
    }
};

static struct value machine_heap_library_finalize_exception = {
    .type = StringValue,
    .location = NULL,
    .hash = 365473802495556352,
    .symbol = {
        .length = 26,
        .bytes = "failed to finalize library"
    }
};

static Value machine_relocate(Value value) {
    if (value == NULL) {
        return NULL;
    } else if ((u64) value < (u64) machine_heap_src || (u64) value >= (u64) (machine_heap_src + machine_heap_size)) {
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

Value machine_collect_garbage(Stack *stack) {
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
            case NativeValue:
                value->native.library = machine_relocate(value->native.library);
                break;
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

    // finalize all libraries which are no longer needed
    bool failed_to_finalize = false;
    u64 new_libraries_size = 0;
    u64 previous_libraries_size = 0;
    for (u64 i = 0; i < machine_heap_libraries_size; ++i) {
        const Value value = machine_heap_libraries[i];

        if ((u64) value >= (u64) machine_heap_src && (u64) value < (u64) (machine_heap_src + machine_heap_size)) {
            // value is not 'NULL' and thus marks the new end of the list
            previous_libraries_size = new_libraries_size;
            new_libraries_size = i + 1;

            // this value resides inside the dirty heap
            if (value->location == NULL) {
                // this value was not relocated by the garbage collector and thus must be garbage
                #if defined(OS_UNIX)
                if (dlclose(value->library.handle) != 0) {
                #elif defined(OS_WINDOWS)
                if (FreeLibrary(value->library.handle) == 0) {
                #else
                if (false) {
                #endif
                    failed_to_finalize = true;
                } else {
                    // delete the weak reference
                    machine_heap_libraries[i] = NULL;
                    new_libraries_size = previous_libraries_size;
                }
            } else {
                // remember the new location since this library is still in use
                machine_heap_libraries[i] = value->location;
            }
        }

        // swap finalized values to pack the list of libraries over time
        if (machine_heap_libraries[i] == NULL && i + 1 < machine_heap_libraries_size && machine_heap_libraries[i + 1] != NULL) {
            machine_heap_libraries[i] = machine_heap_libraries[i + 1];
            machine_heap_libraries[i + 1] = NULL;
            i -= 1;
        }
    }

    // remember the new size to shorten future executions of the loop above
    machine_heap_libraries_size = new_libraries_size;

    if (failed_to_finalize) {
        return &machine_heap_library_finalize_exception;
    } else {
        return NULL;
    }
}

static bool machine_heap_reallocate(u64 new_heap_size) {
    if (machine_heap_src == NULL) {
        machine_heap_src = malloc(new_heap_size * sizeof(u8));
        return machine_heap_src != NULL;
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

static Value machine_heap_resize(Stack *stack, const u64 new_heap_size) {
    // as we cannot simply resize both heaps at once we have to resize the 'machine_heap_src' 
    // which is unused at this point
    if (!machine_heap_reallocate(new_heap_size)) {
        return &machine_heap_exception;
    }
    
    // copy all objects from 'machine_heap_dst' to the new 'machine_heap_src'
    const Value exception = machine_collect_garbage(stack);
    if (exception != NULL) {
        return exception;
    }
    
    // since the two heaps were swapped by the previous call we can resize 'machine_heap_src' again
    if (!machine_heap_reallocate(new_heap_size)) {
        return &machine_heap_exception;
    }

    // it is important to set the 'machine_heap_size' not until both heaps were resized, because it
    // is possible for the second resize to fail in which case both heaps are unevenly large
    machine_heap_size = new_heap_size;

    return NULL;
}

InternalResult machine_allocate(Stack *stack, ValueType type, u64 additional) {
    InternalResult result = {
        .value = NULL,
        .exception = NULL
    };

    const u64 bytes = sizeof(struct value) + additional;

    if (machine_heap_ptr + bytes > machine_heap_size) {
        // try to collect garbage
        result.exception = machine_collect_garbage(stack);
        if (result.exception != NULL) {
            return result;
        }

        // resize the heaps if there is still not enough memory available
        if (machine_heap_ptr + bytes > machine_heap_size) {
            // the new heap size is either twice as large or at least as large to contain the requested object
            const u64 minimum_heap_size = machine_heap_size + (machine_heap_ptr + bytes - machine_heap_size);
            const u64 new_heap_size = MAX(machine_heap_size * 2, minimum_heap_size);
            
            // try to resize the heap to the "best" heap size
            result.exception = machine_heap_resize(stack, new_heap_size);
            if (result.exception == &machine_heap_exception && minimum_heap_size < new_heap_size) {
                // try to resize the heap to the minimum if it is truly smaller than the "best" heap size
                result.exception = machine_heap_resize(stack, minimum_heap_size);
                if (result.exception != NULL) {
                    return result;
                }
            } else if (result.exception != NULL) {
                return result;
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

static Value machine_vexception(Stack *stack, char *message, va_list *list) {
    InternalResult result;
    char buffer[4096];
    vsnprintf(&buffer[0], sizeof(buffer) / sizeof(buffer[0]), message, *list);
    result = machine_string(stack, (u8*) &buffer[0], strlen(buffer));
    return result.exception == NULL ? result.value : result.exception;
}

Value machine_exception(Stack *stack, char *message, ...) {
    va_list list;
    va_start(list, message);
    const Value result = machine_vexception(stack, message, &list);
    va_end(list);
    return result;
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

InternalResult machine_native(Stack *stack, Value library, NativeFunction function) {
    Stack frame = MACHINE_ALLOCATE(stack, library, NULL, NULL);
    InternalResult result = machine_allocate(&frame, NativeValue, 0);
    
    if (result.exception == NULL) {
        result.value->native.library = library;
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

InternalResult machine_library(Stack *stack, char *path) {
    InternalResult result;

    result = machine_allocate(stack, LibraryValue, 0);
    if (result.exception != NULL) {
        return result;
    }

    // remember a weak reference to the library
    if (machine_heap_libraries_size >= machine_heap_libraries_capacity) {
        machine_heap_libraries_capacity = MAX(machine_heap_libraries_capacity * 2, 16);

        if (machine_heap_libraries == NULL) {
            machine_heap_libraries = malloc(machine_heap_libraries_capacity * sizeof(Value));
            if (machine_heap_libraries == NULL) {
                result.exception = &machine_heap_exception;
                result.value = NULL;
                return result;
            }
        } else {
            Value *const new_libraries = realloc(machine_heap_libraries, machine_heap_libraries_capacity * sizeof(Value));
            if (new_libraries == NULL) {
                result.exception = &machine_heap_exception;
                result.value = NULL;
                return result;
            } else {
                machine_heap_libraries = new_libraries;
            }
        }
    }

    machine_heap_libraries[machine_heap_libraries_size++] = result.value;

    // it is important to open the shared library last, otherwise there would be a case where
    // we have to close the library (e.g. in case of a 'out of memory' exception). This may 
    // fail again, thus leading to a point where we need to throw an exception which reports 
    // this failure. However, the user isn't able to react in an appropriate way to this
    // exception, because we are in the middle of the 'library' constructor and there is
    // no reference to the handle available to the user. That is, the user is not able to 
    // close the library and the internal reference counter of it will never reach zero.
    #if defined(OS_UNIX)
        void *const handle = dlopen(path, RTLD_LAZY);
    #elif defined(OS_WINDOWS)
        const HINSTANCE handle = LoadLibrary(path);
    #else
        // setting the handle to 'NULL' here triggers the error in the next line, which is 
        // desired because native libraries are not supported on this platform.
        void *const handle = NULL;
    #endif

    if (handle == NULL) {
        // decrement the size to remove the weak reference of the library
        machine_heap_libraries_size--;

        result.exception = machine_exception(stack, "failed to load library '%s' in function '%s'", path, __FUNCTION__);
        result.value = NULL;
        return result;
    }

    result.value->library.handle = handle;
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

    static struct value library_type = {
        .type = StringValue,
        .location = NULL,
        .hash = 365473802495556352,
        .symbol = {
            .length = 7,
            .bytes = "library"
        }
    };

    static Value types[] = {
        [SymbolValue] = &symbol_type,
        [ListValue] = &list_type,
        [NativeValue] = &native_type,
        [MapValue] = &map_type,
        [BooleanValue] = &boolean_type,
        [NumberValue] = &number_type,
        [StringValue] = &string_type,
        [LibraryValue] = &library_type
    };
    
    InternalResult result;

    if (*stack->datastack == NULL) {
        return machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
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
        return machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
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
        return machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
    }

    const Value a = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (*stack->datastack == NULL) {
        return machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
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
        return machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
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
        return machine_exception(stack, "argument of illegal type '%s' provided in function '%s' (expected 'list', 'string' or 'map')", value_type(value), __FUNCTION__);
    }
}

Value machine_instruction_throw(Stack *stack) {
    if (*stack->datastack == NULL) {
        return machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
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
        return machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
    }

    const Value head = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (*stack->datastack == NULL) {
        return machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
    }

    const Value tail = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (tail != NULL && tail->type != ListValue) {
        return machine_exception(stack, "argument of illegal type '%s' provided in function '%s' (expected 'list')", value_type(tail), __FUNCTION__);
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
        return machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
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
        return machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
    }

    const Value value = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (value == NULL || value->type != StringValue) {
        return machine_exception(stack, "argument of illegal type '%s' provided in function '%s' (expected 'string')", value_type(value), __FUNCTION__);
    }

    char *path = value_string_to_c_string(value);
    if (path == NULL) {
        return &machine_heap_exception;
    }

    result = machine_library(stack, path);
    free(path);

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

Value machine_instruction_native(Stack *stack) {
    InternalResult result;

    if (*stack->datastack == NULL) {
        return machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
    }

    const Value symbol = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (symbol == NULL || symbol->type != StringValue) {
        return machine_exception(stack, "argument of illegal type '%s' provided in function '%s' (expected 'string')", value_type(symbol), __FUNCTION__);
    }

    if (*stack->datastack == NULL) {
        return machine_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
    }

    const Value library = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (library == NULL || library->type != LibraryValue) {
        return machine_exception(stack, "argument of illegal type '%s' provided in function '%s' (expected 'library')", value_type(library), __FUNCTION__);
    }

    char *const symbol_name = value_string_to_c_string(symbol);
    if (symbol_name == NULL) {
        return &machine_heap_exception;
    }

    #if defined(OS_UNIX)
        Value (*const function)(Stack*) = (Value (*)(Stack*)) dlsym(library->library.handle, symbol_name);
    #elif defined(OS_WINDOWS)
        Value (*const function)(Stack*) = (Value (*)(Stack*)) GetProcAddress(library->library.handle, symbol_name);
    #else 
        Value (*const function)(Stack*) = NULL;
    #endif

    if (function == NULL) {
        const Value exception = machine_exception(stack, "failed to load native function '%s' from library in function '%s'", symbol_name, __FUNCTION__);
        free(symbol_name);
        return exception;
    }

    free(symbol_name);
    
    result = machine_native(stack, library, function);
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
        return machine_exception(&frame, "stack underflow in function '%s'", __FUNCTION__);
    }

    Value datastack = (*stack->datastack)->list.head;
    frame.datastack = &datastack;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (*stack->datastack == NULL) {
        return machine_exception(&frame, "stack underflow in function '%s'", __FUNCTION__);
    }

    Value callstack  = (*stack->datastack)->list.head;
    frame.callstack = &callstack;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (*stack->datastack == NULL) {
        return machine_exception(&frame, "stack underflow in function '%s'", __FUNCTION__);
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

Value lime_exception(Stack *stack, char *message, ...) {
    va_list list;
    va_start(list, message);
    const Value result = machine_vexception(stack, message, &list);
    va_end(list);
    return result;
}

char *lime_value_type(Value value) {
    return value_type(value);
}
