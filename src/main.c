#include "main.h"

#include "machine/value.h"
#include "machine/machine.h"

int main(int argc, char *argv[]) {
    Machine machine = machine_create();

    for (word i = argc - 1; i > 0; --i) {
        char *end_ptr;
        double number;

        number = strtod(argv[i], &end_ptr);
        const word arglen = strlen(argv[i]);

        if (end_ptr == argv[i] + arglen) {
            machine->$0 = value_number(number);
        } else if (arglen == 4 && memcmp(argv[i], "true", arglen) == 0) {
            machine->$0 = value_boolean(true);
        } else if (arglen == 5 && memcmp(argv[i], "false", arglen) == 0) {
            machine->$0 = value_boolean(false);
        } else {
            machine->$0 = value_symbol_from_string(argv[i]);
        }

        machine->callstack = value_list(machine->$0, machine->callstack);
    }
    machine->$0 = NULL;

    machine_run(machine);

    machine_delete(machine);
    return EXIT_SUCCESS;
}
