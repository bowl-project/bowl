#include "module.h"

static void fail(BowlValue exception) {
    bool first = true;

    while (exception != NULL) {
        if (first) {
            first = false;
            fprintf(stderr, "[exception] ");
        } else {
            fprintf(stderr, "  caused by ");
        }
        bowl_value_dump(stderr, exception->exception.message);
        fprintf(stderr, "\n");
        fflush(stderr);
        exception = exception->exception.cause;
    }

    // call finalize to clean things up
    bowl_module_finalize(NULL, NULL);

    exit(EXIT_FAILURE);
}

void execute(char *program) {
    BowlStackFrame stack;

    BowlValue callstack = NULL;
    BowlValue datastack = NULL;
    BowlValue dictionary = NULL;

    for (u64 i = 0; i < sizeof(stack.registers) / sizeof(stack.registers[0]); ++i) {
        stack.registers[0] = NULL;
    }

    stack.previous = NULL;
    stack.callstack = &callstack;
    stack.datastack = &datastack;
    stack.dictionary = &dictionary;

    BowlResult result = bowl_string(&stack, (u8 *) program, strlen(program));

    if (result.failure) {
        fail(result.exception);
    }

    result = bowl_tokens(&stack, result.value);

    if (result.failure) {
        fail(result.exception);
    }

    callstack = result.value;

    BowlValue exception = bowl_module_initialize(&stack, NULL);

    if (exception != NULL) {
        fail(exception);
    }

    exception = bowl_module_finalize(&stack, NULL);

    if (exception != NULL) {
        fail(exception);
    }
}

BowlValue bowl_module_initialize(BowlStack stack, BowlValue library) {
    BOWL_STATIC_SYMBOL(run_symbol, "run");
   
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);

    // set up the dictionary
    BowlResult result = bowl_map(&frame, 16);
    if (result.failure) {
        return result.exception;
    }
    
    *frame.dictionary = result.value;

    // bootstrap the kernel
    result = bowl_library(&frame, (char *) bowl_settings_kernel_path);
    
    if (result.failure) {
        return result.exception;
    }

    // remember a reference of the native library to avoid it from being closed
    frame.registers[1] = result.value;

    // set up the datastack
    result = bowl_list(&frame, *frame.dictionary, *frame.datastack);
    
    if (result.failure) {
        return result.exception;
    }

    result = bowl_list(&frame, *frame.callstack, result.value);

    if (result.failure) {
        return result.exception;
    }

    result = bowl_list(&frame, *frame.datastack, result.value);
    
    if (result.failure) {
        return result.exception;
    }

    *frame.datastack = result.value;

    // the function 'run' should be present in the dictionary by now
    const BowlValue run = bowl_map_get_or_else(*frame.dictionary, &run_symbol.value, bowl_sentinel_value);

    if (run == bowl_sentinel_value) {
        return bowl_format_exception(&frame, "failed to initialize module 'kernel' in function '%s'", __FUNCTION__).value;
    } else {
        // bootstrap the first instance 
        BowlValue exception = run->function.function(&frame);

        if (exception != NULL) {
            return exception;
        }

        BowlValue unused;
        BOWL_STACK_POP_VALUE(&frame, &unused);
        BOWL_STACK_POP_VALUE(&frame, &exception);
        BOWL_STACK_POP_VALUE(&frame, &unused);

        if (exception != NULL) {
            return exception;
        }
    }

    return NULL;
}

BowlValue bowl_module_finalize(BowlStack stack, BowlValue library) {
    // run garbage collector a last time to clean things up (e.g. native libraries)
    BowlStackFrame empty = BOWL_EMPTY_STACK_FRAME(NULL);
    return bowl_collect_garbage(&empty);
}
