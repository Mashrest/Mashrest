#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include "../API/msrest_api.h"

static const char* type_names[] = {
    [MSREST_NULL]        = "NULL",
    [MSREST_INT]         = "INT",
    [MSREST_FLOAT]       = "FLOAT",
    [MSREST_BOOL]        = "BOOL",
    [MSREST_STRING]      = "STRING",
    [MSREST_ARRAY]       = "ARRAY",
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

struct GCHeader {
    struct GCHeader *next;
    uint8_t color;
    uint8_t type;
};

struct NativeObject {
    GCHeader gc;
    const char *type;
    void *payload;
    void (*destroy)(void *);
};

struct String {
    GCHeader gc;
    uint64_t hash;
    uint32_t length;
    uint32_t chars;
    char *data;
};

struct Array {
    GCHeader gc;
    uint32_t size;
    uint32_t capacity;
    Value *values;
};

struct BoolArray {
    GCHeader gc;
    uint32_t size;
    uint32_t capacity;
    uint8_t* values;
};

struct FloatArray {
    GCHeader gc;
    uint32_t size;
    uint32_t capacity;
    double *values;
};

typedef struct DictEntry {
    String *key;
    Value value;
    struct DictEntry* next;
} DictEntry;

typedef struct Dict {
    GCHeader gc; 
    uint32_t capacity;  
    uint32_t size;             
    DictEntry** buckets;
} Dict;


#endif