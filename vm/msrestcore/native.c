#include "native.h"


NativeFunc native_table[MAX_IMPORTS] = {0};

static LibHandle loaded_modules[MAX_IMPORTS];
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

        sprintf(dll_name, "../modules/%s/%s" LIB_EXT,
                prog->imports[i].module,
                prog->imports[i].module);

        LibHandle dll = lib_open(dll_name);

        uint32_t (*get_abi)(void) =
            (uint32_t (*)(void))lib_sym(dll, "msrest_abi_version");

        if (!get_abi || get_abi() != MSREST_API_ABI_VERSION) {
            printf("Module '%s' is not compatible with this Mashrest build\n",
                prog->imports[i].module);
            exit(1);
        }

        if (!dll)
        {
            printf("Cannot load module '%s'\n", dll_name);
            exit(1);
        }

        loaded_modules[loaded_count++] = dll;

        native_table[i] =
            (NativeFunc)lib_sym(
                dll,
                prog->imports[i].function);

        if (!native_table[i])
        {
            printf("Cannot find '%s' in %s\n",
                   prog->imports[i].function,
                   dll_name);
            exit(1);
        }
    }
}

void free_imports(void)
{
    for (int i = 0; i < loaded_count; i++)
        lib_close(loaded_modules[i]);

    loaded_count = 0;
}