/* << RUNTIME.C >> */

// я писал на двух языках, потому что хотел публиковать на GitHub.
// но публикация так и не дождалась.
// а на русском комментарии для себя были в основном.
// прошу прощения.

/* < INCLUDE FILES > */
#include "runtime.h"


/* Global ptr */

VM *vm = NULL;
/* Указатель на текущую программу нужен для вывода координат в msrest_panic. */
Program *g_prog = NULL;

uint64_t opcodeTime[256] = {0};
uint64_t totalInstructions = 0;
uint64_t opcode_stats[256] = {0};

int GC_THRESHOLD = 1024;
int GC_INTERVAL = 2000;
int GC_BUDGET = 2000;
uint64_t gc_starts = 0;
uint64_t gc_steps = 0;
int gc_allocations = 0;


/* API declaration */
MsrestAPI msrest_api = {
    .string_to_value = string_to_value,

    .int_to_value = int_to_value,
    .float_to_value = float_to_value,
    .bool_to_value = bool_to_value,
    .null_to_value = null_to_value,
    .value_to_string = value_to_string,

    .value_to_int = value_to_int,
    .value_to_double = value_to_double,
    .value_to_bool = value_to_bool,

    .value_equals = value_equals,

    .value_type = value_type,
    .string_data = string_data,
    .msrest_panic = msrest_panic,
    .msrest_msg = msrest_msg,

    .array_size = array_size,
    .array_get  = array_get,
    .array_set  = array_set,
    .array_push = array_push,
    .array_pop  = array_pop,
    .array_new = array_new,

    .string_chars = string_chars,
    .string_bytes = string_bytes,
    .string_empty = string_empty,
    .array_index_check = array_index_check,
    .check_argc = msrest_check_argc_impl,

    .string_new = string_new,
    .string_new_len = string_new_len,
    .native_new = native_new,
    .native_payload = native_payload,
    .soft_check_type = soft_check_type,

    .dict_new = dict_new,
    .dict_set = dict_set,
    .dict_get = dict_get,
    .dict_has = dict_has,
    .dict_delete = dict_delete,
    .dict_size = dict_size,
    .dict_keys = dict_keys,
    .lib_check_type = lib_check_type,
    .type_name = type_name,

};


/*Native payload getter, unwraps MSREST_NATIVE to payload data*/
void *native_payload(Value value, const char *expected_type)
{
    if(value.type != MSREST_NATIVE) msrest_panic("Type error: Expected native object");

    if(strcmp(value.native->type, expected_type) != 0)
        msrest_panic("Type error: Expected native '%s', got '%s'",
            expected_type, value.native->type);

    return value.native->payload;
}


/*Creates a new native object wrapper*/
/*Requires payload, detstroyer function and type specifier*/
/*The last is needed to avoid type collisions, so the library tags will not overlap*/
Value native_new(const char *type, void *payload, void (*destroy)(void *))
{
    NativeObject *obj = gc_alloc(sizeof(NativeObject), MSREST_NATIVE);

    obj->type = type;
    obj->payload = payload;
    obj->destroy = destroy;

    Value v;
    v.type = MSREST_NATIVE;
    v.native = obj;

    return v;
}




/*Cleanup*/
void vm_destroy(VM *vm) {
    if (!vm) return;
    free(vm->variables);
    free(vm->stack);
    free(vm->frames);
    free(vm);
}

VM* vm_create(void) {
    VM *vm = calloc(1, sizeof(VM));
    if (!vm) return NULL;
    
    /*Stack, variables and frames allocation*/
    vm->variables = calloc(VARCOUNT, sizeof(Value));
    vm->stack = calloc(STACKCOUNT, sizeof(Value));
    vm->frames = calloc(FRAMECOUNT, sizeof(Frame));

    /*Failure*/
    if (!vm->variables || !vm->stack || !vm->frames) {
        vm_destroy(vm);
        return NULL;}
    
    vm->variables_capacity = VARCOUNT;
    vm->stack_capacity = STACKCOUNT;
    vm->frames_capacity = FRAMECOUNT;
    
    // Индексы
    vm->sp = 0;
    vm->fp = 0;
    vm->fsp = 0;
    vm->ip = 0;
    
    return vm;
}

/* Ищет диапазон, покрывающий ip (абсолютное байтовое смещение в code).
   Возвращает NULL, если ничего не нашли или таблицы нет.
   ip конвертируется в индекс инструкции: (ip - code_start) / 9. */
static const RangeEntry* find_range(uint32_t ip)
{
    if (!g_prog || !g_prog->ranges || g_prog->range_count == 0)
        return NULL;
    if (ip < (uint32_t)g_prog->code_start)
        return NULL;

    uint32_t idx = (ip - (uint32_t)g_prog->code_start) / 9;

    int lo = 0, hi = (int)g_prog->range_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const RangeEntry *e = &g_prog->ranges[mid];
        if (idx < e->start_index)      hi = mid - 1;
        else if (idx >= e->end_index)  lo = mid + 1;
        else return e;
    }
    return NULL;
}



static const char* source_text(Source *src)
{
    if (!src) return NULL;
    if (src->text) return src->text;

    if (src->policy == SOURCE_POLICY_FILE && src->path) {
        FILE *f = fopen(src->path, "rb");
        if (!f) return NULL;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        rewind(f);
        if (n > 0) {
            src->text = safe_malloc(n + 1);
            fread(src->text, 1, n, f);
            src->text[n] = '\0';
            src->text_len = (uint32_t)n;
        }
        fclose(f);
        return src->text;
    }
    return NULL;
}


static void print_source_line(Source *src, uint32_t line,
                              uint16_t col, uint16_t end_col)
{
    if (!src || !src->text) return;

    const char *s = src->text;
    uint32_t cur = 0;
    const char *line_start = s;

    while (cur < line && *line_start) {
        if (*line_start == '\n') cur++;
        line_start++;
    }

    const char *line_end = line_start;
    while (*line_end && *line_end != '\n') line_end++;

    fprintf(stderr, "%u | %.*s\n", (unsigned)(line + 1),
            (int)(line_end - line_start), line_start);

    int prefix_len = snprintf(NULL, 0, "%u | ", (unsigned)(line + 1));
    for (int i = 0; i < prefix_len + col; i++) fputc(' ', stderr);
    int width = (end_col > col) ? (end_col - col) : 1;
    for (int i = 0; i < width; i++) fputc('^', stderr);
    fputc('\n', stderr);
}


void msrest_panic(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[VM-ERR] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);

    /* если знаем ip - показываем координаты в исходнике. */
    if (vm && g_prog) {
        const RangeEntry *e = find_range((uint32_t)vm->ip);
        if (e) {
            fprintf(stderr, "  at line %u, col %u-%u\n",
                    (unsigned)e->line + 1,
                    (unsigned)e->col,
                    (unsigned)e->end_col);

            Source *src = g_prog->source;
            if (src) {
                /* source_text сделает ленивую загрузку, если policy == FILE. */
                const char *text = source_text(src);
                if (text) {
                    print_source_line(src, e->line, e->col, e->end_col);
                }
            }
        } else {
            fprintf(stderr, "  at ip = %d (no debug info)\n", vm->ip);
        }
    }

    vm_shutdown(EXIT_FAILURE);
}

_Noreturn void vm_shutdown(int code)
{
    if (vm) {
        gc_collect();       
        gc_free(&vm->gc);
        vm_destroy(vm);
    }

    if (g_prog) {
        free_program(g_prog);
    }

    exit(code);
}


void msrest_msg(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[VM-MSG] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}


int vm_halt(VM *vm, uint8_t *code, uint32_t code_start, int exit_code, uint32_t instruction_count)
{
    /*
    printf("FINAL SP=%d\n", vm->sp);
    printf("EXIT CODE: %d\n", exit_code);
    */
    
    #if _PROFILE
        printf("\n=== PROFILE ===\n");
        for (int i = 0; i < instruction_count; i++) {
            if (instruction_hits[i] == 0)
                continue;
            Instruction ins = fetch(code, code_start + i * 9);
            printf("%04d  OP=%02X  hits=%llu\n",
                   i, ins.opcode,
                   (unsigned long long)instruction_hits[i]);
        }
        free(instruction_hits);
    #endif
    
    gc_collect();
    return exit_code;
}

/* < SUPPORT FUNCTIONS > */
/*There is some wrappers, converters and constructors*/


/*A wrapper that allows for the automatic handling of memory allocation errors*/
void* safe_malloc(size_t size)
{
    void *p = malloc(size);

    if (__builtin_expect(!p, 0)) msrest_panic("Out of memory (malloc)");
    return p;
}

/*Similarly safe_malloc, it handles memory allocation with zero-initialization*/
void* safe_calloc(size_t count, size_t size)
{
    void *p = calloc(count, size);
    if (__builtin_expect(!p && count > 0 && size > 0, 0)) 
        msrest_panic("Out of memory (calloc)");
    return p;
}

/*Similarly safe_malloc, it handles memory reallocation errors.*/
void* safe_realloc(void *ptr, size_t new_size)
{
    void *p = realloc(ptr, new_size);

    if (__builtin_expect(!p && new_size > 0, 0)) msrest_panic("Out of memory (realloc)");
    return p;
}

/*Strdup wrapper*/
char* safe_strdup(const char *s)
{
    char *p = strdup(s);

    if (__builtin_expect(!p, 0)) msrest_panic("Out of memory (strdup)");
    return p;
}


/* < GC >*/

/* GC initialization function*/
void gc_init(GCState *gc) {
    gc->phase = GC_IDLE;
    gc->gray = NULL;
    gc->gray_capacity = 0;
    gc->gray_head = 0;
    gc->gray_tail = 0;
    gc->sweep_ptr = NULL;
    gc->allocations = 0;
    
    /*Start buffer allocation*/
    gc->gray_capacity = 4096;
    gc->gray = safe_malloc(sizeof(GCHeader*) * gc->gray_capacity);
}


/*This function counts important allocations in VM, like strings and arrays*/
void *gc_alloc(size_t size, int type)
{
    GCHeader *obj = safe_calloc(1, size);

    if (vm->gc.phase == GC_SWEEP) 
        obj->color = GC_BLACK;
    else 
        obj->color = (vm->gc.phase == GC_IDLE) ? GC_WHITE : GC_GRAY;


    obj->type = type;
    obj->next = vm->objects;
    vm->objects = obj;
    gc_allocations++;

    return obj;
}

/*Free all GC resources*/
void gc_free(GCState *gc) {
    if (gc->gray) {
        free(gc->gray);
        gc->gray = NULL;
    }
    gc->gray_capacity = 0;
}


/*Checks capacity of gray array and expands if required*/
void gc_ensure_gray_capacity(GCState *gc, int needed) {
    if (needed <= gc->gray_capacity) return;
    
    int new_capacity = gc->gray_capacity * 2;
    if (new_capacity < needed) new_capacity = needed + 1024;
    
    gc->gray = safe_realloc(gc->gray, sizeof(GCHeader*) * new_capacity);
    gc->gray_capacity = new_capacity;
}


/*Adds object to gray queue*/
void mark_value(Value v)
{
    switch(v.type)
    {
        case MSREST_STRING:
        {
            GCHeader *obj = (GCHeader*)v.string;
            if (obj->color == GC_WHITE)
            {
                obj->color = GC_GRAY;
                gc_push_gray(obj);
            }
            break;
        }
        case MSREST_ARRAY:
        {
            GCHeader *obj = (GCHeader*)v.array;
            if (obj->color == GC_WHITE)
            {
                obj->color = GC_GRAY;
                gc_push_gray(obj);
            }
            break;
        }
        case MSREST_INT_ARRAY:
        {
            GCHeader *obj = (GCHeader*)v.int_array;
            if (obj->color == GC_WHITE)
            {
                obj->color = GC_GRAY;
                gc_push_gray(obj);
            }
            break;
        }
        case MSREST_FLOAT_ARRAY:
        {
            GCHeader *obj = (GCHeader*)v.float_array;
            if (obj->color == GC_WHITE)
            {
                obj->color = GC_GRAY;
                gc_push_gray(obj);
            }
            break;
        }
        case MSREST_BOOL_ARRAY:
        {
            GCHeader *obj = (GCHeader*)v.bool_array;
            if (obj->color == GC_WHITE)
            {
                obj->color = GC_GRAY;
                gc_push_gray(obj);
            }
            break;
        }
        case MSREST_NATIVE:
        {
            GCHeader *obj = (GCHeader*)v.native;
            if (obj->color == GC_WHITE)
            {
                obj->color = GC_GRAY;
                gc_push_gray(obj);
            }
            break;
        }
        case MSREST_DICT: {
            GCHeader *obj = (GCHeader*)v.dictionary;
            if (obj->color == GC_WHITE) {
                obj->color = GC_GRAY;
                gc_push_gray(obj);
            }
            break;
        }
    }
}


/*Fully traverses the object, marking it black and its child objects gray*/
/*Takes an object from the gray queue*/
void gc_mark_step(void) {
    if (vm->gc.gray_head >= vm->gc.gray_tail) return;
    
    GCHeader *obj = vm->gc.gray[vm->gc.gray_head++];
    
    switch (obj->type) {
        case MSREST_ARRAY: {
            Array *arr = (Array*)obj;
            for (int i = 0; i < arr->size; i++)
                mark_value(arr->values[i]);
            break;
        }
        case MSREST_DICT: {
            Dict *d = (Dict*)obj;
            // Маркируем все ключи и значения
            for (uint32_t i = 0; i < d->capacity; i++) {
                DictEntry *entry = d->buckets[i];
                while (entry) {
                    // Ключ - строка
                    GCHeader *key_hdr = (GCHeader*)entry->key;
                    if (key_hdr->color == GC_WHITE) {
                        key_hdr->color = GC_GRAY;
                        gc_push_gray(key_hdr);
                    }
                    
                    // значение (может быть любого типа)
                    mark_value(entry->value);
                    
                    entry = entry->next;
                }
            }
            break;
        }
    }
    obj->color = GC_BLACK;
}


/*Performs a GC work step using the budget*/
void gc_step(int budget)
{
    switch(vm->gc.phase)
    {
        /*Active mark phase, takes objects from the queue and marks them*/
        case GC_MARK:

            if(vm->gc.gray_head != vm->gc.gray_tail) {
                for(int i = 0; i < budget; i++)
                {   
                /*Else if the gray array is empty, switch phase to sweep*/
                    if(vm->gc.gray_head == vm->gc.gray_tail) {
                        vm->gc.phase = GC_SWEEP;
                        vm->gc.sweep_ptr = &vm->objects;
                        break;
                    }
                    gc_mark_step();
                }
            }
            else {
                vm->gc.phase = GC_SWEEP;
                vm->gc.sweep_ptr = &vm->objects;}

            break;
        /*If sweep phase, then just checks some objects*/
        case GC_SWEEP:
            gc_sweep_step(GC_BUDGET);
            break;
        /*Else, just skip*/
        case GC_IDLE:

            break;
    }
}


/*Sweeps dead objects, converting black objects to white objects*/
void gc_sweep_step(int budget) 
{
    GCHeader **obj = vm->gc.sweep_ptr;

        int freed = 0;

    while(budget-- && *obj) {
        /* Remarking all the black objects to white for next cycle of GC-collect */
        if((*obj)->color == GC_BLACK) {
            (*obj)->color = GC_WHITE;
            obj = &(*obj)->next;} 
        /* This block frees white objects */
        else {
            GCHeader *dead = *obj;
            *obj = dead->next;
            switch(dead->type) {
                case MSREST_STRING: {
                    String *s = (String*)dead;
                    
                    free(s->data);
                    free(dead);   
                    break;
                }
                case MSREST_ARRAY: {
                    Array *a = (Array*)dead;

                    free(a->values);
                    free(dead);
                    break;
                }
                case MSREST_DICT: {
                    Dict *d = (Dict*)dead;
                    
                    // освобождаем все записи (они через malloc).
                    for (uint32_t i = 0; i < d->capacity; i++) {
                        DictEntry *entry = d->buckets[i];
                        while (entry) {
                            DictEntry *next = entry->next;
                            free(entry);  // через free, не через сборщик. аллоцировали же не через сборщик
                            // здесь стоит весьма важное решение архитектурное
                            entry = next;
                        }
                    }
                    
                    // освобождаем массив корзин.
                    free(d->buckets);
                    
                    // освобождаем сам объект Dict.
                    free(dead);
                    break;
                }
                case MSREST_INT_ARRAY: {
                    IntArray *a = (IntArray*)dead;

                    free(a->values);
                    free(dead);
                    break;
                }
                case MSREST_FLOAT_ARRAY: {
                    FloatArray *a = (FloatArray*)dead;

                    free(a->values);
                    free(dead);
                    break;
                }
                case MSREST_BOOL_ARRAY: {
                    BoolArray *a = (BoolArray*)dead;

                    free(a->values);
                    free(dead);
                    break;
                }
                case MSREST_NATIVE: {
                    NativeObject *n = (NativeObject*)dead;

                    if(n->destroy) n->destroy(n->payload);
                    free(dead);
                    break;
                }
            }
             freed++;
        }
    }
        vm->gc.sweep_ptr = obj;
    
    if(*obj == NULL) vm->gc.phase = GC_IDLE;
}


/*Mark all objects unused, and do some steps if phase != IDLE*/
void gc_collect(void)
{
    gc_start();

    while(vm->gc.phase != GC_IDLE)
        gc_step(GC_STEP_COUNT);
}





/*Marks all objects white, cleans gray queue, and checks roots*/
void gc_start(void) 
{
    GCHeader *obj = vm->objects;
    int count = 0;

    while (obj) {
        obj->color = GC_WHITE;
        obj = obj->next;
        count++;}

    // адаптивный расчёт параметров.
    GC_BUDGET = (count >> 3) + 100;          // минимум 100.
    GC_THRESHOLD = 4096 / (1 + (count >> 12)); // Чем больше объектов, тем меньше порог.
    GC_INTERVAL = 4000 / (1 + (count >> 13));  // Аналогично.

    // сатурация (чтобы не улететь в 0).
    if (GC_BUDGET > 10000) GC_BUDGET = 10000;
    if (GC_THRESHOLD < 64) GC_THRESHOLD = 64;
    if (GC_INTERVAL < 200) GC_INTERVAL = 200;
 
    vm->gc.gray_head = 0;
    vm->gc.gray_tail = 0;
    vm->gc.sweep_ptr = &vm->objects;

    /*Here chekcing the roots*/
    // самый быстрый вариант через строгое, чтобы gcc понял, я ему обещаю.
    Value *restrict vars = vm->variables;
    Value *restrict end = vars + VARCOUNT;
    while (vars < end) {
        mark_value(*vars++);
    }

    Value *restrict stack = vm->stack;
    Value *restrict stack_end = stack + vm->sp;
    while (stack < stack_end) {
        mark_value(*stack++);
    }

    vm->gc.phase = GC_MARK;
    gc_allocations = 0;
}


/* < CONVERTERS > */

Value string_to_value(const char *s)
{
    String *str = gc_alloc(sizeof(String), MSREST_STRING);
    
    str->length = (uint32_t)strlen(s);
    str->chars = count_utf8_chars((uint8_t*)s, str->length);
    str->data = safe_strdup(s);
    
    Value v = {0};
    v.type = MSREST_STRING;
    v.string = str;
    hash_value(v);

    return v;
}

IntArray *intarray_new(uint32_t size) {
    IntArray *arr = gc_alloc(sizeof(IntArray), MSREST_INT_ARRAY);
    arr->size = size;
    arr->capacity = (size > 8) ? size : 8;
    arr->values = safe_malloc(sizeof(int64_t) * arr->capacity);
    return arr;
}

Value array_new(uint32_t size)
{
    Array *arr = gc_alloc(sizeof(Array), MSREST_ARRAY);

    arr->size = size;
    arr->capacity = size > 8 ? size : 8;
    arr->values = safe_malloc(sizeof(Value) * arr->capacity);

    for (int i = 0; i < arr->size; i++)
        arr->values[i] = null_to_value(); // или Value с MSREST_NULL

    Value v;
    v.type = MSREST_ARRAY;
    v.array = arr;

    return v;
}






int array_index_check(Value v, uint32_t index)
{
    if (v.type != MSREST_ARRAY)
        msrest_panic("[lib] Type error: Expected array");

    return index >= 0 && index < v.array->size;
}


Value string_new(const char *str)
{
    if (!str)
        msrest_panic("[lib] Cannot make string");
    return string_to_value(str);
}

Value string_new_len(const char *str, size_t len)
{
    if (!str) msrest_panic("[lib] Cannot make string");
    String *s = gc_alloc(sizeof(String), MSREST_STRING);

    s->length = len;
    s->chars = count_utf8_chars((uint8_t*)str, len);
    
    s->data = safe_malloc(len + 1);
    if (__builtin_expect(!s->data, 0)) msrest_panic("Out of memory");
    memcpy(s->data, str, len);
    s->data[len] = '\0';

    Value v;
    v.type = MSREST_STRING;
    v.string = s;
    hash_value(v);
    return v;
}



Value dict_new(void) {
    Dict *d = gc_alloc(sizeof(Dict), MSREST_DICT);
    
    d->capacity = 8;  // начальный размер.
    d->size = 0;
    d->buckets = safe_calloc(d->capacity, sizeof(DictEntry*));
    
    Value v;
    v.type = MSREST_DICT;
    v.dictionary = d;
    return v;
}


/* получение всех ключей словаря в виде массива */
// для API особенно.

Value dict_keys(Value dict_val) 
{
    if (dict_val.type != MSREST_DICT)
        msrest_panic("[VM-ERR] dict.keys(): expected dictionary");
    
    Dict *d = dict_val.dictionary;
    
    /* создаём массив нужного размера */
    Value array_val = array_new(d->size);
    uint32_t idx = 0;
    
    /* проходим по всем корзинам */
    for (uint32_t i = 0; i < d->capacity; i++) {
        DictEntry *entry = d->buckets[i];
        while (entry) {
            /* кладём ключ в массив */
            Value key_val;
            key_val.type = MSREST_STRING;
            key_val.string = entry->key;
            
            /* array_set принимает Value массив и Value ключ */
            array_set(array_val, idx, key_val);
            idx++;
            entry = entry->next;
        }
    }
    
    return array_val;
}


Value dict_values(Value dict_val) 
{
    if (dict_val.type != MSREST_DICT)
        msrest_panic("[VM-ERR] dict.values(): expected dictionary");
    
    Dict *d = dict_val.dictionary;
    
    /* создаём массив нужного размера */
    Value array_val = array_new(d->size);
    uint32_t idx = 0;
    
    /* проходим по всем корзинам */
    for (uint32_t i = 0; i < d->capacity; i++) {
        DictEntry *entry = d->buckets[i];
        while (entry) {
            /* кладём ключ в массив */
            Value key_val;
            key_val.type = MSREST_STRING;
            key_val.string = entry->key;
            Value data = dict_get(dict_val, key_val);
            
            /* array_set принимает Value массив и Value ключ */
            array_set(array_val, idx, data);
            idx++;
            entry = entry->next;
        }
    }
    
    return array_val;
}



Value dict_pairs(Value dict_val) 
{
    if (dict_val.type != MSREST_DICT)
        msrest_panic("[VM-ERR] dict.pairs(): expected dictionary");

    Value worker = dict_keys(dict_val);
    Array *worker_arr = worker.array;
    Value container = array_new(0);
    
    int i = 0;
    while (i < worker_arr->size) {
        Value v = dict_get(dict_val, worker_arr->values[i]);
        Value imm = array_new(0);
        array_push(imm, worker_arr->values[i]);
        array_push(imm, v);
        array_push(container, imm);
        i++;
    }
    /* проходим по всем корзинам */

    
    return container;
}

/* ресайз словаря */
void dict_resize(Dict *d, uint32_t new_capacity) {
    // новые корзины.
    DictEntry **new_buckets = safe_calloc(new_capacity, sizeof(DictEntry*));
    uint32_t moved = 0;
    // перехешируем все существующие записи.
    for (uint32_t i = 0; i < d->capacity; i++) {
        DictEntry *entry = d->buckets[i];
        while (entry) {
            DictEntry *next = entry->next;
            
            // используем хэш.
            uint32_t new_index = (uint32_t)(entry->key->hash & (new_capacity - 1));
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            moved++;
            entry = next;
        }
    }
    // освобождаем старые корзины
    free(d->buckets);
    d->buckets = new_buckets;
    d->capacity = new_capacity;
}

/* Установка значения в словарь */
void dict_set(Value dict_val, Value key_val, Value value) {
    if (dict_val.type != MSREST_DICT)
        msrest_panic("dict_set: expected dictionary");
    if (key_val.type != MSREST_STRING)
        msrest_panic("dict_set: keys must be strings");
    
    Dict *d = dict_val.dictionary;
    String *key = key_val.string;
    uint32_t index = (uint32_t)(key->hash & (d->capacity - 1));

    
    // поиск существующего ключа.
    DictEntry *entry = d->buckets[index];
    while (entry) {
        if (string_equal(entry->key, key)) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    // создаём новую запись (через malloc, НЕ через сборщик!)
    DictEntry *new_entry = safe_malloc(sizeof(DictEntry));
    new_entry->key = key;
    new_entry->value = value;
    new_entry->next = d->buckets[index];
    d->buckets[index] = new_entry;
    d->size++;
    
    // ресайз при заполнении > 75%.
    if (d->size > d->capacity * 3 / 4) {
        dict_resize(d, d->capacity * 2);
    }
}

/* Получение значения из словаря */
Value dict_get(Value dict_val, Value key_val) {
    if (dict_val.type != MSREST_DICT)
        msrest_panic("dict_get: expected dictionary");
    if (key_val.type != MSREST_STRING)
        msrest_panic("dict_get: keys must be strings");
    
    Dict *d = dict_val.dictionary;
    String *key = key_val.string;
    uint32_t index = (uint32_t)(key->hash & (d->capacity - 1));
    
    
    DictEntry *entry = d->buckets[index];
    int pos = 0;
    while (entry) {
        if (string_equal(entry->key, key)) {
            return entry->value;
        }
        entry = entry->next;
        pos++;
    }
    
    msrest_panic("Key Error: key '%s' not found in dictionary", key->data);
    return null_to_value();
}

/* Проверка наличия ключа */
uint8_t dict_has(Value dict_val, Value key_val) {
    if (dict_val.type != MSREST_DICT)
        msrest_panic("dict_has: expected dictionary");
    if (key_val.type != MSREST_STRING)
        msrest_panic("dict_has: keys must be strings");
    
    Dict *d = dict_val.dictionary;
    String *key = key_val.string;
    uint32_t index = (uint32_t)(key->hash & (d->capacity - 1));
    
    DictEntry *entry = d->buckets[index];
    while (entry) {
        if (string_equal(entry->key, key)) {
            return 1;
        }
        entry = entry->next;
    }
    
    return 0;
}

/* Удаление ключа */
uint8_t dict_delete(Value dict_val, Value key_val) {
    if (dict_val.type != MSREST_DICT)
        msrest_panic("dict_delete: expected dictionary");
    if (key_val.type != MSREST_STRING)
        msrest_panic("dict_delete: keys must be strings");
    
    Dict *d = dict_val.dictionary;
    String *key = key_val.string;
    uint32_t index = (uint32_t)(key->hash & (d->capacity - 1));
    
    DictEntry *entry = d->buckets[index];
    DictEntry *prev = NULL;
    
    while (entry) {
        if (string_equal(entry->key, key)) {
            // удаляем запись.
            if (prev) {
                prev->next = entry->next;
            } else {
                d->buckets[index] = entry->next;
            }
            
            // освобождаем память записи (не ключ, он в GC).
            free(entry);
            d->size--;
            return 1;
        }
        prev = entry;
        entry = entry->next;
    }
    
    return 0;  // ключ не найден.
}

/* Размер словаря */
uint32_t dict_size(Value dict_val) {
    if (dict_val.type != MSREST_DICT)
        msrest_panic("dict_size: expected dictionary");
    return dict_val.dictionary->size;
}




void array_set(Value v, uint32_t index, Value value)
{
    if (v.type != MSREST_ARRAY)
        msrest_panic("Type error: Expected array");
    if (index < 0 || index >= v.array->size)
        msrest_panic("Access violation error: Array index out of range");
    v.array->values[index] = value;
}

void array_push(Value array, Value value)
{
    if (array.type != MSREST_ARRAY)
        msrest_panic("Type error: Expected array");

    Array *arr = array.array;
    if (arr->size >= arr->capacity) {
        arr->capacity *= 2;
        arr->values = safe_realloc(arr->values,
                                   sizeof(Value) * arr->capacity);}
    arr->values[arr->size++] = value;
}

Value array_pop(Value v)
{
    if (v.type != MSREST_ARRAY)
        msrest_panic("Type error: Expected array");
    if (__builtin_expect(v.array->size == 0, 0))
        msrest_panic("Access violation error: Array is empty");
    return v.array->values[--v.array->size];
}



uint8_t value_equals(Value a, Value b)
{
    /* 1 - a и b равны. 0 - не равны.*/
    /* разные типы: единственное исключение - int/float как числа. */
    if (a.type != b.type) {
        if ((a.type == MSREST_INT && b.type == MSREST_FLOAT) ||
            (a.type == MSREST_FLOAT && b.type == MSREST_INT)) {
            double da = (a.type == MSREST_INT) ? (double)a.integer : a.floating;
            double db = (b.type == MSREST_INT) ? (double)b.integer : b.floating;
            return da == db;
        }
        return 0;
    }

    switch (a.type) {
        case MSREST_NULL: {
            return 1;
        }
        case MSREST_INT: {
            return a.integer == b.integer;
        }
        case MSREST_FLOAT: {
            return a.floating == b.floating;
        }
        case MSREST_BOOL: {
            return a.logic == b.logic;
        }
        case MSREST_STRING: {
            /* сравниваем по хешу, потом по длине, потом по байтам. */
            if (a.string->hash != b.string->hash) return 0;
            if (a.string->length != b.string->length) return 0;
            return memcmp(a.string->data, b.string->data, a.string->length) == 0;
        }
        case MSREST_ARRAY: {
            return a.array == b.array;
        }
        case MSREST_INT_ARRAY: {
            return a.int_array == b.int_array;
        }
        case MSREST_FLOAT_ARRAY: {
            return a.float_array == b.float_array;
        }
        case MSREST_BOOL_ARRAY: {
            return a.bool_array == b.bool_array;
        }
        case MSREST_DICT: {
            return a.dictionary == b.dictionary;
        }
        case MSREST_NATIVE: {
            return a.native == b.native;
        }
        case MSREST_FUNCREF: {
            return a.fnref.addr == b.fnref.addr &&
                   a.fnref.arity == b.fnref.arity;
        }
        default: {
            return 0;
        }
    }
}


static char* value_to_qstring(Value v)
{
    if (v.type == MSREST_STRING) {
        const char *raw = v.string->data;
        size_t raw_len = strlen(raw);
        char *quoted = safe_malloc(raw_len + 3);
        quoted[0] = '\'';
        memcpy(quoted + 1, raw, raw_len);
        quoted[raw_len + 1] = '\'';
        quoted[raw_len + 2] = '\0';
        return quoted;
    }
    return value_to_string(v);
}


char* value_to_string(Value v) {
    
    static void* visited[256];
    static int depth = 0;
    
    // проверка рекурсии.
    if (v.type == MSREST_ARRAY || v.type == MSREST_DICT) {
        void* ptr = (void*)(v.type == MSREST_ARRAY ? (void*)v.array : (void*)v.dictionary);
        for (int i = 0; i < depth; i++) {
            if (visited[i] == ptr) {
                return safe_strdup("<recursion>");
            }
        }
        visited[depth++] = ptr;
    }
    
    char* result = NULL;
    char buffer[128];
    
    
    switch(v.type) {
        case MSREST_STRING:
            result = safe_strdup(v.string->data);
            break;
        case MSREST_BOOL:
            result = safe_strdup(v.logic ? "<1>" : "<0>");
            break;
        case MSREST_INT:
            snprintf(buffer, sizeof(buffer), "%lld", (long long)v.integer);
            result = safe_strdup(buffer);
            break;
        case MSREST_FLOAT:
            snprintf(buffer, sizeof(buffer), "%.12g", v.floating);
            result = safe_strdup(buffer);
            break;
        case MSREST_NULL:
            result = safe_strdup("null");
            break;
        case MSREST_DICT:
            result = dict_to_string(v.dictionary);
            break;
        case MSREST_FUNCREF:
            snprintf(buffer, sizeof(buffer), "<fnref (argc: %d)>", v.fnref.arity);
            result = safe_strdup(buffer);
            break;
        case MSREST_INT_ARRAY:
            result = int_array_to_string(v.int_array);
            break;
        case MSREST_FLOAT_ARRAY:
            result = float_array_to_string(v.float_array);
            break;
        case MSREST_BOOL_ARRAY:
            result = bool_array_to_string(v.bool_array);
            break;
        case MSREST_ARRAY:
            result = array_to_string(v.array);
            break;
        case MSREST_NATIVE:
            result = safe_strdup("<native object>");
            break;
        default:
            result = safe_strdup("<unknown>");
    }

    if (v.type == MSREST_ARRAY || v.type == MSREST_DICT) {
        depth--;
    }
    
    return result;
}

char *concat_strings(const char *a, const char *b) {
    size_t la = strlen(a);
    size_t lb = strlen(b);

    char *result = safe_malloc(la + lb + 1);

    memcpy(result, a, la);
    memcpy(result + la, b, lb + 1); // вместе с '\0'.
    return result;
}



/* Конвертирует IntArray в строку */
char* int_array_to_string(IntArray* arr) 
{
    if (!arr) return safe_strdup("null");
    if (arr->size == 0) return safe_strdup("[]");
    
    // оцениваем размер: [ + элементы + ", " + ].
    size_t estimated = 2; // для '[' и ']'
    for (int i = 0; i < arr->size; i++) {
        estimated += 22; // максимум для 64-битного числа.
        if (i < arr->size - 1) estimated += 2; // для ", ".
    }
    
    char* result = safe_malloc(estimated);
    char* ptr = result;
    ptr += sprintf(ptr, "[");
    
    for (int i = 0; i < arr->size; i++) {
        ptr += sprintf(ptr, "%lld", (long long)arr->values[i]);
        if (i < arr->size - 1) {
            ptr += sprintf(ptr, ", ");
        }
    }
    
    ptr += sprintf(ptr, "]");
    return result;
}

/* Конвертирует FloatArray в строку */
char* float_array_to_string(FloatArray* arr) 
{
    if (!arr) return safe_strdup("null");
    if (arr->size == 0) return safe_strdup("[]");
    
    size_t estimated = 2;
    for (int i = 0; i < arr->size; i++) {
        estimated += 64; // достаточно для float
        if (i < arr->size - 1) estimated += 2;
    }
    
    char* result = safe_malloc(estimated);
    char* ptr = result;
    ptr += sprintf(ptr, "[");
    
    for (int i = 0; i < arr->size; i++) {
        ptr += sprintf(ptr, "%.12g", arr->values[i]);
        if (i < arr->size - 1) {
            ptr += sprintf(ptr, ", ");
        }
    }
    
    ptr += sprintf(ptr, "]");
    return result;
}

/* Конвертирует BoolArray в строку */
char* bool_array_to_string(BoolArray* arr) 
{
    if (!arr) return safe_strdup("null");
    if (arr->size == 0) return safe_strdup("[ ]");
    
    size_t estimated = 2 + arr->size * 5; // "<0>" или "<1>" + ", "
    char* result = safe_malloc(estimated);
    char* ptr = result;
    ptr += sprintf(ptr, "[");
    
    for (int i = 0; i < arr->size; i++) {
        ptr += sprintf(ptr, "%s", arr->values[i] ? "<1>" : "<0>");
        if (i < arr->size - 1) {
            ptr += sprintf(ptr, ", ");
        }
    }
    
    ptr += sprintf(ptr, "]");
    return result;
}


/* Конвертирует Array (массив значений) в строку */
char* array_to_string(Array* arr) 
{
    if (!arr) return safe_strdup("null");
    if (arr->size == 0) return safe_strdup("[]");
    
    // первый проход: считаем точный размер.
    size_t total_len = 2; // для '[' и ']'.
    size_t* elem_lengths = NULL;
    char** elem_strings = NULL;
    
    // выделяем память под длины и строки.
    elem_lengths = safe_malloc(arr->size * sizeof(size_t));
    elem_strings = safe_malloc(arr->size * sizeof(char*));
    
    if (!elem_lengths || !elem_strings) {
        free(elem_lengths);
        free(elem_strings);
        return NULL;
    }
    
    // конвертируем все элементы и считаем общую длину.
    for (int i = 0; i < arr->size; i++) {
        Value elem = arr->values[i];
        elem_strings[i] = value_to_qstring(elem);
        
        if (!elem_strings[i]) {
            for (int j = 0; j < i; j++) free(elem_strings[j]);
            free(elem_lengths);
            free(elem_strings);
            return NULL;
        }
        
        elem_lengths[i] = strlen(elem_strings[i]);
        total_len += elem_lengths[i];
        
        if (i < arr->size - 1) total_len += 2;
    }
        
    // один malloc на точный размер (+1 для '\0').
    char* result = safe_malloc(total_len + 1);
    if (!result) {
        for (int i = 0; i < arr->size; i++) free(elem_strings[i]);
        free(elem_lengths);
        free(elem_strings);
        return NULL;
    }
    
    // второй проход: заполняем результат.
    size_t pos = 0;
    result[pos++] = '[';
    
    for (int i = 0; i < arr->size; i++) {
        size_t len = elem_lengths[i];
        memcpy(result + pos, elem_strings[i], len);
        pos += len;
        free(elem_strings[i]);
        
        if (i < arr->size - 1) {
            result[pos++] = ',';
            result[pos++] = ' ';
        }
    }
    
    result[pos++] = ']';
    result[pos] = '\0';
    
    free(elem_lengths);
    free(elem_strings);
    
    return result;
}




/* < PROGRAM LOADER > */

static char* derive_source_path(const char *mrb_path)
{
    const char *dot = strrchr(mrb_path, '.');
    size_t base_len = dot ? (size_t)(dot - mrb_path) : strlen(mrb_path);
    const char *ext = ".msr";
    size_t ext_len = strlen(ext);

    char *result = safe_malloc(base_len + ext_len + 1);
    memcpy(result, mrb_path, base_len);
    memcpy(result + base_len, ext, ext_len + 1);
    return result;
}


/*Find marker*/
int find_marker(uint8_t *code, int file_size, uint8_t marker, int start_offset) {
    for(int i = start_offset; i < file_size - 8; i++) {
        int ok = 1;
        for(int j = 0; j < 9; j++) {
            if(code[i + j] != marker) {
                ok = 0;
                break;
            }
        }
        if(ok) return i + 9; // skip marker
    }
    return -1; // error while finding marker
}

int load_constants(uint8_t *code, int pos, Value *constant_pool, int constant_count) {
    for (int i = 0; i < constant_count; i++) {
        uint8_t type = code[pos++];

        if (type == 1) { // CONST_STRING
        int len = read_int(code + pos);
        pos += 4;

            if (len < 0 || len > 0xCCCCCCCC) {
                msrest_panic("Invalid string length: %d", len);
            }

            char *str = safe_malloc(len + 1);
            memcpy(str, &code[pos], len);
            str[len] = '\0';
            pos += len;

            constant_pool[i] = string_to_value(str);
        }
        else if (type == 2) { // CONST_FLOAT
            double value;
            memcpy(&value, code + pos, 8);
            pos += 8;
            constant_pool[i] = float_to_value(value);
        }
        else if (type == 3) { // CONST_INT
            int64_t value;
            memcpy(&value, code + pos, 8);
            pos += 8;
            constant_pool[i] = int_to_value(value);
        }
        else {
            msrest_panic("ICE: Unknown constant type: %d at pos %d", type, pos - 1);
        }
    }
    return pos;
}

Program* load_program(const char *filename) {

    FILE *f = fopen(filename, "rb");
    if (!f) {
        msrest_panic("Cannot open file: %s\n", filename);
    }
    
    fseek(f, 0, SEEK_END);
    long file_size_long = ftell(f);
    rewind(f);

    if (file_size_long <= 0 || file_size_long > INT_MAX) {
        fclose(f);
        msrest_panic("File too big or empty: %s\n", filename);
    }

    int file_size = (int)file_size_long;
    uint8_t *code = safe_malloc(file_size);
    size_t got = fread(code, 1, file_size, f);
    fclose(f);

    if ((int)got != file_size) {
        free(code);
        msrest_panic("Short read: %s\n", filename);
    }

    int header_start = find_marker(code, file_size, 0xEA, 0);
    int import_start = find_marker(code, file_size, 0xED, 0);
    int pool_start   = find_marker(code, file_size, 0xEC, 0);
    int source_start = find_marker(code, file_size, 0xE1, 0);
    int range_start  = find_marker(code, file_size, 0xE9, 0);
    int code_start   = find_marker(code, file_size, 0xEB, 0);
    int code_end     = find_marker(code, file_size, 0xEF, 0);

    if (code_start == -1) {
        printf("ICE: Code start marker not found\n");
        free(code);
        return NULL;
    }

    Program *prog = safe_malloc(sizeof(Program));
    memset(prog, 0, sizeof(Program));

     /* HEADER: abi(2) + policy(1) + flags(1). */
    uint8_t policy = SOURCE_POLICY_NONE;
    uint8_t flags  = 0;
    if (header_start != -1) {
        int hpos = header_start;
        prog->abi_version = read_short(code + hpos); hpos += 2;
        policy = code[hpos++];
        flags  = code[hpos++];
    }

    /* SOURCE-секция: len(4) + текст. len=0, если исходник не вшит. */
    char *src_text = NULL;
    uint32_t src_len = 0;
    if (source_start != -1) {
        int spos = source_start;
        uint32_t slen = (uint32_t)read_int(code + spos); spos += 4;
        if (slen > 0 && slen < (uint32_t)file_size) {
            src_text = safe_malloc(slen + 1);
            memcpy(src_text, code + spos, slen);
            src_text[slen] = '\0';
            src_len = slen;
        }
    }

    /* создаём Source, если политика не NONE или есть вшитый текст. */
    if (policy != SOURCE_POLICY_NONE || src_text) {
        Source *src = safe_calloc(1, sizeof(Source));
        src->policy = policy;
        src->flags  = flags;
        src->text = src_text;
        src->text_len = src_len;

        if (policy == SOURCE_POLICY_FILE) {
            src->path = derive_source_path(filename);
        }

        prog->source = src;
    } else if (src_text) {
        free(src_text);
    }
    // импортики.
    if (import_start != -1) {
        int ipos = import_start;
        int import_count = read_int(code + ipos);
        ipos += 4;

        prog->import_count = import_count;
        prog->imports = safe_malloc(sizeof(NativeImport) * import_count);

        for (int i = 0; i < import_count; i++) {
            uint16_t module_len = read_short(code + ipos);
            ipos += 2;

            char module[256];
            memcpy(module, code + ipos, module_len);
            module[module_len] = '\0';
            ipos += module_len;

            uint16_t func_len = read_short(code + ipos);
            ipos += 2;

            char func[256];
            memcpy(func, code + ipos, func_len);
            func[func_len] = '\0';
            ipos += func_len;

            prog->imports[i].module = safe_strdup(module);
            prog->imports[i].function = safe_strdup(func);
        }
    }

    // константы.
    int pos = pool_start;
    int constant_count = read_int(code + pos);
    pos += 4;

    Value *constant_pool = safe_malloc(sizeof(Value) * constant_count);
    pos = load_constants(code, pos, constant_pool, constant_count);

    prog->code = code;
    prog->code_size = file_size;
    prog->code_start = code_start;
    prog->code_end = code_end;
    prog->constant_pool = constant_pool;
    prog->constant_count = constant_count;

    /* таблица диапазонов — опциональная секция. */
    prog->ranges = NULL;
    prog->range_count = 0;

    if (range_start != -1) {
        int rpos = range_start;
        uint32_t rc = (uint32_t)read_int(code + rpos);
        rpos += 4;

        prog->range_count = rc;
        prog->ranges = safe_malloc(sizeof(RangeEntry) * rc);

        for (uint32_t i = 0; i < rc; i++) {
            /* формат записи: uint32 start, uint32 end,
                             uint16 line, uint16 col, uint16 end_col, uint16 pad. */
            prog->ranges[i].start_index = (uint32_t)read_int(code + rpos); rpos += 4;
            prog->ranges[i].end_index   = (uint32_t)read_int(code + rpos); rpos += 4;
            prog->ranges[i].line     = read_short(code + rpos); rpos += 2;
            prog->ranges[i].col      = read_short(code + rpos); rpos += 2;
            prog->ranges[i].end_col  = read_short(code + rpos); rpos += 2;
            prog->ranges[i]._pad     = read_short(code + rpos); rpos += 2;
        }
    }

    return prog;
}



/*Cleans constant pool and code sections*/
void free_program(Program *prog) {
    if (!prog) return;
    if (prog->constant_pool) {
        free(prog->constant_pool);}

    if (prog->ranges) {
        free(prog->ranges);}

    /*Free the code*/
    if (prog->code) {
        free(prog->code);}

    if (prog->source) {
        if (prog->source->text) free(prog->source->text);
        if (prog->source->path) free(prog->source->path);
        free(prog->source);
    }
}

/* < BYTECODE VALIDATION > */


/*Checking all the opcodes before execution*/
/*This feature is here to speed up code execution by eliminating runtime checks*/
int Mvalidate_program_opcodes(Program *prog) 
{
    if (!prog || !prog->code) { msrest_panic("ICE: Mvalidate_program_opcodes: invalid program");}
    
    static uint8_t supported_opcodes[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 
        0x1B, 0x1C,
        0x28, 0x29, 0x2A,
        0x31, 0x32, 0x33,
        0x34, 0x35, 0x36, 0x37, 0x38,
        0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
        0x4B, 0x4C, 0x4D, 0x4E,
        0x50,
        0x58, 0x59,
        0x60, 0x66, 0x68,
        0x70, 0x76, 0x77, 0x79, 0x7A,
        0xA0, 0xA1,
        0xBB, 0xEE
    };
    static uint8_t opcode_map[256] = {0};
    static int initialized = 0;
    
    if (!initialized) {
        for (int i = 0; i < sizeof(supported_opcodes); i++) {
            opcode_map[supported_opcodes[i]] = 1;}
        initialized = 1;
    }
    
    int code_start = prog->code_start;
    int code_end = prog->code_end;
    uint8_t *code = prog->code;
    
    int total_instructions = 0;
    int invalid_count = 0;
    int pos = code_start;
    
    /*Scan only up to code_end, but skip the markers.*/
    while (pos < code_end) {
        /*Check: if this is the end marker?*/
        if (pos + 8 < prog->code_size) {
            /* Checking marker EF (end) */
            if (code[pos] == 0xEF && 
                code[pos+1] == 0xEF && 
                code[pos+2] == 0xEF &&
                code[pos+3] == 0xEF &&
                code[pos+4] == 0xEF &&
                code[pos+5] == 0xEF &&
                code[pos+6] == 0xEF &&
                code[pos+7] == 0xEF &&
                code[pos+8] == 0xEF) { break;}
        }
        
        if (pos + 9 > code_end) {
            printf(" Warning: Incomplete instruction at offset 0x%X\n", pos);
            break;
        }
        
        uint8_t opcode = code[pos];
        total_instructions++;
        
        if (!opcode_map[opcode]) {
            invalid_count++;
            printf("  INVALID OPCODE: 0x%02X (%d) at offset 0x%X (instruction #%d)\n", 
                   opcode, opcode, pos, total_instructions - 1);
            printf("\n");
        }
        pos += 9;
    }
    
    if (invalid_count > 0) {
        printf("  VALIDATION FAILED!\n");
        printf("\nValidation results:\n");
        printf("  Total instructions: %d\n", total_instructions);
        printf("  Invalid opcodes:    %d\n", invalid_count);
        printf("  Valid opcodes:      %d\n", total_instructions - invalid_count);
        return 0;} 
    else return 1;
}


// Функция, которая вызывается перед выполнением программы
void Mvalidate_before_execution(Program *prog) 
{
    if (!prog) { msrest_panic("validate_before_execution: program is NULL");}
    if (!Mvalidate_program_opcodes(prog)) {
        msrest_panic(
            "\n"
            "╔════════════════════════════════════════════════════════════╗\n"
            "║   PROGRAM VALIDATION FAILED                                ║\n"
            "║  The bytecode contains unsupported opcodes.                ║\n"
            "║  This VM version may be incompatible with the bytecode.    ║\n"
            "║  Check your compiler output or bytecode version.           ║\n"
            "╚════════════════════════════════════════════════════════════╝\n");}
}

/* < DEBUG > */

char* dict_to_string(Dict *d) {
    if (!d) return safe_strdup("null");
    if (d->size == 0) return safe_strdup("{}");
    
    // Оцениваем размер
    size_t total_len = 2;  // для { и }
    char **key_strings = safe_malloc(d->size * sizeof(char*));
    char **value_strings = safe_malloc(d->size * sizeof(char*));
    
    int idx = 0;
    for (uint32_t i = 0; i < d->capacity; i++) {
        DictEntry *entry = d->buckets[i];
            while (entry) {
                // ключ всегда строка, то есть оборачиваем вручную.
                const char *raw = entry->key->data;
                size_t raw_len = strlen(raw);
                char *quoted = safe_malloc(raw_len + 3);
                quoted[0] = '\'';
                memcpy(quoted + 1, raw, raw_len);
                quoted[raw_len + 1] = '\'';
                quoted[raw_len + 2] = '\0';
                key_strings[idx] = quoted;
                
                // значение через хелпер (строки тоже в кавычках).
                value_strings[idx] = value_to_qstring(entry->value);
                
                total_len += strlen(key_strings[idx]) + strlen(value_strings[idx]) + 4;
                idx++;
                entry = entry->next;
            }
    }
    
    char *result = safe_malloc(total_len + 1);
    char *ptr = result;
    ptr += sprintf(ptr, "{");
    
    for (int i = 0; i < d->size; i++) {
        ptr += sprintf(ptr, "%s: %s", key_strings[i], value_strings[i]);
        if (i < d->size - 1) {
            ptr += sprintf(ptr, ", ");
        }
        free(key_strings[i]);
        free(value_strings[i]);
    }
    
    ptr += sprintf(ptr, "}");
    free(key_strings);
    free(value_strings);
    
    return result;
}


/* Соболезную сам себе */
void vm_print_offsets(void) {
    printf("\n========== VM STRUCTURE OFFSETS ==========\n");
    printf("📐 sizeof(VM) = %zu bytes (0x%zX)\n", sizeof(VM), sizeof(VM));
    printf("\n");
    
    printf("📍 VM offsets:\n");
    printf("  variables:    %zu (0x%zX)\n", offsetof(VM, variables), offsetof(VM, variables));
    printf("  stack:        %zu (0x%zX)\n", offsetof(VM, stack), offsetof(VM, stack));
    printf("  frames:       %zu (0x%zX)\n", offsetof(VM, frames), offsetof(VM, frames));
    printf("  sp:           %zu (0x%zX)\n", offsetof(VM, sp), offsetof(VM, sp));
    printf("  fp:           %zu (0x%zX)\n", offsetof(VM, fp), offsetof(VM, fp));
    printf("  fsp:          %zu (0x%zX)\n", offsetof(VM, fsp), offsetof(VM, fsp));
    printf("  ip:           %zu (0x%zX)\n", offsetof(VM, ip), offsetof(VM, ip));

    printf("  objects:      %zu (0x%zX)\n", offsetof(VM, objects), offsetof(VM, objects));
    printf("  gc:           %zu (0x%zX)\n", offsetof(VM, gc), offsetof(VM, gc));
    
    printf("\n📐 GCState: %zu bytes (0x%zX)\n", sizeof(GCState), sizeof(GCState));
    printf("  phase:           %zu (0x%zX)\n", offsetof(GCState, phase), offsetof(GCState, phase));
    printf("  gray:            %zu (0x%zX)\n", offsetof(GCState, gray), offsetof(GCState, gray));
    printf("  gray_capacity:   %zu (0x%zX)\n", offsetof(GCState, gray_capacity), offsetof(GCState, gray_capacity));
    printf("  gray_head:       %zu (0x%zX)\n", offsetof(GCState, gray_head), offsetof(GCState, gray_head));
    printf("  gray_tail:       %zu (0x%zX)\n", offsetof(GCState, gray_tail), offsetof(GCState, gray_tail));
    printf("  sweep_ptr:       %zu (0x%zX)\n", offsetof(GCState, sweep_ptr), offsetof(GCState, sweep_ptr));
    printf("  allocations:     %zu (0x%zX)\n", offsetof(GCState, allocations), offsetof(GCState, allocations));
    
    printf("\n📐 Value: %zu bytes (0x%zX)\n", sizeof(Value), sizeof(Value));
    printf("  type:    %zu (0x%zX)\n", offsetof(Value, type), offsetof(Value, type));
    // union
    printf("  integer: %zu (0x%zX)\n", offsetof(Value, integer), offsetof(Value, integer));
    printf("  floating:%zu (0x%zX)\n", offsetof(Value, floating), offsetof(Value, floating));
    printf("  string:  %zu (0x%zX)\n", offsetof(Value, string), offsetof(Value, string));
    printf("  array:   %zu (0x%zX)\n", offsetof(Value, array), offsetof(Value, array));
    printf("  int_array:%zu (0x%zX)\n", offsetof(Value, int_array), offsetof(Value, int_array));
    printf("  float_array:%zu (0x%zX)\n", offsetof(Value, float_array), offsetof(Value, float_array));
    printf("  bool_array:%zu (0x%zX)\n", offsetof(Value, bool_array), offsetof(Value, bool_array));
    printf("  native:  %zu (0x%zX)\n", offsetof(Value, native), offsetof(Value, native));
    printf("  logic:   %zu (0x%zX)\n", offsetof(Value, logic), offsetof(Value, logic));
    
    printf("\n📐 Instruction: %zu bytes (0x%zX)\n", sizeof(Instruction), sizeof(Instruction));
    printf("  opcode: %zu (0x%zX)\n", offsetof(Instruction, opcode), offsetof(Instruction, opcode));
    printf("  arg1:   %zu (0x%zX)\n", offsetof(Instruction, arg1), offsetof(Instruction, arg1));
    printf("  arg2:   %zu (0x%zX)\n", offsetof(Instruction, arg2), offsetof(Instruction, arg2));
    
    printf("\n📐 Frame: %zu bytes (0x%zX)\n", sizeof(Frame), sizeof(Frame));
    printf("  return_ip:  %zu (0x%zX)\n", offsetof(Frame, return_ip), offsetof(Frame, return_ip));
    printf("  old_fp:     %zu (0x%zX)\n", offsetof(Frame, old_fp), offsetof(Frame, old_fp));
    printf("  old_sp:     %zu (0x%zX)\n", offsetof(Frame, old_sp), offsetof(Frame, old_sp));
    
    printf("\n📐 Array: %zu bytes (0x%zX)\n", sizeof(Array), sizeof(Array));
    printf("  gc:       %zu (0x%zX)\n", offsetof(Array, gc), offsetof(Array, gc));
    printf("  size:     %zu (0x%zX)\n", offsetof(Array, size), offsetof(Array, size));
    printf("  capacity: %zu (0x%zX)\n", offsetof(Array, capacity), offsetof(Array, capacity));
    printf("  values:   %zu (0x%zX)\n", offsetof(Array, values), offsetof(Array, values));
    
    printf("\n📐 String: %zu bytes (0x%zX)\n", sizeof(String), sizeof(String));
    printf("  gc:     %zu (0x%zX)\n", offsetof(String, gc), offsetof(String, gc));
    printf("  length: %zu (0x%zX)\n", offsetof(String, length), offsetof(String, length));
    printf("  data:   %zu (0x%zX)\n", offsetof(String, data), offsetof(String, data));
    
    printf("\n📐 GCHeader: %zu bytes (0x%zX)\n", sizeof(GCHeader), sizeof(GCHeader));
    printf("  color: %zu (0x%zX)\n", offsetof(GCHeader, color), offsetof(GCHeader, color));
    printf("  type:  %zu (0x%zX)\n", offsetof(GCHeader, type), offsetof(GCHeader, type));
    printf("  next:  %zu (0x%zX)\n", offsetof(GCHeader, next), offsetof(GCHeader, next));
    
    printf("\n========== SIZES ==========\n");
    printf("  VM:           %zu\n", sizeof(VM));
    printf("  Value:        %zu\n", sizeof(Value));
    printf("  Instruction:  %zu\n", sizeof(Instruction));
    printf("  Frame:        %zu\n", sizeof(Frame));
    printf("  Array:        %zu\n", sizeof(Array));
    printf("  String:       %zu\n", sizeof(String));
    printf("  GCHeader:     %zu\n", sizeof(GCHeader));
    printf("  GCState:      %zu\n", sizeof(GCState));
    printf("  Program:      %zu\n", sizeof(Program));
    printf("  NativeObject: %zu\n", sizeof(NativeObject));
    
    printf("\n========== ARRAY SIZES ==========\n");
    printf("  stack[%d]:    %zu bytes\n", STACKCOUNT, sizeof(Value) * STACKCOUNT);
    printf("  frames[%d]:   %zu bytes\n", FRAMECOUNT, sizeof(Frame) * FRAMECOUNT);
    printf("  variables[%d]: %zu bytes\n", VARCOUNT, sizeof(Value) * VARCOUNT);
    printf("  gray[%d]:     %zu bytes\n", vm->gc.gray_capacity, sizeof(GCHeader*) * vm->gc.gray_capacity);
}