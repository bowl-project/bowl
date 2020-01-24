#include "module.h"

static void fail(LimeValue exception) {
    bool first = true;

    while (exception != NULL) {
        if (first) {
            first = false;
            fprintf(stderr, "[exception] ");
        } else {
            fprintf(stderr, "  caused by ");
        }
        lime_value_dump(stderr, exception->exception.message);
        fprintf(stderr, "\n");
        fflush(stderr);
        exception = exception->exception.cause;
    }

    // call finalize to clean things up
    lime_module_finalize(NULL, NULL);

    exit(EXIT_FAILURE);
}

void execute(char *program) {
    LimeStackFrame stack;

    LimeValue callstack = NULL;
    LimeValue datastack = NULL;
    LimeValue dictionary = NULL;

    for (u64 i = 0; i < sizeof(stack.registers) / sizeof(stack.registers[0]); ++i) {
        stack.registers[0] = NULL;
    }

    stack.previous = NULL;
    stack.callstack = &callstack;
    stack.datastack = &datastack;
    stack.dictionary = &dictionary;

    LimeResult result = lime_string(&stack, (u8 *) program, strlen(program));

    if (result.failure) {
        fail(result.exception);
    }

    result = lime_tokens(&stack, result.value);

    if (result.failure) {
        fail(result.exception);
    }

    callstack = result.value;

    LimeValue exception = lime_module_initialize(&stack, NULL);

    if (exception != NULL) {
        fail(exception);
    }

    exception = lime_module_finalize(&stack, NULL);

    if (exception != NULL) {
        fail(exception);
    }
}

LimeValue lime_module_initialize(LimeStack stack, LimeValue library) {
    LIME_STATIC_SYMBOL(run_symbol, "run");
   
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);

    // set up the dictionary
    LimeResult result = lime_map(&frame, 16);
    if (result.failure) {
        return result.exception;
    }
    
    *frame.dictionary = result.value;

    // bootstrap the kernel
    result = lime_library(&frame, (char *) lime_settings_kernel_path);
    
    if (result.failure) {
        return result.exception;
    }

    // remember a reference of the native library to avoid it from being closed
    frame.registers[1] = result.value;

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
    const LimeValue run = lime_map_get_or_else(*frame.dictionary, &run_symbol.value, lime_sentinel_value);

    if (run == lime_sentinel_value) {
        return lime_format_exception(&frame, "failed to initialize module 'kernel' in function '%s'", __FUNCTION__).value;
    } else {
        // bootstrap the first instance 
        LimeValue exception = run->function.function(&frame);

        if (exception != NULL) {
            return exception;
        }

        LimeValue unused;
        LIME_STACK_POP_VALUE(&frame, &unused);
        LIME_STACK_POP_VALUE(&frame, &exception);
        LIME_STACK_POP_VALUE(&frame, &unused);

        if (exception != NULL) {
            return exception;
        }
    }

    return NULL;
}

LimeValue lime_module_finalize(LimeStack stack, LimeValue library) {
    // run garbage collector a last time to clean things up (e.g. native libraries)
    LimeStackFrame empty = LIME_EMPTY_STACK_FRAME(NULL);
    return lime_collect_garbage(&empty);
}
