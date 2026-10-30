#ifndef GETTERS_H
#define GETTERS_H

#include "../API/msrest_api.h"
#include "types.h" 

// Forward declarations.
typedef struct Value Value;
typedef struct Array Array;
typedef struct FloatArray FloatArray;
typedef struct BoolArray BoolArray;

uint32_t array_size(Value v);

Value array_get(Value obj, uint32_t index);
double farray_get(Value obj, uint32_t index);
uint8_t barray_get(Value obj, uint32_t index);

uint32_t string_chars(Value v);
uint32_t string_bytes(Value v);
uint8_t string_empty(Value v);

uint32_t count_utf8_chars(const uint8_t *data, size_t bytes);
const char *string_data(Value v);

/* Helpers. */
static inline int both_int(Value a, Value b)
{
    return a.type == MSREST_INT && b.type == MSREST_INT;
}

static inline uint8_t value_type(Value v)
{
    return v.type;
}


uint64_t hash_value(Value val);
uint64_t xxhash3_64(const void* data, size_t len);

#endif