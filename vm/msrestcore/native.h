#ifndef NATIVE_H
#define NATIVE_H

#include "runtime.h"

extern NativeFunc native_table[MAX_IMPORTS];

void load_imports(Program *prog);
void free_imports(void);

#endif