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

#endif