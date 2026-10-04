/* << MSREST_API.H >> */

/* < INCLUDE FILES > */

#include "execute.h"
#include "native.h"
    
#if _PROFILE
#include <intrin.h>
#define PROFILE_START() \
    uint64_t __profile_start = __rdtsc()
#define PROFILE_END(op) \
    opcode_cycles[(op)] += __rdtsc() - __profile_start

#else
#define PROFILE_START()
#define PROFILE_END(op)
#endif


/* < MAIN EXECUTION INTERPRETER! > */

int msrest_execute(Program *prog)
{
    Mvalidate_before_execution(prog);

    uint8_t *code = prog->code;
    Value *constant_pool = prog->constant_pool;
    uint32_t code_start = prog->code_start;

    Value *stack = vm->stack;
    int sp = vm->sp;
    int ip = vm->ip;

    uint64_t gc_starts = 0;
    uint64_t gc_steps = 0;
    int gc_interval = GC_INTERVAL;
    uint32_t gc_counter = 0;

    uint32_t instruction_count = (prog->code_end - prog->code_start) / 9;

    srand((unsigned)time(NULL));

    Instruction ins;


#if _PROFILE
    uint64_t opcode_counter[256] = {0};
    uint64_t *instruction_hits = calloc(instruction_count, sizeof(uint64_t));
#endif


    static void *dispatch[256] = {
        [0x00] = &&OP_00,
        [0x01] = &&OP_01,
        [0x02] = &&OP_02,
        [0x03] = &&OP_03,
        [0x04] = &&OP_04,
        [0x05] = &&OP_05,
        [0x06] = &&OP_06,

        [0x09] = &&OP_09,
        [0x0A] = &&OP_0A,
        [0x0B] = &&OP_0B,
        [0x0C] = &&OP_0C,
        [0x0D] = &&OP_0D,
        [0x0E] = &&OP_0E,
        [0x0F] = &&OP_0F,

        [0x10] = &&OP_10,
        [0x11] = &&OP_11,
        [0x12] = &&OP_12,
        [0x13] = &&OP_13,
        [0x14] = &&OP_14,
        [0x15] = &&OP_15,
        [0x16] = &&OP_16,
        [0x17] = &&OP_17,
        [0x18] = &&OP_18,
        [0x19] = &&OP_19,
        [0x1A] = &&OP_1A,
        [0x1B] = &&OP_1B,
        [0x1C] = &&OP_1C,
        [0x1E] = &&OP_1E,
        [0x1F] = &&OP_1F,

        [0x28] = &&OP_28,
        [0x29] = &&OP_29,
        [0x2A] = &&OP_2A,

        [0x31] = &&OP_31,
        [0x32] = &&OP_32,
        [0x33] = &&OP_33,

        [0x34] = &&OP_34,
        [0x35] = &&OP_35,
        [0x36] = &&OP_36,
        [0x37] = &&OP_37,
        [0x38] = &&OP_38,

        [0x3B] = &&OP_3B,
        [0x3C] = &&OP_3C,
        [0x3D] = &&OP_3D,
        [0x3E] = &&OP_3E,
        [0x3F] = &&OP_3F,
        [0x43] = &&OP_43,
        [0x44] = &&OP_44,
        [0x45] = &&OP_45,

        [0x50] = &&OP_50,

        [0x58] = &&OP_58,
        [0x59] = &&OP_59,

        [0x60] = &&OP_60,
        [0x66] = &&OP_66,
        [0x68] = &&OP_68,
        [0x70] = &&OP_70,

        [0x76] = &&OP_76,
        [0x77] = &&OP_77,
        [0x78] = NULL, // здесь было одно провальное решение
        [0x79] = &&OP_79,
        [0x7A] = &&OP_7A,

        [0x82] = &&OP_82,
        [0x83] = &&OP_83,
        [0x85] = &&OP_85,

        [0xA0] = &&OP_A0,
        [0xA1] = &&OP_A1,

        [0xBB] = &&OP_BB,
        [0xEE] = &&OP_EE
    };


#define LPUSH(val) do { \
    stack[sp++] = (val); \
} while (0)

#define LPOP() ({ \
    stack[--sp]; \
})


#if _PROFILE
    #define COUNT_STATS() \
        opcode_counter[ins.opcode]++; \
        instruction_hits[(vm->ip - code_start - 9) / 9]++;
#else
    #define COUNT_STATS() ((void)0)
#endif

#if _PROFILE
#define FETCH_GC() \
    do { \
        if (__builtin_expect(++gc_counter >= gc_interval || gc_allocations >= GC_THRESHOLD, 0)) { \
            gc_counter = 0; \
            gc_allocations = 0; \
            gc_steps++; \
            vm->ip = ip; \
            vm->sp = sp; \
            if (vm->gc.phase == GC_IDLE) { \
                gc_starts++; \
                gc_start(); \
            } else { \
                gc_step(GC_STEP_COUNT); \
            } \
            gc_interval = GC_INTERVAL; \
        } \
    } while (0)
#else
#define FETCH_GC() \
    do { \
        if (__builtin_expect(++gc_counter >= gc_interval || gc_allocations >= GC_THRESHOLD, 0)) { \
            gc_counter = 0; \
            gc_allocations = 0; \
            vm->ip = ip; \
            vm->sp = sp; \
            if (vm->gc.phase == GC_IDLE) { \
                gc_starts++; \
                gc_start(); \
            } else { \
                gc_step(GC_STEP_COUNT); \
            } \
            gc_interval = GC_INTERVAL; \
        } \
    } while (0)
#endif

#if _PROFILE
    #define FETCH() \
        ins = fetch(code, ip); \
        ip += 9; \
        opcode_counter[ins.opcode]++; \
        instruction_hits[(ip - code_start - 9) / 9]++; \
        FETCH_GC()
#else
    #define FETCH() \
        ins = fetch(code, ip); \
        ip += 9; \
        FETCH_GC()
#endif


#if _PROFILE
    static uint64_t opcode_cycles[256];
    #define PROF_START() \
        uint64_t __profile_start = __rdtsc()
    #define PROF_END() \
        opcode_cycles[ins.opcode] += __rdtsc() - __profile_start
#else
    #define PROF_START()
    #define PROF_END()
#endif

#define NEXT() do {goto *dispatch[ins.opcode]; } while (0)
    
    FETCH();
    NEXT();


/* < ALL INSTRUCTION HANDLERS > */


OP_00: {
    #if _PROFILE
    PROF_START(); // PUSH
    #endif

    LPUSH(int_to_value(ins.arg1));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_01: {
    #if _PROFILE
    PROF_START(); // LOAD
    #endif

    LPUSH(vm->variables[ins.arg1]);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_02: {
    #if _PROFILE
    PROF_START(); // STORE
    #endif

    Value new_value = LPOP();
    vm->variables[ins.arg1] = new_value;
    if (vm->gc.phase != GC_IDLE) mark_value(new_value);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_03: {
    #if _PROFILE
    PROF_START(); // DROP
    #endif

    LPOP();

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_04: {
    #if _PROFILE
    PROF_START(); // PUSH_NULL
    #endif

    LPUSH(null_to_value());

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_05: {
    #if _PROFILE
    PROF_START(); // LOAD from frame
    #endif

    LPUSH(vm->stack[vm->fp + ins.arg1]);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_06: {
    #if _PROFILE
    PROF_START(); // STORE to frame
    #endif

    uint32_t slot = ins.arg1;
    Value new_value = LPOP();
    vm->stack[vm->fp + slot] = new_value;

    if (vm->gc.phase != GC_IDLE) mark_value(new_value);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_09: {
    #if _PROFILE
    PROF_START(); // IDIV
    #endif

    Value b = LPOP();
    Value a = LPOP();

    if (!IS_NUMBER(a) || !IS_NUMBER(b)) {
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);    
        msrest_panic("Type error: expected INTEGER or FLOAT, got %s and %s types\n", imm1, imm2);
    }

    int64_t int_a = (a.type == MSREST_INT) ? a.integer : (int64_t)value_to_double(a);
    int64_t int_b = (b.type == MSREST_INT) ? b.integer : (int64_t)value_to_double(b);
    if (int_b == 0) msrest_panic("Access violation error: Cannot divide without remainder by zero\n");

    LPUSH(int_to_value(int_a / int_b));

    #if _PROFILE
    PROF_END();
    #endif
    FETCH();
    NEXT();
}


OP_0A: {  // ADD
    Value b = LPOP();
    Value a = LPOP();
    Value result;

    
    // если оба числа → складываем.
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        if (both_int(a, b)) {
            int64_t imm;
            if (__builtin_add_overflow(a.integer, b.integer, &imm)) {
                msrest_panic("Overflow error: 'add' operator got integer overflow\n");
            }
            result = int_to_value(imm);
        } else {
            double r = value_to_double(a) + value_to_double(b);
            if (!isfinite(r)) {
                msrest_panic("Overflow error: 'add' operator got non-finite float\n");
            }
            result = float_to_value(r);
        }
        LPUSH(result);
    }
    // если хотя бы один - строка, то конкатенируем.
    else if (a.type == MSREST_STRING || b.type == MSREST_STRING) {
        char* a_str = (a.type == MSREST_STRING) ? a.string->data : value_to_string(a);
        char* b_str = (b.type == MSREST_STRING) ? b.string->data : value_to_string(b);
        
        char* result_str = concat_strings(a_str, b_str);

        
        LPUSH(string_to_value(result_str));
        free(result_str);
        // если преобразовывали число, освобождаем.
        if (a.type != MSREST_STRING) free(a_str);
        if (b.type != MSREST_STRING) free(b_str);
    }
    else {
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);
        msrest_panic("Type error: 'add' expected NUMBER or STRING, got %s and %s types\n", imm1, imm2);
    }
    
    FETCH();
    NEXT();
}

OP_0B: {
    #if _PROFILE
    PROF_START(); // SUB
    #endif

    Value b = LPOP();
    Value a = LPOP();
    Value result;
    if(!IS_NUMBER(a) || !IS_NUMBER(b)) {
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);
        msrest_panic("Type error: 'sub' expected INT or FLOAT, got %s and %s types\n", imm1, imm2);
    }
    else if(both_int(a, b)){
        int64_t imm;
        if (__builtin_sub_overflow(a.integer, b.integer, &imm)) {
            msrest_panic("Overflow error: 'sub' operator got integer overflow\n");
        }
        result = int_to_value(imm);
    }
    else {
        double r = value_to_double(a) - value_to_double(b);
        if (!isfinite(r)) {
            msrest_panic("Overflow error: 'sub' operator got non-finite float\n");
        }
        result = float_to_value(r);
    }
    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_0C: {
    #if _PROFILE
    PROF_START(); // MUL
    #endif

    Value b = LPOP();
    Value a = LPOP();
    Value result;
    if(!IS_NUMBER(a) || !IS_NUMBER(b)) {
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);
        msrest_panic("Type error: 'mul' expected INT or FLOAT, got %s and %s types\n", imm1, imm2);
    }
    else if(both_int(a, b)){
        int64_t imm;
        if (__builtin_mul_overflow(a.integer, b.integer, &imm)) {
            msrest_panic("Overflow error: 'mul' operator got integer overflow\n");
        }
        result = int_to_value(imm);
    }
    else {
        double r = value_to_double(a) * value_to_double(b);
        if (!isfinite(r)) {
            msrest_panic("Overflow error: 'mul' operator got non-finite float\n");
        }
        result = float_to_value(r);
    }
    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_0D: {
    #if _PROFILE
    PROF_START(); // DIV
    #endif

    Value b = LPOP();
    Value a = LPOP();

    if (__builtin_expect(!IS_NUMBER(a) || !IS_NUMBER(b), 0)) {
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);
        msrest_panic("Type error: 'div' expected INT or FLOAT, got %s and %s types\n", imm1, imm2);
    }

    double da = value_to_double(a);
    double db = value_to_double(b);

    if (__builtin_expect(db == 0.0, 0)) {
        msrest_panic("Arithmetic error: Division by zero\n");
    }

    double r = da / db;

    if (__builtin_expect(!isfinite(r), 0)) {
        msrest_panic("Overflow error: 'div' got float overflow\n");
    }

    Value result = float_to_value(r);
    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_0E: {

    #if _PROFILE
    PROF_START(); // MOD
    #endif

    Value b = LPOP();
    Value a = LPOP();
    if (__builtin_expect(a.type != MSREST_INT || b.type != MSREST_INT, 0)){
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);
        msrest_panic("Type error: 'modulo' expects INTEGER types, got %s and %s types\n", imm1, imm2);
    }
    else if (b.integer == 0){
        msrest_panic("Evaluating error: Cannot modulo by zero\n");
    }
    else if (__builtin_expect(a.integer == INT64_MIN && b.integer == -1, 0)) {
        msrest_panic("Overflow error: 'modulo' got integer overflow\n");
    }
    LPUSH(int_to_value(a.integer % b.integer));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_0F: {
    #if _PROFILE
    PROF_START(); // NEG
    #endif

    Value v = LPOP();
    Value result = {0};
    switch(v.type) {
        case MSREST_INT: { 
            if (__builtin_expect(v.integer == INT64_MIN, 0)) {
                msrest_panic("Overflow error: 'negate' got integer overflow\n");
            }
            result = int_to_value(-v.integer); 
            break; }
        case MSREST_FLOAT: { 
            result = float_to_value(-v.floating); 
            break; }
        default: 
            msrest_panic("Type error: 'negate' expects INT/FLOAT\n");
    }
    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_10: {
    #if _PROFILE
    PROF_START(); // INC GLOBAL
    #endif

    Value *v = &vm->variables[ins.arg1];
    Value result;

    switch (v->type) {
        case MSREST_INT: {
            int64_t old = v->integer;
            int64_t new;

            if (__builtin_expect(__builtin_add_overflow(old, (int64_t)1, &new), 0)) {
                msrest_panic("Overflow error: 'increment' got integer overflow\n");
            }
            v->integer = new;
            result = int_to_value(ins.arg2 ? old : new);
            break;
        }
        case MSREST_FLOAT: {
            double old = v->floating;
            double next = old + 1.0;

            if (__builtin_expect(!isfinite(next), 0)) {
                msrest_panic("Overflow error: 'increment' got non-finite float\n");
            }
            v->floating = next;
            result = float_to_value(ins.arg2 ? old : next);
        }
        default:
            msrest_panic("Type error: cannot increment %s, expected INTEGER or FLOAT\n", type_name(v->type));
    }

    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_11: {
    #if _PROFILE
    PROF_START(); // DEC GLOBAL
    #endif

    Value *v = &vm->variables[ins.arg1];
    Value result;

    switch (v->type) {
        case MSREST_INT: {
            int64_t old = v->integer;
            int64_t new;
            if (__builtin_expect(__builtin_sub_overflow(old, (int64_t)1, &new), 0)) {
                msrest_panic("Overflow error: 'decrement' got integer overflow\n");
            }
            v->integer = new;
            result = int_to_value(ins.arg2 ? old : new);
            break;
        }
        case MSREST_FLOAT: {
            double old = v->floating;
            double next = old - 1.0;

            if (__builtin_expect(!isfinite(next), 0)) {
                msrest_panic("Overflow error: 'decrement' got non-finite float\n");
            }
            v->floating = next;
            result = float_to_value(ins.arg2 ? old : next);
        }
        default:
            msrest_panic("Type error: cannot decrement %s, expected INTEGER or FLOAT\n", type_name(v->type));
    }

    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_12: {  // INC_LOCAL <slot> <is_post>
    Value *v = &vm->stack[vm->fp + ins.arg1];
    Value result;

    switch (v->type) {
        case MSREST_INT: {
            int64_t old = v->integer;
            int64_t new;
            if (__builtin_expect(__builtin_add_overflow(old, (int64_t)1, &new), 0)) {
                msrest_panic("Overflow error: 'increment' got integer overflow\n");
            }
            v->integer = new;
            result = int_to_value(ins.arg2 ? old : new);
            break;
        }
        case MSREST_FLOAT: {
            double old = v->floating;
            double next = old + 1.0;

            if (__builtin_expect(!isfinite(next), 0)) {
                msrest_panic("Overflow error: 'increment' got non-finite float\n");
            }
            v->floating = next;
            result = float_to_value(ins.arg2 ? old : next);
        }
        default:
            msrest_panic("Type error: cannot increment %s, expected INTEGER or FLOAT\n", type_name(v->type));
    }
    LPUSH(result);
    FETCH(); NEXT();
}

OP_13: {
    #if _PROFILE
    PROF_START(); // DEC_LOCAL <slot> <is_post>
    #endif

    Value *v = &vm->stack[vm->fp + ins.arg1];
    Value result;

    switch (v->type) {
        case MSREST_INT: {
            int64_t old = v->integer;
            int64_t new;
            if (__builtin_expect(__builtin_sub_overflow(old, (int64_t)1, &new), 0)) {
                msrest_panic("Overflow error: 'decrement' got integer overflow\n");
            }
            v->integer = new;
            result = int_to_value(ins.arg2 ? old : new);
            break;
        }
        case MSREST_FLOAT: {
            double old = v->floating;
            double next = old - 1.0;

            if (__builtin_expect(!isfinite(next), 0)) {
                msrest_panic("Overflow error: 'decrement' got non-finite float\n");
            }
            v->floating = next;
            result = float_to_value(ins.arg2 ? old : next);
        }
        default:
            msrest_panic("Type error: cannot decrement %s, expected INTEGER or FLOAT\n", type_name(v->type));
    }

    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_14: {
    #if _PROFILE
    PROF_START(); // LESS
    #endif

    Value b = LPOP();
    Value a = LPOP();

    if (__builtin_expect(!IS_NUMBER(a) || !IS_NUMBER(b), 0)) {
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);
        msrest_panic("Type error: 'less' expected FLOAT or INTEGER types, got unsupported type %s and %s\n", imm1, imm2);
    }
    Value result = bool_to_value(value_to_double(a) < value_to_double(b));

    LPUSH(result);
        
    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_15: {
    #if _PROFILE
    PROF_START(); // GREATER
    #endif

    Value b = LPOP();
    Value a = LPOP();
    if (__builtin_expect(!IS_NUMBER(a) || !IS_NUMBER(b), 0)) {
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);
        msrest_panic("Type error: 'greater' expected FLOAT or INTEGER types, got unsupported type %s and %s\n", imm1, imm2);
    }

    Value result = bool_to_value(value_to_double(a) > value_to_double(b));
    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_16: {
    #if _PROFILE
    PROF_START(); // EQUAL
    #endif
    
    Value b = LPOP();
    Value a = LPOP();
    Value result;

    if (a.type != b.type) {
        result = bool_to_value(0);}
    else {
        switch (a.type) {
            case MSREST_NULL:
                result = bool_to_value(1);
                break;

            case MSREST_BOOL:
                result = bool_to_value(a.integer == b.integer);
                break;

            case MSREST_INT:
                result = bool_to_value(a.integer == b.integer);
                break;

            case MSREST_FLOAT:
                result = bool_to_value(a.floating == b.floating);
                break;

            case MSREST_STRING: {
                if (a.string == b.string) {
                    result = bool_to_value(1);
                    break;
                }
                // разная длина.
                if (a.string->length != b.string->length) {
                    result = bool_to_value(0);
                    break;
                }
                // побайтовое сравнение.
                result = bool_to_value(string_equal(a.string, b.string));
                break; 
                }
                
            default: {
                const char* imm1 = type_name(a.type);
                const char* imm2 = type_name(b.type);
                msrest_panic("Type error: 'not equal' got unsupported type %s and %s\n", imm1, imm2);
            }
        }
    }

    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_17: {
    #if _PROFILE
    PROF_START(); // NOT EQUAL
    #endif
        
    Value b = LPOP();
    Value a = LPOP();
    Value result;
    if(a.type == MSREST_NULL && b.type == MSREST_NULL) 
        result = bool_to_value(0);

    else if(a.type == MSREST_NULL || b.type == MSREST_NULL)
        result = bool_to_value(1);

    else if(IS_NUMBER(a) && IS_NUMBER(b))
        result = bool_to_value(value_to_double(a) != value_to_double(b));
    
    else{
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);
        msrest_panic("Type error: 'not equal' got unsupported type %s and %s\n", imm1, imm2);
    }

    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_18: {
    #if _PROFILE
    PROF_START(); // LESS OR EQUAL
    #endif

    Value b = LPOP();
    Value a = LPOP();

    if(__builtin_expect(!IS_NUMBER(a) || !IS_NUMBER(b), 0)){
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);
        msrest_panic("Type error: 'less or equal' expected FLOAT or INTEGER types, got %s and %s\n", imm1, imm2);
    }
    Value result = bool_to_value(value_to_double(a) <= value_to_double(b));
    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_19: {
    #if _PROFILE
    PROF_START(); // GREATER OR EQUAL
    #endif
    Value b = LPOP();
    Value a = LPOP();

    if(__builtin_expect(!IS_NUMBER(a) || !IS_NUMBER(b), 0)) {
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);
        msrest_panic("Type error: 'Greater or equal' expected FLOAT or INTEGER types, got %s and %s\n", imm1, imm2);
    }

    Value result = bool_to_value(value_to_double(a) >= value_to_double(b));
    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_1A: {
    #if _PROFILE
    PROF_START(); // LTINT
    #endif
    
    int64_t b = LPOP().integer;
    int64_t a = LPOP().integer;
    LPUSH(bool_to_value(a < b));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_1B: {
    #if _PROFILE
    PROF_START(); // SAME
    #endif

    Value b = LPOP();
    Value a = LPOP();
    
    uint8_t result = 0;
    if (a.type == b.type) {
        switch (a.type) {
            // ссылочные типы - сравнение указателей.
            case MSREST_STRING:      result = (a.string == b.string); break;
            case MSREST_ARRAY:       result = (a.array == b.array); break;
            case MSREST_INT_ARRAY:   result = (a.int_array == b.int_array); break;
            case MSREST_FLOAT_ARRAY: result = (a.float_array == b.float_array); break;
            case MSREST_BOOL_ARRAY:  result = (a.bool_array == b.bool_array); break;
            case MSREST_DICT:        result = (a.dictionary == b.dictionary); break;
            case MSREST_NATIVE:      result = (a.native == b.native); break;
            
            case MSREST_INT:
                case MSREST_FLOAT:
                    case MSREST_BOOL:
                        case MSREST_NULL: result = 0; break;

            case MSREST_FUNCREF:
                result = (a.fnref.addr == b.fnref.addr && a.fnref.arity == b.fnref.arity);
                break;
        }
    }
    LPUSH(bool_to_value(result));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH(); 
    NEXT();
}


OP_1C: {
    #if _PROFILE
    PROF_START(); // NOT
    #endif

    Value v = LPOP();

    if (__builtin_expect(v.type != MSREST_BOOL, 0)) 
        msrest_panic("Type error: 'NOT' expected BOOL type, got %s\n", type_name(v.type));
    Value result = bool_to_value(!value_to_bool(v));
    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_1E: {
    #if _PROFILE
    PROF_START(); // AND
    #endif

    Value b = LPOP();
    Value a = LPOP();
    Value result;

    if (IS_NULL(a) || IS_NULL(b)) 
        result = null_to_value();

    else if (__builtin_expect(a.type != MSREST_BOOL || b.type != MSREST_BOOL, 0)) {
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);
        msrest_panic("Type error: logic 'or' expected BOOL and BOOL types, got %s and %s\n", imm1, imm2);
    }
    else result = bool_to_value(value_to_bool(a) && value_to_bool(b));

    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif
    FETCH();
    NEXT();
}


OP_1F: {
    #if _PROFILE
    PROF_START(); // OR
    #endif

    Value b = LPOP();
    Value a = LPOP();

    if (IS_NULL(a) || IS_NULL(b)) 
        LPUSH(null_to_value());
    else if (__builtin_expect(a.type != MSREST_BOOL || b.type != MSREST_BOOL, 0)) {      
        const char* imm1 = type_name(a.type);
        const char* imm2 = type_name(b.type);
        msrest_panic("Type error: logic 'or' expected BOOL and BOOL types, got %s and %s\n", imm1, imm2);
    }
    else 
    LPUSH(bool_to_value(a.logic || b.logic));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_28: {
    #if _PROFILE
    PROF_START(); // PRINT
    #endif

    Value v = LPOP();
    char *s = value_to_string(v);
    printf("%s", s);
    free(s);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_29: {
    #if _PROFILE
    PROF_START(); // PRINTLN
    #endif

    Value v = LPOP();
    char *s = value_to_string(v);
    printf("%s\n", s);
    free(s);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_2A: {
    #if _PROFILE
    PROF_START(); // INPUT
    #endif

    // TODO рефакторинг в отдельную функцию
    size_t cap = 256;
    size_t len = 0;
    char *buf = safe_malloc(cap);

    while (fgets(buf + len, cap - len, stdin)) {
        len += strlen(buf + len);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[--len] = '\0';
            break;
        }
        if (len + 1 >= cap) {
            cap *= 2;
            buf = safe_realloc(buf, cap);
        }
    }

    if (len == 0 && feof(stdin)) {
        free(buf);
        msrest_panic("Input error: failed to read from stdin\n");
    }

    LPUSH(string_to_value(buf));
    free(buf);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_31: {
    #if _PROFILE
    PROF_START();    //PUSH_BOOL
    #endif

    LPUSH(bool_to_value(ins.arg1));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_32: {
    #if _PROFILE
    PROF_START(); // JMP_IF_FALSE
    #endif

    if(!value_to_bool(LPOP())) ip = code_start + ins.arg1;

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_33: {
    #if _PROFILE
    PROF_START(); // JMP
    #endif

    ip = code_start + ins.arg1;
    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_34: {
    #if _PROFILE
    PROF_START(); // CALL
    #endif

    uint32_t target = ins.arg1;
    uint32_t argc = ins.arg2;
    int num = sp - argc;
    Frame *f = &vm->frames[vm->fsp++];

    f->return_ip = ip;
    f->old_fp = vm->fp;
    f->old_sp = num;
    vm->fp = num;
    ip = code_start + target;

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_35: {
    #if _PROFILE
    PROF_START(); // RETURN
    #endif

    Frame *f = &vm->frames[--vm->fsp];
    Value ret = LPOP();

    sp = f->old_sp;
    vm->fp = f->old_fp;
    ip = f->return_ip;

    LPUSH(ret);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_36: {
    #if _PROFILE
    PROF_START(); // ENTER
    #endif

    uint32_t extra = ins.arg1;

    if (__builtin_expect(extra == 0, 1)) {
        #if _PROFILE
        PROF_END();
        #endif
        FETCH();
        NEXT();}

    size_t bytes = (size_t)extra * sizeof(Value);
    memset(&stack[sp], 0, bytes);
    sp += extra;

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_37: { 
    #if _PROFILE
    PROF_START(); // PUSH_FN
    #endif

    Value v;
    v.type = MSREST_FUNCREF;
    v.fnref.addr = ins.arg1;
    v.fnref.arity = ins.arg2;
    LPUSH(v);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_38: {
    #if _PROFILE
    PROF_START(); // CALL_REF
    #endif

    uint32_t argc = ins.arg1;
    Value fnv = LPOP();

    if (fnv.type != MSREST_FUNCREF) 
        msrest_panic("Type error: 'indirect call' expected a function type, got %s\n", type_name(fnv.type));
    
    if (fnv.fnref.arity != argc) 
        msrest_panic("'indirect call' expected %u args, got %u\n", fnv.fnref.arity, argc);


    uint32_t target = fnv.fnref.addr;
    int num = sp - argc;
    Frame *f = &vm->frames[vm->fsp++];

    f->return_ip = ip;
    f->old_fp = vm->fp;
    f->old_sp = num;
    vm->fp = num;
    ip = code_start + target;

    #if _PROFILE
    PROF_END();
    #endif

    FETCH(); 
    NEXT();
}


OP_3B: {
    #if _PROFILE
    PROF_START(); // ARR_POP
    #endif
    Value array = LPOP();
    if(array.type == MSREST_INT_ARRAY){
        IntArray *arr = array.int_array;
        if(arr->size == 0) msrest_panic("Access violation error: Pop from empty INT ARRAY\n");
        int64_t value = arr->values[--arr->size];
        LPUSH(int_to_value(value));
    }
    else if(array.type == MSREST_ARRAY){
        Array *arr = array.array;
        if(arr->size == 0) msrest_panic("Access violation error: Pop from empty ARRAY\n");
        Value value = arr->values[--arr->size];
        LPUSH(value);
    }
    else if(array.type == MSREST_BOOL_ARRAY) {
        BoolArray *arr = array.bool_array;
        if(arr->size == 0) msrest_panic("Access violation error: Pop from empty BOOL ARRAY\n");
        uint8_t value = arr->values[--arr->size];
        LPUSH(bool_to_value(value));
    }
    else if(array.type == MSREST_FLOAT_ARRAY) {
        FloatArray *arr = array.float_array;
        if(arr->size == 0) msrest_panic("Access violation error: Pop from empty FLOAT ARRAY\n");
        double value = arr->values[--arr->size];
        LPUSH(float_to_value(value));
    }

    else msrest_panic("Type error: 'array pop' expected ARRAY, got %s\n", type_name(array.type));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_3C: {
    #if _PROFILE
    PROF_START(); // ARRAY_CREATE
    #endif

    uint32_t count = ins.arg1;
    Array *arr = gc_alloc(sizeof(Array), MSREST_ARRAY);
    arr->size = count;
    arr->capacity = (count > 8) ? count : 8;
    arr->values = safe_malloc(sizeof(Value) * arr->capacity);
    memcpy(arr->values, &stack[sp - count], count * sizeof(Value));
    sp -= count;

    Value v = {0};
    v.type = MSREST_ARRAY;
    v.array = arr;
    LPUSH(v);

    #if _PROFILE
    PROF_END();
    #endif
    FETCH();
    NEXT();
}


OP_3D: {
    #if _PROFILE
    PROF_START(); // CONT_SET
    #endif

    Value val  = LPOP();
    Value key  = LPOP();
    Value cont = LPOP();

    switch (cont.type) {

        case MSREST_ARRAY: {
            Array *arr = cont.array;
            int64_t index = value_to_int(key);
            arr->values[index] = val;
            if (vm->gc.phase != GC_IDLE) {
                mark_value(val);
            }
            break;
        }
        case MSREST_INT_ARRAY: {
            IntArray *arr = cont.int_array;
            arr->values[value_to_int(key)] = value_to_int(val);
            break;
        }
        case MSREST_FLOAT_ARRAY: {
            FloatArray *arr = cont.float_array;
            arr->values[value_to_int(key)] = value_to_double(val);
            break;
        }
        case MSREST_BOOL_ARRAY: {
            BoolArray *arr = cont.bool_array;
            arr->values[value_to_int(key)] = value_to_int(val);
            break;
        }

        case MSREST_STRING:
            msrest_panic("Access violation error: Cannot mutate element in string\n");

        case MSREST_DICT: {
            if (key.type != MSREST_STRING) {
                msrest_panic("'CONT_SET' on DICT expects STRING key, got %s\n",
                             type_name(key.type));
            }
            dict_set(cont, key, val);
            break;
        }

        default:
            msrest_panic("Type error: 'set element' expected ARRAY or DICT, got %s\n",
                         type_name(cont.type));
    }

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_3E: {  // CONT_GET
    #if _PROFILE
    PROF_START(); // CONT_GET
    #endif

    Value key  = LPOP();
    Value cont = LPOP();
    Value result;

    switch (cont.type) {

        case MSREST_ARRAY:
            result = array_get(cont, value_to_int(key));
            break;
        case MSREST_INT_ARRAY:
            result = int_to_value(iarray_get(cont, value_to_int(key)));
            break;
        case MSREST_FLOAT_ARRAY:
            result = float_to_value(farray_get(cont, value_to_int(key)));
            break;
        case MSREST_BOOL_ARRAY:
            result = bool_to_value(barray_get(cont, value_to_int(key)));
            break;

        // неэффективно, ну да ладно.
        // пока что ищем вручную через руны.
        case MSREST_STRING: {
            int64_t index = value_to_int(key);
            String *str = cont.string;

            if (index < 0 || (uint64_t)index >= str->chars) {
                msrest_panic(
                    "Access violation error: String index out of bounds: "
                    "%lld (string length=%u)\n",
                    (long long)index, str->chars);
            }

            size_t pos = 0;
            uint32_t current = 0;
            uint32_t len = str->length;
            while (pos < len && current < index) {
                uint8_t c = (uint8_t)str->data[pos];
                if      ((c & 0x80) == 0)    pos += 1;
                else if ((c & 0xE0) == 0xC0) pos += 2;
                else if ((c & 0xF0) == 0xE0) pos += 3;
                else if ((c & 0xF8) == 0xF0) pos += 4;
                else                         pos += 1;
                current++;
            }

            uint8_t c = (uint8_t)str->data[pos];
            size_t char_bytes;
            if      ((c & 0x80) == 0)    char_bytes = 1;
            else if ((c & 0xE0) == 0xC0) char_bytes = 2;
            else if ((c & 0xF0) == 0xE0) char_bytes = 3;
            else if ((c & 0xF8) == 0xF0) char_bytes = 4;
            else                         char_bytes = 1;

            char *tmp = safe_malloc(char_bytes + 1);
            memcpy(tmp, str->data + pos, char_bytes);
            tmp[char_bytes] = '\0';
            result = string_to_value(tmp);
            free(tmp);
            break;
        }

        case MSREST_DICT: {
            if (key.type != MSREST_STRING) {
                msrest_panic("Type error: 'CONT_GET' on DICT expects STRING key, got %s\n",
                             type_name(key.type));
            }
            result = dict_get(cont, key);
            break;
        }

        default:
            msrest_panic("Access violation error: %s object is not indexable\n",
                         type_name(cont.type));
    }

    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_3F: {
    #if _PROFILE
    PROF_START(); // LEN
    #endif

    Value v = LPOP();
    Value result = {0};
    switch(v.type) {
        case MSREST_ARRAY: result = int_to_value(v.array->size); break;
        case MSREST_STRING: result = int_to_value(v.string->chars); break;
        case MSREST_INT_ARRAY: result = int_to_value(v.int_array->size); break;
        case MSREST_FLOAT_ARRAY: result = int_to_value(v.float_array->size); break;
        case MSREST_BOOL_ARRAY:  result = int_to_value(v.bool_array->size); break;
        case MSREST_DICT: result = int_to_value(v.dictionary->size); break;
        default: msrest_panic("Type error: len() expected ARRAY or STRING, got %s\n", type_name(v.type));
    }
    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}



OP_43: {
    #if _PROFILE
    PROF_START(); // ARRAY_PUSH_MANY
    #endif

    uint32_t count = ins.arg1;

    if (__builtin_expect(count == 0 || count > 256, 0)) {
        msrest_panic("ARRAY_PUSH_MANY: count %u out of range [1..256]\n", count);
    }

    Value values[256];
    for (int i = (int)count - 1; i >= 0; i--) {
        values[i] = LPOP();
    }

    Value arr_val = LPOP();
    
    switch(arr_val.type) {
        case MSREST_ARRAY: {
            Array *arr = arr_val.array;
            while (arr->size + count > arr->capacity) {
                arr->capacity *= 2;
                arr->values = safe_realloc(arr->values, sizeof(Value) * arr->capacity);}

            for (uint_fast16_t i = 0; i < count; i++) {
                arr->values[arr->size++] = values[i];
                if (vm->gc.phase != GC_IDLE)
                    mark_value(values[i]);}

            break;
        }
        case MSREST_INT_ARRAY: {
            IntArray *arr = arr_val.int_array;
            uint32_t needed = arr->size + count;
            
            if (needed > arr->capacity) {
                uint32_t new_capacity = arr->capacity ? arr->capacity : 1;
                while (new_capacity < needed) new_capacity <<= 1;
                arr->values = safe_realloc(arr->values, sizeof(int64_t) * new_capacity);
                arr->capacity = new_capacity;
            }

            int64_t *dest = arr->values + arr->size;
            
            for (uint_fast32_t i = 0; i < count; i++) {
                dest[i] = values[i].integer;
            }
            
            arr->size = needed;
            break;
        }
        case MSREST_FLOAT_ARRAY: {
            FloatArray *arr = arr_val.float_array;
            while(arr->size + count > arr->capacity) {
                arr->capacity *= 2;
                arr->values = safe_realloc(arr->values, sizeof(double) * arr->capacity);}

            for(uint_fast16_t i = 0; i < count; i++) {
                arr->values[arr->size++] = value_to_double(values[i]);}
            break;
        }
        case MSREST_BOOL_ARRAY: {
            BoolArray *arr = arr_val.bool_array;
            while(arr->size + count > arr->capacity) {
                arr->capacity *= 2;
                arr->values = safe_realloc(arr->values, sizeof(uint8_t) * arr->capacity);}

            for(uint_fast32_t i = 0; i < count; i++) {
                arr->values[arr->size++] = value_to_bool(values[i]);}
            break;
        }
        default:
            msrest_panic("Type error: 'array push' expects ARRAY type, got %s\n", type_name(arr_val.type));}
    LPUSH(arr_val);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}   

OP_44: { //MONOPUSH
    #if _PROFILE
    PROF_START();
    #endif

    Value value = LPOP();
    Value arr = LPOP();
    
    array_push(arr, value);
    LPUSH(arr);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}



OP_45: {
    #if _PROFILE
    PROF_START(); // TYPED_ARRAY
    #endif

    uint32_t type = ins.arg1;
    uint32_t count = ins.arg2;
    
    switch(type) {   
        case 1: {
            IntArray *arr = gc_alloc(sizeof(IntArray), MSREST_INT_ARRAY);
            arr->size = count;
            arr->capacity = (count > 8) ? count : 8;
            arr->values = safe_malloc(sizeof(int64_t) * arr->capacity);
            
            for(int i = count - 1; i >= 0; i--){
                Value v = LPOP();
                arr->values[i] = value_to_int(v);
            }
            
            Value v = {0};
            v.type = MSREST_INT_ARRAY;
            v.int_array = arr;
            LPUSH(v);
            break;
        }
        case 2: {
            FloatArray *arr = gc_alloc(sizeof(FloatArray), MSREST_FLOAT_ARRAY);
            arr->size = count;
            arr->capacity = (count > 8) ? count : 8;
            arr->values = safe_malloc(sizeof(double) * arr->capacity);
            
            for(int i = count - 1; i >= 0; i--){
                Value v = LPOP();
                arr->values[i] = value_to_double(v);
            }
            
            Value v = {0};
            v.type = MSREST_FLOAT_ARRAY;
            v.float_array = arr;
            LPUSH(v);
            break;
        }
        case 3: {
            BoolArray *arr = gc_alloc(sizeof(BoolArray), MSREST_BOOL_ARRAY);
            arr->size = count;
            arr->capacity = (count > 8) ? count : 8;
            arr->values = safe_malloc(sizeof(uint8_t) * arr->capacity);
            
            for(int i = count - 1; i >= 0; i--){
                Value v = LPOP();
                arr->values[i] = value_to_bool(v);
            }
            
            Value v = {0};
            v.type = MSREST_BOOL_ARRAY;
            v.bool_array = arr;
            LPUSH(v);
            break;
        }
    }

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_50: {
    #if _PROFILE
    PROF_START(); // PUSH_CONST
    #endif

    uint32_t id = ins.arg1;
    LPUSH(constant_pool[id]);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_58: {
    #if _PROFILE
    PROF_START(); // SHR с аргументом.
    #endif

    Value a = LPOP(); // значение для сдвига.
    if (a.type != MSREST_INT) 
        msrest_panic("Type error: SHR expects INTEGER type, got %s type\n", type_name(a.type));
    uint32_t shift = (uint32_t)ins.arg1 & 63u;
    Value result = int_to_value(a.integer >> shift);
    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_59: {
    #if _PROFILE
    PROF_START();
    #endif

    Value a = LPOP();
    if (a.type != MSREST_INT)
        msrest_panic("Type error: SHL expects INTEGER type, got %s type\n", type_name(a.type));
    
    uint32_t sh = (uint32_t)ins.arg1 & 63u;
    Value result = int_to_value((int64_t)((uint64_t)a.integer << sh));
    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_60: {
    #if _PROFILE
    PROF_START(); // NATIVE_FUNC
    #endif
    
    NativeFunc fn = native_table[ins.arg1];
    if (!fn) 
        msrest_panic("Access violation error: Native function %d not loaded\n", ins.arg1);

    int argc = ins.arg2;
    if (__builtin_expect(argc < 0 || argc > 32, 0)) {
        msrest_panic("NATIVE_FUNC: argc %d out of range [0..32]\n", argc);
    }

    Value args[32];
    
    for (int i = argc - 1; i >= 0; i--) {
        args[i] = LPOP();
    }

    Value result = fn(&msrest_api, args, argc);

    /*
    printf("fn=%p\n", fn);
    printf("argc=%d\n", argc);
    printf("args=%p\n", args);
    */

    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_66: {
    #if _PROFILE
    PROF_START(); // INVOKE_STATIC
    #endif

    Value v = LPOP();
    LPUSH(invoke_static(ins.arg1, v));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_68: {
    #if _PROFILE
    PROF_START(); // HAS
    #endif

    Value obj = LPOP();      // искомый элемент.
    Value cont = LPOP();     // контейнер.

    uint8_t found = 0;

    switch (cont.type) {
        case MSREST_ARRAY: {
            Array *a = cont.array;
            uint32_t size = a->size;

            for (uint32_t i = 0; i < size; i++) {
                Value v = a->values[i];

                if (v.type == obj.type) {
                    switch (v.type) {
                        case MSREST_INT:
                            if (v.integer == obj.integer) found = 1;
                            break;
                        case MSREST_FLOAT:
                            if (v.floating == obj.floating) found = 1;
                            break;
                        case MSREST_BOOL:
                            if (v.logic == obj.logic) found = 1;
                            break;
                        case MSREST_NULL:
                            found = 1;
                            break;
                        default:
                            msrest_panic("Cannot use 'has' operator with %s type\n", type_name(v.type));
                    }
                }
                if (found) break;
            }
            break;
        }

        case MSREST_DICT: {
            if (obj.type != MSREST_STRING) {
                msrest_panic("'has' on dict expects STRING key, got %s", type_name(obj.type));
            }
            found = dict_has(cont, obj);
            break;
        }

        default:
            msrest_panic("Cannot use 'has' with %s type\n", type_name(cont.type));
    }

    LPUSH(bool_to_value(found));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_70: {
    #if _PROFILE
    PROF_START(); // ARR_OP
    #endif
    // TODO рефакторинг

    uint32_t op = ins.arg1;
    Value vb = LPOP();
    Value va = LPOP();
    char result_type = (va.type == MSREST_FLOAT_ARRAY || vb.type == MSREST_FLOAT_ARRAY) ? MSREST_FLOAT_ARRAY : MSREST_INT_ARRAY;
    uint32_t size_a = (va.type == MSREST_INT_ARRAY) ? va.int_array->size : va.float_array->size;
    uint32_t size_b = (vb.type == MSREST_INT_ARRAY) ? vb.int_array->size : vb.float_array->size;
    int min_size = (size_a < size_b) ? size_a : size_b;
    int max_size = (size_a > size_b) ? size_a : size_b;

    switch(op){
        case 1: { // ADD
            if(result_type == MSREST_FLOAT_ARRAY) {
                FloatArray *out = gc_alloc(sizeof(FloatArray), MSREST_FLOAT_ARRAY);
                out->size = max_size;
                out->capacity = (min_size > 8) ? min_size : 8;
                out->values = safe_malloc(sizeof(double) * out->capacity);
                for(uint32_t i = 0; i < min_size; i++) {
                    out->values[i] = farray_get(va, i) + farray_get(vb, i);
                }
                for(uint32_t i = min_size; i < max_size; i++) {
                    if(size_a > size_b)
                        out->values[i] = farray_get(va, i);
                    else
                        out->values[i] = farray_get(vb, i);
                }
                Value result;
                result.type = MSREST_FLOAT_ARRAY;
                result.float_array = out;
                LPUSH(result);
            }
            else {
                IntArray *out = intarray_new(min_size);
                out->size = max_size;
                for(uint_fast32_t i = 0; i < min_size; i++) {
                    out->values[i] = iarray_get(va, i) + iarray_get(vb, i);
                }
                for(uint_fast32_t i = min_size; i < max_size; i++) {
                    if(size_a > size_b)
                        out->values[i] = iarray_get(va, i);
                    else
                        out->values[i] = iarray_get(vb, i);
                }
                Value result;
                result.type = MSREST_INT_ARRAY;
                result.int_array = out;
                LPUSH(result);
            }
            break;
        }
        
        case 2: { // SUB
            if(result_type == MSREST_FLOAT_ARRAY) {
                FloatArray *out = gc_alloc(sizeof(FloatArray), MSREST_FLOAT_ARRAY);
                out->size = max_size;
                out->capacity = (min_size > 8) ? min_size : 8;
                out->values = safe_malloc(sizeof(double) * out->capacity);
                for(uint_fast32_t i = 0; i < min_size; i++) {
                    out->values[i] = farray_get(va, i) - farray_get(vb, i);
                }
                for(uint_fast32_t i = min_size; i < max_size; i++) {
                    if(size_a > size_b)
                        out->values[i] = farray_get(va, i);
                    else
                        out->values[i] = farray_get(vb, i);
                }
                Value result;
                result.type = MSREST_FLOAT_ARRAY;
                result.float_array = out;
                LPUSH(result);
            }
            else {
                IntArray *out = intarray_new(min_size);
                out->size = max_size;
                for(uint_fast32_t i = 0; i < min_size; i++) {
                    out->values[i] = iarray_get(va, i) - iarray_get(vb, i);
                }
                for(uint_fast32_t i = min_size; i < max_size; i++) {
                    if(size_a > size_b)
                        out->values[i] = iarray_get(va, i);
                    else
                        out->values[i] = iarray_get(vb, i);
                }
                Value result;
                result.type = MSREST_INT_ARRAY;
                result.int_array = out;
                LPUSH(result);
            }
            break;
        }
        
        case 3: { // MUL
            if(result_type == MSREST_FLOAT_ARRAY) {
                FloatArray *out = gc_alloc(sizeof(FloatArray), MSREST_FLOAT_ARRAY);
                out->size = max_size;
                out->capacity = (min_size > 8) ? min_size : 8;
                out->values = safe_malloc(sizeof(double) * out->capacity);
                for(uint_fast32_t i = 0; i < min_size; i++) {
                    out->values[i] = farray_get(va, i) * farray_get(vb, i);
                }
                for(uint_fast32_t i = min_size; i < max_size; i++) {
                    if(size_a > size_b)
                        out->values[i] = farray_get(va, i);
                    else
                        out->values[i] = farray_get(vb, i);
                }
                Value result;
                result.type = MSREST_FLOAT_ARRAY;
                result.float_array = out;
                LPUSH(result);
            }
            else {
                IntArray *out = intarray_new(min_size);
                out->size = max_size;
                for(uint_fast32_t i = 0; i < min_size; i++) {
                    out->values[i] = iarray_get(va, i) * iarray_get(vb, i);
                }
                for(uint_fast32_t i = min_size; i < max_size; i++) {
                    if(size_a > size_b)
                        out->values[i] = iarray_get(va, i);
                    else
                        out->values[i] = iarray_get(vb, i);
                }
                Value result;
                result.type = MSREST_INT_ARRAY;
                result.int_array = out;
                LPUSH(result);
            }
            break;
        }
    }

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_76: {

    #if _PROFILE
    PROF_START(); // DICT_NEW
    #endif

    Value dict = dict_new();
    uint32_t count = ins.arg1;

    for (uint32_t i = 0; i < count; i++) {
    Value key = LPOP();
    Value v = LPOP();
    dict_set(dict, key, v);
    }
    LPUSH(dict);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_77: {

    #if _PROFILE
    PROF_START(); // DICT_DEFGET
    #endif

    Value def = LPOP();
    Value key = LPOP();
    Value dict = LPOP();
    Value result;

    if (dict_has(dict, key)) {
        result = dict_get(dict, key);
    } else {
        result = def;
    }
    
    LPUSH(result);

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}




OP_79: {
    #if _PROFILE
    PROF_START(); // DICT_DELETE
    #endif

    Value key = LPOP();
    Value dict = LPOP();
    uint8_t deleted = dict_delete(dict, key);
    LPUSH(bool_to_value(deleted));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_7A: {
    #if _PROFILE
    PROF_START(); // DICT_METHODS
    #endif

    uint32_t method = ins.arg1;
    Value v = LPOP();
    Value res;

    switch (method) {
        case DICT_KEYS: res = dict_keys(v); break;
        case DICT_PAIRS: res = dict_pairs(v); break;
        case DICT_VALUES: res = dict_values(v); break;
        default: msrest_panic("Unknown static method id: %u\n", method);
    }
    LPUSH(res);
    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT(); 
}

OP_82: {
    #if _PROFILE
    PROF_START(); // LLEQ
    #endif

    Value a = vm->variables[ins.arg1];
    Value b = vm->variables[ins.arg2];
    LPUSH(bool_to_value(a.integer == b.integer));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_83: {
    #if _PROFILE
    PROF_START(); // LLLT
    #endif

    Value a = vm->variables[ins.arg1];
    Value b = vm->variables[ins.arg2];
    LPUSH(bool_to_value(a.integer < b.integer));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_85: {
    #if _PROFILE
    PROF_START(); // LLLE
    #endif

    Value a = vm->variables[ins.arg1];
    Value b = vm->variables[ins.arg2];
    LPUSH(bool_to_value(a.integer <= b.integer));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}


OP_A0: {
    #if _PROFILE
    PROF_START(); // RAND
    #endif

    int64_t min = value_to_int(LPOP());
    int64_t max = value_to_int(LPOP());
    
    int64_t value = min + rand() % (max - min + 1);
    LPUSH(int_to_value(value));

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_A1: {
    #if _PROFILE
    PROF_START(); // TYPEOF
    #endif

    uint32_t mode = ins.arg1;   // 0 = строка, 1 = числовой код ВМ
    Value v = LPOP();

    if (mode == 0)
        LPUSH(string_to_value(typeof_name(v.type)));
    else 
        LPUSH(int_to_value(v.type));
    

    #if _PROFILE
    PROF_END();
    #endif

    FETCH();
    NEXT();
}

OP_EE: {
    #if _PROFILE
    PROF_START(); // HALT
    #endif

    vm->ip = ip;
    vm->sp = sp;

    #if PROFILE

    fprintf(stderr, "FINAL sp=%d fp=%d fsp=%d\n", sp, vm->fp, vm->fsp);
    for (int i = 0; i < sp; i++) {
        Value v = stack[i];
        fprintf(stderr, "  stack[%d] type=%d", i, v.type);
        if (IS_NUMBER(v))
            fprintf(stderr, " int=%lld dbl=%.17g",
                    (long long)v.integer, value_to_double(v));
        else if (v.type == MSREST_STRING)
            fprintf(stderr, " str='%s'", v.string->data);
        fprintf(stderr, "\n");
    }

    # endif

        
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
        
        printf("\n===== TIME PROFILE =====\n");
        for (int i = 0; i < 256; i++) {
            if (opcode_counter[i]) {
                printf(
                    "OP=%02X hits=%llu cycles=%llu avg=%.2f\n",
                    i,
                    opcode_counter[i],
                    opcode_cycles[i],
                    (double)opcode_cycles[i] / opcode_counter[i]
                );
            }
        }
    #endif
    
    gc_collect();
    return 0;
}
    
OP_BB: {
    // HALT_WITH_CODE

    vm->sp = sp;
    vm->ip = ip;
    uint32_t exit_code = ins.arg1;
    return vm_halt(vm, code, code_start, exit_code, instruction_count);
}

}




/* < STATIC METHOD DISPATCH > */

static Value invoke_static_int(Value v)
{
    switch (v.type) {
        case MSREST_INT:   return v;
        case MSREST_FLOAT: return int_to_value((int64_t)v.floating);
        case MSREST_STRING: return int_to_value(atoll(v.string->data));
        case MSREST_NULL:  return int_to_value(0);
        case MSREST_BOOL:  return int_to_value(v.integer ? 1 : 0);
        default:
            msrest_panic("Cannot convert %s to int\n", type_name(v.type));
    }
}

static Value invoke_static_float(Value v)
{
    switch (v.type) {
        case MSREST_FLOAT:  return v;
        case MSREST_INT:    return float_to_value((double)v.integer);
        case MSREST_BOOL:   return float_to_value(v.integer ? 1.0 : 0.0);
        case MSREST_NULL:   return float_to_value(0.0);
        case MSREST_STRING: {
            char *endptr = NULL;
            double val = strtod(v.string->data, &endptr);
            if (endptr == v.string->data)
                msrest_panic("Cannot convert string to float: '%s'\n", v.string->data);
            return float_to_value(val);
        }
        default:
            msrest_panic("Cannot convert %s to float\n", type_name(v.type));
    }
}

static Value invoke_static_str(Value v)
{
    if (v.type == MSREST_STRING) return v;

    char *s = value_to_string(v);
    Value result = string_to_value(s);
    free(s);
    return result;
}


static Value invoke_static(uint32_t method, Value v)
{
    switch (method) {
        case TO_INT:   return invoke_static_int(v);
        case TO_FLOAT: return invoke_static_float(v);
        case TO_STR:   return invoke_static_str(v);
        case TO_BOOL:  return bool_to_value(value_to_bool(v));
        case STR_BYTELEN: return int_to_value(string_bytes(v));
        default:
            msrest_panic("Unknown static method id: %u\n", method);
    }
}