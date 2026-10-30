/* << RUNTIME.H >> */

#ifndef RUNTIME_H
#define RUNTIME_H


/* < INCLUDE FILES > */

#include "../API/msrest_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include <sys/stat.h>
#include <stdarg.h>

#include "platform.h"
#include "getters_support.h"
#include "types.h"


/* < VM CONSTANTS > */

#define STACKCOUNT 262144
#define FRAMECOUNT 131072
#define VARCOUNT 2048

#define _PROFILE 0
#define BYTECODE_VALIDATION 1
#define GC_STEP_COUNT 1750
#define GC_INTERVAL_BYTES (GC_INTERVAL * 9)

#define MAX_IMPORTS 4096
#define V_MSREST_UNDEFINED -42


/* < ENUM DECLARATIONS > */
enum {
    TO_INT = 1,
    TO_FLOAT,
    TO_BOOL,
    TO_STR,
    STR_BYTELEN = 8
};

enum {
    SOURCE_POLICY_NONE   = 0,
    SOURCE_POLICY_FILE   = 1,
    SOURCE_POLICY_INLINE = 2,
};

enum {
    DICT_KEYS = 1,
    DICT_PAIRS,
    DICT_VALUES,
};

enum {
    CONST_STRING = 0,
    CONST_FLOAT,
    CONST_INT 
};

enum {
    GC_WHITE = 0,
    GC_GRAY  = 1,
    GC_BLACK = 2
};

typedef enum {
    GC_IDLE,
    GC_MARK,
    GC_SWEEP
} GCPhase;

typedef enum {
    GC_MODE_LAZY,
    GC_MODE_NORMAL,
    GC_MODE_AGGRESSIVE,
    GC_MODE_INSANE
} GCMode;


/* < FUNCTION FORWARD DECLARATIONS > */

typedef struct GCHeader GCHeader;
typedef struct Instruction Instruction;
typedef struct Value Value;
typedef struct Array Array;
typedef struct FloatArray FloatArray;
typedef struct BoolArray BoolArray;
typedef struct String String;
typedef struct Frame Frame;
typedef struct VM VM;
typedef struct Program Program;
typedef struct NativeObject NativeObject;

/* < GCState > */
typedef struct {

    GCPhase phase;
    GCHeader **gray;

    int gray_capacity;
    int gray_head;
    int gray_tail;

    GCHeader **sweep_ptr;
    int allocations;
} GCState;


/* < VM DEFINITION > */

struct VM {
    Value *variables;
    Value *stack;
    Frame *frames;
    
    size_t variables_capacity;
    size_t stack_capacity;
    size_t frames_capacity;
    
    int sp;
    int fp; 
    int fsp;
    int ip;
    
    GCHeader *objects;
    GCState gc;
};


/* < STRUCTURES DECLARATION > */


struct Instruction {
    uint8_t opcode;
    int arg1;
    int arg2;
};



typedef struct {
    char *module;
    char *function;
} NativeImport;


/* Одна запись таблицы диапазонов: ip-индексы -> координаты в исходнике.
   start_index/end_index - индексы инструкций в code-секции, не байты.
   end_index эксклюзивный. line/col/end_col - 16-битные */
typedef struct {
    uint32_t start_index;
    uint32_t end_index;
    uint16_t line;
    uint16_t col;
    uint16_t end_col;
    uint16_t _pad;
} RangeEntry;

typedef struct {
    uint8_t  policy;
    uint8_t  flags;
    char    *path;       /* путь к .msr-файлу, если policy == FILE */
    char    *text;       /* вшитый исходник, если policy == INLINE */
    uint32_t text_len;
} Source;

struct Frame {
    int return_ip;
    int old_fp;
    int old_sp;
};


struct Program {
    uint8_t *code;
    int code_size;
    int code_start;
    int code_end;
    Value *constant_pool;
    int constant_count;

    NativeImport *imports;

    int import_count;
    char base_path[1024];

    /* таблица диапазонов ip -> исходник. опциональная. */
    RangeEntry *ranges;
    uint32_t range_count;

    Source *source;
    uint16_t abi_version;
};

// Panic
#if defined(__GNUC__)
__attribute__((cold, noreturn, format(printf, 1, 2)))
#endif
void msrest_panic(const char *fmt, ...);


/* VM management */

VM* vm_create(void);
void vm_destroy(VM *vm);

/* < READ NUMBER > */

/* Reads a 16-bit number */
static inline uint16_t read_short(const uint8_t *p)
{
    uint16_t x;
    memcpy(&x, p, sizeof(x));
    return x;
}

/* Reads a 32-bit number */
static inline int32_t read_int(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}


static inline uint8_t soft_check_type(uint8_t required, uint8_t got)
{
    return required == got ? 1 : 0;
}

// TODO удалить
static inline void msrest_check_argc_impl(const char *func_name,
                                         uint16_t argc, uint16_t required)
{
    if (argc != required) {
        msrest_panic("%s expected %d arguments, got %d\n", 
                        func_name, required, argc);
    }
}
void msrest_msg(const char *fmt, ...);

/* < GLOBAL VARIABLES (extern) > */

extern VM *vm;
extern uint64_t opcode_stats[256];
extern uint64_t totalInstructions;
extern MsrestAPI msrest_api;
extern int gc_allocations;
extern Program *g_prog;


/* < FUNCTION PROTOTYPES > */

/* Default API function */
typedef Value (*NativeFunc)(MsrestAPI *api, Value *args, int argc);

/* Declaration of functions for working with the GC */
void gc_init(GCState *gc);
void gc_free(GCState *gc);

void gc_start(void);
void gc_ensure_gray_capacity(GCState *gc, int needed);
void *gc_alloc(size_t size, int type);

void mark_value(Value v);
void gc_mark_step(void);
void gc_sweep_step(int budget);
void gc_step(int budget);

void gc_collect(void);

/* Just small helper for GC_step (will remove in the future) */
static inline void gc_maybe_start(void)
{
    if (vm->gc.phase != GC_IDLE) {
        gc_step(GC_STEP_COUNT);
    }
}

extern int GC_THRESHOLD;
extern int GC_INTERVAL;
extern int GC_BUDGET;

static void gc_push_gray(GCHeader *obj) 
{
    if (!obj) return;
    if (obj->color != GC_WHITE) return;
    
    gc_ensure_gray_capacity(&vm->gc, vm->gc.gray_tail + 1);
    
    obj->color = GC_GRAY;
    vm->gc.gray[vm->gc.gray_tail++] = obj;
}


/* Program */

Program* load_program(const char *filename);
void free_program(Program *prog);
int load_constants(uint8_t *code, int pos, Value *constant_pool, int constant_count);
int Mvalidate_program_opcodes(Program *prog);
void Mvalidate_before_execution(Program *prog);

static inline Instruction fetch(uint8_t *code, int ip) 
{
    Instruction ins;
    ins.opcode = code[ip];
    ins.arg1 = read_int(code + ip + 1);
    ins.arg2 = read_int(code + ip + 5);
    return ins;
}
void vm_print_offsets(void);


/* Double <-> Value converters */
static inline double value_to_double(Value v) 
{
    if(v.type == MSREST_FLOAT)
        return v.floating;
    if(v.type == MSREST_INT)
        return (double)v.integer;
    msrest_panic("Type error: expected number VtD\n");
}

static inline Value float_to_value(double x) 
{
    Value v = {0};
    v.type = MSREST_FLOAT;
    v.floating = x;
    return v;
}


/* Integer <-> Value converters */
static inline intptr_t value_to_int(Value v) 
{
    if (v.type != MSREST_INT) msrest_panic("Type error: expected int VtI\n");
    return v.integer;
}

static inline Value int_to_value(intptr_t i) 
{
    Value v = {0};
    v.type = MSREST_INT;
    v.integer = i;
    return v;
}


/* Bool <-> Value converters */
static inline uint8_t value_to_bool(Value v) 
{
    switch(v.type) {
        case MSREST_BOOL:
            return v.logic != 0;
        case MSREST_INT:
            return v.integer != 0;
        case MSREST_FLOAT:
            return v.floating != 0.0;
        default:
            return 0;
    }
}

static inline Value bool_to_value(uint8_t b)
{
    Value v = {0};
    v.type = MSREST_BOOL;
    v.logic = b ? 1 : 0;
    return v;
}


/* String <-> Value converters */
Value string_to_value(const char *s);
char *value_to_string(Value v);


/* NULL -> Value */
static inline Value null_to_value(void) 
{
    Value v = {0};
    v.type = MSREST_NULL;
    return v;
}



static inline void lib_check_type(uint8_t required, uint8_t got, const char *lib_name, const char *funcname)
{
    if (required != got) {
        msrest_panic("[%s] Type error: %s() Required: %s, Got: %s\n", 
            lib_name, funcname, type_name(required), type_name(got));
    }
}

uint8_t value_equals(Value a, Value b);

_Noreturn void vm_shutdown(int code);

/* Array functions */
Value array_new(uint32_t capacity);

void array_push(Value v, Value value);
Value array_pop(Value v);

int array_index_check(Value v, uint32_t index);
void array_set(Value v, uint32_t index, Value value);

/* String functions */
Value string_new(const char *str);
Value string_new_len(const char *str, size_t len);
char *concat_strings(const char *a, const char *b);


/* Functions to work with native objects */
Value native_new(const char *type, void *payload, void (*destroy)(void *));
void *native_payload(Value value, const char *expected_type);

char* float_array_to_string(FloatArray* arr);
char* bool_array_to_string(BoolArray* arr);
char* array_to_string(Array* arr);

Value dict_new(void);
void dict_resize(Dict *d, uint32_t new_capacity);

void dict_set(Value dict_val, Value key_val, Value value);
Value dict_get(Value dict_val, Value key_val);
uint8_t dict_has(Value dict_val, Value key_val);
uint8_t dict_delete(Value dict_val, Value key_val);
uint32_t dict_size(Value dict_val);
char* dict_to_string(Dict *d);
Value dict_keys(Value dict_val);
Value dict_pairs(Value dict_val);
Value dict_values(Value dict_val);

/* < TYPEOF NAMES > */

static const char* typeof_names[] = {
    [MSREST_NULL]        = "null",
    [MSREST_INT]         = "integer",
    [MSREST_FLOAT]       = "float",
    [MSREST_BOOL]        = "bool",
    [MSREST_STRING]      = "string",
    [MSREST_ARRAY]       = "array",
    [MSREST_FLOAT_ARRAY] = "float_array",
    [MSREST_BOOL_ARRAY]  = "bool_array",
    [MSREST_DICT]        = "dict",
    [MSREST_NATIVE]      = "native",
    [MSREST_FUNCREF]     = "function",
};

static inline const char* typeof_name(uint8_t type) {
    if (type < sizeof(typeof_names)/sizeof(typeof_names[0]) && typeof_names[type])
        return typeof_names[type];
    return "unknown";
}

/* Сравнение строк (используем хеш для быстрого сравнения) */
static inline int string_equal(String *a, String *b) {
    if (a == b) return 1;
    if (a->hash != b->hash) return 0;
    if (a->length != b->length) return 0;
    return memcmp(a->data, b->data, a->length) == 0;
}


/* Stack */
static inline void push(VM* vm, Value value) 
{
     if (__builtin_expect(vm->sp >= STACKCOUNT, 0))
        msrest_panic("Stack overflow\n");
    vm->stack[vm->sp++] = value;
}

static inline Value pop(VM* vm) 
{
    if (__builtin_expect(vm->sp <= 0, 0)) {
        msrest_panic("Stack underflow\n");}
    return vm->stack[--vm->sp];
}

static inline Value raw_array_pop(Array *arr)
{
    return arr->values[--arr->size];
}

int vm_halt(VM *vm, uint8_t *code, uint32_t code_start, int exit_code, uint32_t instruction_count);

/* Memory functions safe wrappers*/
void* safe_malloc(size_t size);
void* safe_realloc(void *ptr, size_t new_size);
void* safe_calloc(size_t count, size_t size);
char* safe_strdup(const char *s);

static inline int64_t norm_index(Value cont, int64_t raw)
{
    uint32_t size;

    switch (cont.type) {
        case MSREST_ARRAY:       size = cont.array->size;       break;
        case MSREST_BOOL_ARRAY:  size = cont.bool_array->size;  break;
        case MSREST_FLOAT_ARRAY: size = cont.float_array->size; break;
        default: return V_MSREST_UNDEFINED;
    }

    int64_t idx = raw;
    if (idx < 0) idx += (int64_t)size;
    if (idx < 0 || (uint64_t)idx >= size) return V_MSREST_UNDEFINED;
    return idx;
}

#endif // RUNTIME_H
