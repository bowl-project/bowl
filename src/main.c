#include "main.h"

static void fail(Value exception) {
    fprintf(stderr, "[exception] ");
    value_dump(stderr, exception);
    fprintf(stderr, "\n");
    fflush(stderr);
    exit(EXIT_FAILURE);
}

static void enter(Stack *stack, char *name, NativeFunction function) {
    InternalResult result;

    // enter symbol
    result = machine_symbol(stack, (u8*) name, strlen(name));
    
    if (result.exception != NULL) {
        fail(result.exception);
    }
    
    stack->registers[0] = result.value;
    result = machine_native(stack, function);
    
    if (result.exception != NULL) {
        fail(result.exception);
    }
    
    stack->registers[1] = result.value;
    result = machine_map_put(stack, *stack->dictionary, stack->registers[0], stack->registers[1]);

    if (result.exception != NULL) {
        fail(result.exception);
    }

    *stack->dictionary = result.value;
}

int main(int argc, char *argv[]) {
    InternalResult result;

    Value callstack = NULL;
    Value datastack = NULL;
    Value dictionary = NULL;

    result = machine_map(NULL, 16);
    if (result.exception != NULL) {
        fail(result.exception);
    } else {
        dictionary = result.value;
    }

    Stack frame = {
        .previous = NULL,
        .registers = { NULL, NULL, NULL },
        .callstack = &callstack,
        .datastack = &datastack,
        .dictionary = &dictionary
    };

    enter(&frame, "run", machine_instruction_run);
    enter(&frame, "type", machine_instruction_type);
    enter(&frame, "hash", machine_instruction_hash);
    enter(&frame, "equals", machine_instruction_equals);
    enter(&frame, "length", machine_instruction_length);

    for (int i = argc - 1; i > 0; --i) {
        result = machine_symbol(&frame, (u8*) argv[i], strlen(argv[i]));

        if (result.exception != NULL) {
            fail(result.exception);
        }

        result = machine_list(&frame, result.value, callstack);
        
        if (result.exception != NULL) {
            fail(result.exception);
        }

        callstack = result.value;
    }

    // bootstrap datastack
    frame.registers[0] = datastack;

    result = machine_list(&frame, dictionary, datastack);
    
    if (result.exception != NULL) {
        fail(result.exception);
    }

    datastack = result.value;
    result = machine_list(&frame, callstack, datastack);

    if (result.exception != NULL) {
        fail(result.exception);
    }

    datastack = result.value;
    result = machine_list(&frame, frame.registers[0], datastack);
    
    if (result.exception != NULL) {
        fail(result.exception);
    }

    datastack = result.value;

    // run the machine using the bootstrapped datastack
    const Value exception = machine_instruction_run(&frame);
    if (exception != NULL) {
        fail(exception);
        return EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}
