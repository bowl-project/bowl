#include "io.h"

// try: ./lime "./library/io/io.so" show library "hello world" show print
static Value io_print(Stack *stack) {
    if (*stack->datastack == NULL) {
        return lime_exception(stack, "stack underflow in function '%s'", __FUNCTION__);
    }

    const Value value = (*stack->datastack)->list.head;
    *stack->datastack = (*stack->datastack)->list.tail;

    if (value == NULL || value->type != StringValue) {
        return lime_exception(stack, "argument of illegal type '%s' provided in function '%s' (expected 'string')", lime_value_type(value), __FUNCTION__);
    }

    if (fwrite(value->string.bytes, sizeof(u8), value->string.length, stdout) != value->string.length) {
        return lime_exception(stack, "io exception in function '%s'", __FUNCTION__);
    }

    return NULL;
}

Value initialize(Stack *stack) {
    Value exception;
    
    exception = lime_register_function(stack, "print", io_print);
    if (exception != NULL) {
        return exception;
    }

    return NULL;
}
