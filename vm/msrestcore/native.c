#include "native.h"


NativeFunc native_table[MAX_IMPORTS] = {0};

static HMODULE loaded_modules[MAX_IMPORTS];
static int loaded_count = 0;

void load_imports(Program *prog)
{
    //printf("Loading native imports...\n");

    if (prog->import_count > MAX_IMPORTS) {
        printf("Too many imports: %d (max %d)\n", prog->import_count, MAX_IMPORTS);
        exit(1);
    }

    for (int i = 0; i < prog->import_count; i++)
    {
        char dll_name[256];

        sprintf(dll_name, "../modules/%s/%s.dll",
        prog->imports[i].module,
        prog->imports[i].module);

        HMODULE dll = LoadLibraryA(dll_name);

        if (!dll)
        {
            printf("Cannot load %s\n", dll_name);
            exit(1);
        }

        loaded_modules[loaded_count++] = dll;

        native_table[i] =
            (NativeFunc)GetProcAddress(
                dll,
                prog->imports[i].function);

                //printf("dll=%p\n", dll);

native_table[i] =
    (NativeFunc)GetProcAddress(
        dll,
        prog->imports[i].function);

//printf("fn=%p\n", native_table[i]);

        if (!native_table[i])
        {
            printf("Cannot find %s in %s\n",
                   prog->imports[i].function,
                   dll_name);
            exit(1);
        }

        /*printf("%d -> %s::%s\n",
               i,
               prog->imports[i].module,
               prog->imports[i].function);*/
    }
}

void free_imports(void)
{
    for (int i = 0; i < loaded_count; i++)
        FreeLibrary(loaded_modules[i]);

    loaded_count = 0;
}