/* << MSREST_API.H >> */

#ifndef MSREST_API_H
#define MSREST_API_H

/* < INCLUDE FILES > */

#include <stdint.h>
#include <stdlib.h>


/* Версия ABI. Библиотеки и VM
   должны иметь одинаковую версию, иначе VM откажется грузить DLL. */
#define MSREST_API_ABI_VERSION 2

enum {
    MSREST_NULL = 0,
    MSREST_INT,
    MSREST_ARRAY,
    MSREST_STRING,
    MSREST_FLOAT,
    MSREST_DICT,
    MSREST_INT_ARRAY,
    MSREST_FLOAT_ARRAY,
    MSREST_BOOL_ARRAY,
    MSREST_BOOL,
    MSREST_NATIVE,
    MSREST_FUNCREF
};

static const char* type_names[] = {
    [MSREST_NULL]        = "NULL",
    [MSREST_INT]         = "INT",
    [MSREST_FLOAT]       = "FLOAT",
    [MSREST_BOOL]        = "BOOL",
    [MSREST_STRING]      = "STRING",
    [MSREST_ARRAY]       = "ARRAY",
    [MSREST_INT_ARRAY]   = "INT_ARRAY",
    [MSREST_FLOAT_ARRAY] = "FLOAT_ARRAY",
    [MSREST_BOOL_ARRAY]  = "BOOL_ARRAY",
    [MSREST_DICT]        = "DICT",
    [MSREST_NATIVE]      = "NATIVE",
    [MSREST_FUNCREF]     = "FUNCREF",
};

/* инлайн-штуко, которая будет для ошибок */
static inline const char* type_name(uint8_t type) {
    if (type < sizeof(type_names)/sizeof(type_names[0]) && type_names[type])
        return type_names[type];
    return "UNKNOWN";
}


#define IS_NUMBER(v) ((v).type == MSREST_INT || (v).type == MSREST_FLOAT)
#define IS_NULL(v) ((v).type == MSREST_NULL)

/* < TYPEDEF > */
typedef struct Array Array;
typedef struct String String;
typedef struct IntArray IntArray;
typedef struct FloatArray FloatArray;
typedef struct BoolArray BoolArray;
typedef struct Value Value;
typedef struct GCHeader GCHeader;
typedef struct NativeObject NativeObject;
typedef struct Dict Dict;

/* < VALUE DECLARATION > */
/* The only main Mashrest Value declaration. */
struct Value {
    uint8_t type;
    union {
        int64_t integer;
        double floating;
        Array *array;
        String *string;
        Dict *dictionary;
        IntArray *int_array;
        FloatArray *float_array;
        BoolArray *bool_array;
        uint8_t logic;
        NativeObject *native;
        struct {
            uint32_t addr;
            uint16_t arity;
        } fnref;   
    };
};


// макрос для удобства.
#define check_argc(argc, required) \
    check_argc(__func__, argc, required)


/* macro for EXPORT */
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif


/* Mashrest API export functions declaration */
typedef struct {
    uint32_t abi_version;

    Value (*string_to_value)(const char *);
    Value (*int_to_value)(intptr_t);
    Value (*float_to_value)(double);
    Value (*bool_to_value)(uint8_t);
    Value (*null_to_value)(void);

    intptr_t (*value_to_int)(Value);
    double (*value_to_double)(Value);
    uint8_t (*value_to_bool)(Value);
    char *(*value_to_string)(Value v);


    void (*msrest_panic)(const char *, ...);
    void (*msrest_msg)(const char *fmt, ...);


    uint32_t (*array_size)(Value);
    Value (*array_get)(Value, uint32_t);
    void (*array_set)(Value, uint32_t, Value);
    void (*array_push)(Value, Value);
    Value (*array_pop)(Value);
    Value (*array_new)(uint32_t capacity);
    int (*array_index_check)(Value, uint32_t);


    uint32_t (*string_bytes)(Value);
    uint32_t (*string_chars)(Value);
    const char *(*string_data)(Value);
    uint8_t (*string_empty)(Value);

    Value (*string_new)(const char *str);
    Value (*string_new_len)(const char *str, size_t len);


    Value (*native_new)(const char *, void *, void (*)(void *));
    void *(*native_payload)(Value, const char *);

    void (*check_argc)(const char *func_name, uint16_t argc, uint16_t required);
    uint8_t (*soft_check_type)(uint8_t required, uint8_t got);
    uint8_t (*value_type)(Value);
    void (*lib_check_type)(uint8_t required, uint8_t got, const char *lib_name, const char *funcname);


    Value (*dict_new)(void);
    void (*dict_set)(Value dict_val, Value key_val, Value value);
    Value (*dict_get)(Value dict_val, Value key_val);
    uint8_t (*dict_has)(Value dict_val, Value key_val);
    uint8_t (*dict_delete)(Value dict_val, Value key_val);
    uint32_t (*dict_size)(Value dict_val);
    Value (*dict_keys)(Value dict_val);

} MsrestAPI;

typedef Value (*NativeFunc)(MsrestAPI *api, Value *args, int argc);

#endif