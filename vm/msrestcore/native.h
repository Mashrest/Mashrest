#ifndef NATIVE_H
#define NATIVE_H

#include "runtime.h"

extern NativeFunc native_table[MAX_IMPORTS]; // массив указателей на функции.

typedef struct LoadedModule {
    LibHandle handle;
    MsrestAPI *api;       // указатель на malloc копии.
    int in_use;
    char name[64];
} LoadedModule;

extern LoadedModule g_modules[MAX_IMPORTS];
extern int g_import_module[MAX_IMPORTS];  // import_idx -> module_idx

void load_imports(Program *prog);
void free_imports(void);

#endif