// main.c
#include "../msrestcore/runtime.h"
#include "../msrestcore/native.h"
#include "../msrestcore/execute.h"

#define DEBUG 0

/* main launch */
int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: msrestvm <file>\n");
        return 1;
    }

/* setting console format */
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    /* VM initialisation */
    vm = vm_create();  // global ptr
    if (!vm) {
        printf("ERROR: Failed to create VM!\n");
        return 1;
    }

    /* GC init */
    gc_init(&vm->gc);
    vm->objects = NULL;

    /* Program loader */
    Program *prog = load_program(argv[1]);
    if (!prog) {
        printf("ERROR: Failed to load program!\n");
        vm_destroy(vm);
        return 1;
    }

    /* import load */
    load_imports(prog);

    /* IP setting */
    vm->sp  = 0;
    vm->fp  = 0;
    vm->fsp = 0;
    vm->ip  = prog->code_start;

/* time-debug: only under DEBUG */
#ifdef DEBUG
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
#endif

    /* executing! */
    int r = msrest_execute(prog);

#ifdef DEBUG
    QueryPerformanceCounter(&end);

    /* statistics */
    printf("\n=== OPCODE STATS ===\n");
    for (int i = 0; i < 256; i++) {
        if (opcode_stats[i] != 0) {
            printf("%02X : %llu\n", i, (unsigned long long)opcode_stats[i]);
        }
    }

    double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;
    printf("%.9f sec\n", elapsed);
#endif

    /* clearing */
    free_program(prog);
    vm_destroy(vm);

    /* the most powerful thing in the universe */
    return 0;
}