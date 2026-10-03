#ifndef NATIVE_H
#define NATIVE_H

#include "runtime.h"

#ifdef _WIN32
#include <windows.h>
#endif

extern NativeFunc native_table[256];

void load_imports(Program *prog);
void free_imports(void);

#endif