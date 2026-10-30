#ifndef EXECUTE_H
#define EXECUTE_H

#include "runtime.h"
#include <math.h>

/* < STATIC METHOD DISPATCH > */
static Value invoke_static_int(Value v);
static Value invoke_static_float(Value v);
static Value invoke_static_str(Value v);
static Value invoke_static(uint32_t method, Value v);

int msrest_execute(Program *prog);

static inline uint32_t msrest_resolve_index(
    VM *vm, int ip, Value key, const char *type_name,
    uint32_t size)
{
    int64_t raw = value_to_int(key);
    int64_t idx = raw;

    if (idx < 0) idx += (int64_t)size;

    if (idx < 0 || (uint64_t)idx >= size) {
        vm->ip = ip - 9;
        msrest_panic(
            "Access violation error: %s index out of bounds: %lld "
            "(size=%u, valid range=[-%u, %u])\n",
            type_name, (long long)raw, size, size,
            size ? size - 1 : 0);
    }

    return (uint32_t)idx;
}

#endif