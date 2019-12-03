#include "main.h"

static void fail(LimeValue exception) {
    fprintf(stderr, "[exception] ");
    lime_value_dump(stderr, exception);
    fprintf(stderr, "\n");
    fflush(stderr);
    exit(EXIT_FAILURE);
}

static void enter(LimeStack stack, char *name, LimeFunction function) {
    LimeResult result;

    // enter symbol
    result = lime_symbol(stack, (u8*) name, strlen(name));
    
    if (result.failure) {
        fail(result.exception);
    }
    
    stack->registers[0] = result.value;
    result = lime_function(stack, NULL, function);
    
    if (result.failure) {
        fail(result.exception);
    }
    
    stack->registers[1] = result.value;
    result = lime_map_put(stack, *stack->dictionary, stack->registers[0], stack->registers[1]);

    if (result.failure) {
        fail(result.exception);
    }

    *stack->dictionary = result.value;
}

int main(int argc, char *argv[]) {
    // TODO: predefined list of static allocated exceptions
    // TODO: if there would be a "init" and "shutdown" function in each library, we could prevent fetching the "kernel_run"
    LimeResult result;

    LimeValue callstack = NULL;
    LimeValue datastack = NULL;
    LimeValue dictionary = NULL;

    result = lime_map(NULL, 16);
    if (result.failure) {
        fail(result.exception);
    } else {
        dictionary = result.value;
    }

    LimeStackFrame frame = {
        .previous = NULL,
        .registers = { NULL, NULL, NULL },
        .callstack = &callstack,
        .datastack = &datastack,
        .dictionary = &dictionary
    };

    for (int i = argc - 1; i > 0; --i) {
        result = lime_symbol(&frame, (u8*) argv[i], strlen(argv[i]));

        if (result.failure) {
            fail(result.exception);
        }

        result = lime_list(&frame, result.value, callstack);
        
        if (result.failure) {
            fail(result.exception);
        }

        callstack = result.value;
    }

    // bootstrap datastack
    frame.registers[0] = datastack;

    result = lime_list(&frame, dictionary, datastack);
    
    if (result.failure) {
        fail(result.exception);
    }

    datastack = result.value;
    result = lime_list(&frame, callstack, datastack);

    if (result.failure) {
        fail(result.exception);
    }

    datastack = result.value;
    result = lime_list(&frame, frame.registers[0], datastack);
    
    if (result.failure) {
        fail(result.exception);
    }

    datastack = result.value;

    // bootstrap the kernel
    result = lime_library(&frame, "./../lime-kernel/kernel.so");
    
    if (result.failure) {
        fail(result.exception);
    }

    const LimeFunction function = dlsym(result.value->library.handle, "kernel_run");
    result = lime_function(&frame, result.value, function);

    if (result.failure) {
        fail(result.exception);
    }

    function(&frame);

    // run garbage collector a last time to clean things up (e.g. native libraries)
    LimeValue exception = lime_collect_garbage(NULL);

    if (exception != NULL) {
        fail(exception);
        return EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}
