#include "native.h"


NativeFunc native_table[MAX_IMPORTS] = {0};

static LibHandle loaded_modules[MAX_IMPORTS];
static int loaded_count = 0;


LoadedModule g_modules[MAX_IMPORTS] = {0};
int g_import_module[MAX_IMPORTS] = {0};

/* Эта функция проверяет имя модуля, ищет, открыт ли он. */
static int find_module(const char *name) {
    for (int i = 0; i < MAX_IMPORTS; i++)
        if (g_modules[i].in_use && strcmp(g_modules[i].name, name) == 0)
            return i;
    return -1;
}

/* O(n) штука, которая просто перебором ищет свободный индекс. */
static int alloc_module_slot(void) {
    for (int i = 0; i < MAX_IMPORTS; i++)
        if (!g_modules[i].in_use) return i;
    return -1;
}

void load_imports(Program *prog)
{
    if (prog->import_count > MAX_IMPORTS)
        msrest_panic("Too many imports: %d (max %d)", prog->import_count, MAX_IMPORTS);

    /* Цикл инициализации на все импорты программы */
    for (int i = 0; i < prog->import_count; i++) {
        const char *mod_name = prog->imports[i].module;

        int m = find_module(mod_name); // индекс модуля.
        if (m == -1) {
            char path[256];
            snprintf(path, sizeof(path), "../modules/%s/%s" LIB_EXT, mod_name, mod_name);

            LibHandle dll = lib_open(path);
            if (!dll) {
                msrest_panic("Cannot load module '%s'", path); }


            /* Обязательная функция любого модуля! */
            uint32_t (*get_abi)(void) = (uint32_t (*)(void))lib_sym(dll, "msrest_abi_version");

            if (!get_abi) {
                lib_close(dll);
                msrest_panic("Module '%s' cannot provide it's version "
                            "(is it a Mashrest module?)", mod_name);
            }

            uint32_t module_abi = get_abi();
            if (module_abi != MSREST_API_ABI_VERSION) {
                lib_close(dll);
                msrest_panic("Module '%s' ABI mismatch: expected %u, got %u "
                            "(Update the module to the current ABI version)",
                            mod_name, (unsigned)MSREST_API_ABI_VERSION, (unsigned)module_abi);
            }

            m = alloc_module_slot();
            if (m == -1) { 
                lib_close(dll); 
                msrest_panic("Module table full"); }

            // личная копия через malloc (важно!!!).
            MsrestAPI *copy = malloc(sizeof(MsrestAPI));
            if (!copy) { 
                lib_close(dll); 
                msrest_panic("Out of memory while initializing libraries!"); 
            }
            memcpy(copy, &msrest_api, sizeof(MsrestAPI));

            g_modules[m].handle = dll;
            g_modules[m].api = copy;
            g_modules[m].in_use = 1;
            strncpy(g_modules[m].name, mod_name, sizeof(g_modules[m].name) - 1);
            g_modules[m].name[sizeof(g_modules[m].name) - 1] = '\0';
        }

        /* Если модуль уже открыт (провал из верхнего if по идее, или если он ложный). */

        NativeFunc fn = (NativeFunc)lib_sym(g_modules[m].handle, prog->imports[i].function);
        if (!fn) msrest_panic("Cannot find function '%s' in module '%s'", prog->imports[i].function, mod_name);

        native_table[i] = fn;
        g_import_module[i] = m;
    }
}

void free_imports(void)
{
    for (int i = 0; i < MAX_IMPORTS; i++) {
        if (g_modules[i].in_use) {
            lib_close(g_modules[i].handle);
            free(g_modules[i].api);           // освободить копию надо.
            g_modules[i].in_use = 0;
        }
    }
    memset(native_table, 0, sizeof(native_table));
    memset(g_import_module, 0, sizeof(g_import_module));
}