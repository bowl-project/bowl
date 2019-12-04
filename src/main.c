#include "main.h"

static void fail(LimeValue exception) {
    fprintf(stderr, "[exception] ");
    lime_value_dump(stderr, exception);
    fprintf(stderr, "\n");
    fflush(stderr);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    // TODO: predefined list of static allocated exceptions
    // TODO: if there would be a "init" and "shutdown" function in each library, we could prevent fetching the "kernel_run"
    LimeValue callstack = NULL;
    LimeValue datastack = NULL;
    LimeValue dictionary = NULL;

    LimeStackFrame frame = {
        .previous = NULL,
        .registers = { NULL, NULL, NULL },
        .callstack = &callstack,
        .datastack = &datastack,
        .dictionary = &dictionary
    };

    for (int i = argc - 1; i > 0; --i) {
        LimeResult result = lime_symbol(&frame, (u8*) argv[i], strlen(argv[i]));

        if (result.failure) {
            fail(result.exception);
        }

        result = lime_list(&frame, result.value, callstack);
        
        if (result.failure) {
            fail(result.exception);
        }

        callstack = result.value;
    }

    LimeValue exception = lime_module_initialize(&frame);
    
    if (exception == NULL) {
        exception = lime_module_finalize(NULL);
    }
     
    if (exception == NULL) {
        return EXIT_SUCCESS;
    } else {
        fail(exception);
        return EXIT_FAILURE;
    }
}

LimeValue lime_module_initialize(LimeStack stack) {
    static struct lime_value marker = {
        .type = LimeSymbolValue,
        .location = NULL,
        .hash = 31,
        .symbol = {
            .length = 0
        }
    };
    
    static struct lime_value run_symbol = {
        .type = LimeSymbolValue,
        .location = NULL,
        .hash = 0,
        .symbol = {
            .length = 3,
            .bytes = "run"
        }
    };

    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, NULL, NULL, NULL);

    // set up the dictionary
    LimeResult result = lime_map(&frame, 16);
    if (result.failure) {
        return result.exception;
    }
        
    *frame.dictionary = result.value;

    // bootstrap the kernel
    result = lime_library(&frame, "./../lime-kernel/kernel.so");
    
    if (result.failure) {
        return result.exception;
    }

    // remember a reference of the native library to avoid it from being closed
    frame.registers[0] = result.value;

    // set up the datastack
    result = lime_list(&frame, *frame.dictionary, *frame.datastack);
    
    if (result.failure) {
        return result.exception;
    }

    result = lime_list(&frame, *frame.callstack, result.value);

    if (result.failure) {
        return result.exception;
    }

    result = lime_list(&frame, *frame.datastack, result.value);
    
    if (result.failure) {
        return result.exception;
    }

    *frame.datastack = result.value;

    // the function 'run' should be present in the dictionary by now
    const LimeValue run = lime_map_get_or_else(*frame.dictionary, &run_symbol, &marker);

    if (run == &marker) {
        return lime_exception(&frame, "failed to initialize module 'kernel' in function '%s'", __FUNCTION__);
    } else {
        // bootstrap the first instance 
        return run->function.function(&frame);
    }
}

LimeValue lime_module_finalize(LimeStack stack) {
    // run garbage collector a last time to clean things up (e.g. native libraries)
    return lime_collect_garbage(NULL);
}
