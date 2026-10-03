#include "getters_support.h"
#include "../runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
// саппорт-макрос.
#define XXH_ROTL64(x, r) (((x) << (r)) | ((x) >> (64 - (r))))


/* < GETTERS FUNCTIONS > */
/* There are functions that retrieve values ​​from arrays. */


Value array_get(Value obj, int index) 
{
    Array *arr = obj.array;
    if (index < 0 || index >= arr->size) {
        msrest_panic("Array index out of bounds: %d (size=%d)\n", index, arr->size);}
    return arr->values[index];
}

int64_t iarray_get(Value obj, int index) 
{
    IntArray *arr = obj.int_array;
    if (index < 0 || index >= arr->size) {
        msrest_panic("IntArray index out of bounds: %d (size=%d)\n", index, arr->size);}
    return arr->values[index];
}

double farray_get(Value obj, int index) 
{
    FloatArray *arr = obj.float_array;
    if (index < 0 || index >= arr->size) {
        msrest_panic("FloatArray index out of bounds: %d (size=%d)\n", index, arr->size);}
    return arr->values[index];
}

uint8_t barray_get(Value obj, int index) 
{
    BoolArray *arr = obj.bool_array;
    if (index < 0 || index >= arr->size) {
        msrest_panic("BoolArray index out of bounds: %d (size=%d)\n", index, arr->size);}
    return arr->values[index];
}


uint32_t array_size(Value v)
{
    switch (v.type)
    {
        case MSREST_ARRAY: {
            return v.array->size;
        }
        case MSREST_INT_ARRAY: {
            return v.int_array->size;
        }
        case MSREST_FLOAT_ARRAY: {
            return v.float_array->size;
        }
        case MSREST_BOOL_ARRAY: {
            return v.bool_array->size;
        }
        default: {
            msrest_panic("Expected array");
        }
    }
}


uint32_t string_chars(Value v) {
    if (v.type != MSREST_STRING)
        msrest_panic("Expected string");
    return v.string->chars;  // возвращаем кодовые точки (буквы, а не байты по факту).
}

uint8_t string_empty(Value v)
{
    return string_bytes(v) == 0;
}


uint32_t string_bytes(Value v)
{
    if (v.type != MSREST_STRING)
        msrest_panic("Expected string");
    return v.string->length;
}

uint32_t count_utf8_chars(const uint8_t *data, size_t bytes)
{
    uint32_t chars = 0;
    
    for (size_t i = 0; i < bytes; i++) {
        if ((data[i] & 0x80) == 0) {
            chars++;
        } else if ((data[i] & 0xE0) == 0xC0) {
            chars++;
            i += 1;
        } else if ((data[i] & 0xF0) == 0xE0) {
            chars++;
            i += 2;
        } else if ((data[i] & 0xF8) == 0xF0) {
            chars++;
            i += 3;
        }
    }
    
    return chars;
}

const char *string_data(Value v)
{
    if (v.type != MSREST_STRING)
        msrest_panic("Expected string");
    return v.string->data;
}



// ЭТО ХЭШИРОВАНИЕ
// сколько я на него потратил времени.
uint64_t xxhash3_64(const void* data, size_t len) {
    const uint64_t prime1 = 0x9E3779B185EBCA87ULL;
    const uint64_t prime2 = 0xC2B2AE3D27D4EB4FULL;
    const uint64_t prime3 = 0x165667B19E3779F9ULL;
    const uint64_t prime4 = 0x85EBCA77C2B2AE63ULL;
    const uint64_t prime5 = 0x27D4EB2F165667C5ULL;

    const uint8_t* p = (const uint8_t*)data;
    const uint8_t* const end = p + len;
    uint64_t h64;

    if (len >= 32) {
        const uint8_t* const limit = end - 32;
        uint64_t v1 = prime1 + prime2;
        uint64_t v2 = prime2;
        uint64_t v3 = 0;
        uint64_t v4 = -prime1;

        do {
            // обработка 4 блоков по 8 байт (32 байта за итерацию), может gcc раскидает по SIMD.
            uint64_t k1 = *(const uint64_t*)p;
            uint64_t k2 = *(const uint64_t*)(p + 8);
            uint64_t k3 = *(const uint64_t*)(p + 16);
            uint64_t k4 = *(const uint64_t*)(p + 24);

            k1 *= prime2;
            k1 = XXH_ROTL64(k1, 31);
            k1 *= prime1;
            v1 ^= k1;
            v1 = XXH_ROTL64(v1, 27);
            v1 += v2;
            v1 = v1 * 5 + prime3;

            k2 *= prime2;
            k2 = XXH_ROTL64(k2, 31);
            k2 *= prime1;
            v2 ^= k2;
            v2 = XXH_ROTL64(v2, 27);
            v2 += v3;
            v2 = v2 * 5 + prime3;

            k3 *= prime2;
            k3 = XXH_ROTL64(k3, 31);
            k3 *= prime1;
            v3 ^= k3;
            v3 = XXH_ROTL64(v3, 27);
            v3 += v4;
            v3 = v3 * 5 + prime3;

            k4 *= prime2;
            k4 = XXH_ROTL64(k4, 31);
            k4 *= prime1;
            v4 ^= k4;
            v4 = XXH_ROTL64(v4, 27);
            v4 += v1;
            v4 = v4 * 5 + prime3;

            p += 32;
        } while (p <= limit);

        h64 = XXH_ROTL64(v1, 1) + XXH_ROTL64(v2, 7) + 
              XXH_ROTL64(v3, 12) + XXH_ROTL64(v4, 18);

        // объединяем каналы.
        v1 *= prime2;
        v1 = XXH_ROTL64(v1, 31);
        v1 *= prime1;
        h64 ^= v1;
        h64 = h64 * prime1 + prime4;

        v2 *= prime2;
        v2 = XXH_ROTL64(v2, 31);
        v2 *= prime1;
        h64 ^= v2;
        h64 = h64 * prime1 + prime4;

        v3 *= prime2;
        v3 = XXH_ROTL64(v3, 31);
        v3 *= prime1;
        h64 ^= v3;
        h64 = h64 * prime1 + prime4;

        v4 *= prime2;
        v4 = XXH_ROTL64(v4, 31);
        v4 *= prime1;
        h64 ^= v4;
        h64 = h64 * prime1 + prime4;

        h64 += len;
    } else {
        h64 = prime5 + len;
    }

    // обрабатываем оставшиеся байты.
    while (p + 8 <= end) {
        uint64_t k = *(const uint64_t*)p;
        k *= prime2;
        k = XXH_ROTL64(k, 31);
        k *= prime1;
        h64 ^= k;
        h64 = XXH_ROTL64(h64, 27) * prime1 + prime4;
        p += 8;
    }

    if (p + 4 <= end) {
        uint32_t k = *(const uint32_t*)p;
        k *= prime3;
        h64 ^= k;
        h64 = XXH_ROTL64(h64, 23) * prime2 + prime3;
        p += 4;
    }

    while (p < end) {
        h64 ^= (*p) * prime5;
        h64 = XXH_ROTL64(h64, 11) * prime1;
        p++;
    }

    h64 ^= h64 >> 33;
    h64 *= prime2;
    h64 ^= h64 >> 29;
    h64 *= prime3;
    h64 ^= h64 >> 32;

    return h64;
}


uint64_t hash_value(Value val) {
    // жадное хэширование.
    if (val.type != MSREST_STRING || val.string == NULL) return 0;

    String* str = val.string;
    
    // если хэш уже вычислен - возвращаем кэш.
    if (str->hash != 0) return str->hash;
    
    // вычисляем хэш и кэшируем в строку.
    str->hash = xxhash3_64(str->data, str->length);
    return str->hash;
}