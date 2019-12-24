#include "main.h"

static void error(char *message, ...) {
    va_list list;
    fprintf(stderr, "[exception] ");
    va_start(list, message);
    vfprintf(stderr, message, list);
    va_end(list);
    fprintf(stderr, "\n");
    fflush(stderr);
    exit(EXIT_FAILURE);
}

static void fail(LimeValue exception) {
    fprintf(stderr, "[exception] ");
    lime_value_dump(stderr, exception);
    fprintf(stderr, "\n");
    fflush(stderr);
    exit(EXIT_FAILURE);
}

static void execute(char *program) {
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

    LimeResult result = lime_string(&frame, (u8 *) program, strlen(program));

    if (result.failure) {
        fail(result.exception);
    }

    result = lime_tokens(&frame, result.value);

    if (result.failure) {
        fail(result.exception);
    }

    callstack = result.value;

    const LimeValue initialize_exception = lime_module_initialize(&frame, NULL);
    const LimeValue finalize_exception = lime_module_finalize(&frame, NULL);
    
    if (initialize_exception != NULL) {
        fail(initialize_exception);
    } else if (finalize_exception != NULL) {
        fail(finalize_exception);
    }
}

static inline bool matches_flag(char *const argument, const u64 count, ...) {
    if (argument == NULL || *argument == '\0') {
        return false;
    } else if (argument[0] != '-') {
        return false;
    }
    
    va_list list;
    va_start(list, count);
    for (u64 i = 0; i < count; ++i) {
        if (strcmp(argument + 1, va_arg(list, char *)) == 0) {
            return true;
        }
    }
    va_end(list);
    
    return false;
}

int main(int argc, char *argv[]) {
    for (u64 i = 0; i < argc; ++i) {
        if (matches_flag(argv[i], 2, "execute", "x")) {
            if (i + 1 < argc) {
                execute(argv[++i]);
            } else {
                error("missing argument for flag '%s'", argv[i]);
            }
        }
    }

    return EXIT_SUCCESS;
}

LimeValue lime_module_initialize(LimeStack stack, LimeValue library) {
    static struct lime_value run_symbol = {
        .type = LimeSymbolValue,
        .location = NULL,
        .hash = 0,
        .symbol = {
            .length = 3,
            .bytes = "run"
        }
    };

    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);

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
    const LimeValue run = lime_map_get_or_else(*frame.dictionary, &run_symbol, lime_sentinel_value);

    if (run == lime_sentinel_value) {
        return lime_exception(&frame, "failed to initialize module 'kernel' in function '%s'", __FUNCTION__);
    } else {
        // bootstrap the first instance 
        return run->function.function(&frame);
    }
}

LimeValue lime_module_finalize(LimeStack stack, LimeValue library) {
    // run garbage collector a last time to clean things up (e.g. native libraries)
    LimeStackFrame empty = LIME_EMPTY_STACK_FRAME(NULL);
    return lime_collect_garbage(&empty);
}
