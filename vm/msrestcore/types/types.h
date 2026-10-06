#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include "../API/msrest_api.h"

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

struct IntArray {
    GCHeader gc;
    int size;
    int capacity;
    int64_t* values;
};

struct BoolArray {
    GCHeader gc;
    int size;
    int capacity;
    uint8_t* values;
};

struct FloatArray {
    GCHeader gc;
    int size;
    int capacity;
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