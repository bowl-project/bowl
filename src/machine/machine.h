#ifndef MACHINE_MACHINE_H
#define MACHINE_MACHINE_H

#include "../common/utility.h"
#include "common.h"
#include "value.h"

#define DEBUG 0

extern Machine *machine_instances;
extern word machine_instance_count;

Machine machine_create(void);

void machine_delete(Machine machine);

void machine_step(Machine machine);

void machine_run(Machine machine);

void machine_equals(Machine machine);

void machine_quote(Machine machine);

void machine_dup(Machine machine);

void machine_exit(Machine machine);

void machine_swap(Machine machine);

void machine_rot(Machine machine);

void machine_drop(Machine machine);

void machine_read(Machine machine);

void machine_input(Machine machine);

void machine_show(Machine machine);

void machine_tokens(Machine machine);

void machine_invoke(Machine machine);

void machine_head(Machine machine);

void machine_tail(Machine machine);

void machine_split(Machine machine);

void machine_nil(Machine machine);

void machine_length(Machine machine);

void machine_print(Machine machine);

void machine_concatenate(Machine machine);

void machine_continue(Machine machine);

void machine_prepend(Machine machine);

void machine_put(Machine machine);

void machine_get(Machine machine);

void machine_empty_map(Machine machine);

void machine_get_or_else(Machine machine);

void machine_reverse(Machine machine);

void machine_if(Machine machine);

void machine_and(Machine machine);

void machine_or(Machine machine);

void machine_xor(Machine machine);

void machine_not(Machine machine);

void machine_less_than(Machine machine);

void machine_less_equal(Machine machine);

void machine_greater_than(Machine machine);

void machine_greater_equal(Machine machine);

void machine_add(Machine machine);

void machine_sub(Machine machine);

void machine_mul(Machine machine);

void machine_div(Machine machine);

void machine_rem(Machine machine);

void machine_library(Machine machine);

void machine_get_directory(Machine machine);

void machine_change_directory(Machine machine);

void machine_execute(Machine machine);

#endif
