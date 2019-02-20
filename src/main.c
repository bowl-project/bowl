#include "main.h"

#include "machine/value.h"
#include "machine/machine.h"

int main(int argc, char *argv[]) {
    Machine machine = machine_create();

    for (word i = argc - 1; i > 0; --i) {
        char *end_ptr;
        double number;

        number = strtod(argv[i], &end_ptr);

        if (end_ptr == argv[i] + strlen(argv[i])) {
            machine->$0 = value_number(number);
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
