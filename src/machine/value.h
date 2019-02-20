#ifndef MACHINE_VALUE_H
#define MACHINE_VALUE_H

#include "../common/utility.h"
#include "common.h"

extern Machine *machine_instances;
extern word machine_instance_count;

extern const Value value_marker;

Value value_create(ValueType type, word additional);

Value value_symbol(word length, char *value);

Value value_number(double value);

Value value_native(word length, char *value, NativeFunction function);

Value value_native_from_string(char *string, NativeFunction function);

Value value_symbol_from_string(char *string);

Value value_string(word length, char *value);

Value value_string_from_string(char *string);

Value value_list(Value head, Value tail);

Value value_map(word bucket_count);

Value value_map_put(Value map, Value key, Value value);

Value value_map_get_or_else(Value map, Value key, Value otherwise);

word value_byte_size(Value value);

word value_hash(Value value);

bool value_equals(Value a, Value b);

char *value_to_string(Value value);

void value_dump(FILE *stream, Value value);

#endif
