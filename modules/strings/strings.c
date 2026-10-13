/* << DLL strings >> */

/* < INCLUDE FILES > */
#include <stdio.h>
#include "API/msrest_api.h"
#include <direct.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define LIB_NAME "STRINGS"


static inline void lib_check_type(MsrestAPI *api, uint8_t required, uint8_t got, const char *funcname)
{
    if (required != got) {
        api->msrest_panic("[" LIB_NAME "] Type error: %s() Required: %s, Got: %s\n", 
            funcname, type_names[required], type_names[got]);
    }
}


// UTF-8 утилиты.

// принимает сырые байты, декодирует в нормальное число.
static uint32_t utf8_decode(const uint8_t* data) {
    uint8_t c = data[0];
    // если <128, то ASCII, просто скип и возврат.
    if (c < 0x80) return c;
    // если первый байт начинается на 110, значит лидер двухбайтового.
    if ((c & 0xE0) == 0xC0) {
        if ((data[1] & 0xC0) != 0x80) return 0xFFFD;
        return ((c & 0x1F) << 6) | (data[1] & 0x3F);
    }
    // 1110 - 3 байта.
    if ((c & 0xF0) == 0xE0) {
        if ((data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80) return 0xFFFD;
        return ((c & 0x0F) << 12) | ((data[1] & 0x3F) << 6) | (data[2] & 0x3F);
    }
    // 4 байта.
    if ((c & 0xF8) == 0xF0) {
        if ((data[1] & 0xC0) != 0x80 || (data[2] & 0xC0) != 0x80 || (data[3] & 0xC0) != 0x80)
            return 0xFFFD;
        return ((c & 0x07) << 18) | ((data[1] & 0x3F) << 12) |
               ((data[2] & 0x3F) << 6) | (data[3] & 0x3F);
    }
    // битый юникод, как по стандарту.
    return 0xFFFD;
}

// просто считает длину руны по байту.
static inline size_t utf8_char_bytes(const char* str, size_t pos) {
    uint8_t c = (uint8_t)str[pos];
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}


/*
0xxxxxxx - лидер однобайтового символа (ASCII)
110xxxxx -лидер 2-байтового
1110xxxx - лидер 3-байтового
11110xxx - лидер 4-байтового
10xxxxxx - байт-продолжение.
*/
// кодируем в буфер, аппендим.
static size_t utf8_encode(char* buf, uint32_t cp) {
    if (cp < 0x80) {
        buf[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        buf[0] = 0xC0 | (cp >> 6);
        buf[1] = 0x80 | (cp & 0x3F);
        return 2;
    }
    if (cp < 0x10000) {
        buf[0] = 0xE0 | (cp >> 12);
        buf[1] = 0x80 | ((cp >> 6) & 0x3F);
        buf[2] = 0x80 | (cp & 0x3F);
        return 3;
    }
    buf[0] = 0xF0 | (cp >> 18);
    buf[1] = 0x80 | ((cp >> 12) & 0x3F);
    buf[2] = 0x80 | ((cp >> 6) & 0x3F);
    buf[3] = 0x80 | (cp & 0x3F);
    return 4;
}

// принимает количество символов, которое нужно декодировать, указатель на первый байт строки.
static uint32_t* decode_to_codepoints(const char* str, size_t char_len, size_t* out_byte_len) {
    uint32_t* cp = malloc(char_len * sizeof(uint32_t));
    if (!cp) return NULL;
    
    size_t byte_pos = 0;
    for (int64_t i = 0; i < char_len; i++) {
        cp[i] = utf8_decode((uint8_t*)str + byte_pos);
        byte_pos += utf8_char_bytes(str, byte_pos);
    }
    // может быть NULL. О
    if (out_byte_len) *out_byte_len = byte_pos;
    return cp;
}

static char* encode_codepoints(const uint32_t* cp, size_t char_len, size_t* out_byte_len) {
    char_len = (int64_t)char_len;
    
    size_t total = 0;
    for (int64_t i = 0; i < char_len; i++) {
        if (cp[i] < 0x80) total += 1;
        else if (cp[i] < 0x800) total += 2;
        else if (cp[i] < 0x10000) total += 3;
        else total += 4;
    }
    
    char* buf = malloc(total + 1);
    if (!buf) return NULL;
    
    size_t pos = 0;
    for (int64_t i = 0; i < char_len; i++) {
        pos += utf8_encode(buf + pos, cp[i]);
    }
    buf[pos] = '\0';
    if (out_byte_len) *out_byte_len = pos;
    return buf;
}

// проверки символов.

static int is_unicode_space(uint32_t cp) {
    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\v' || cp == '\f') return 1;
    if (cp == 0x00A0 || cp == 0x1680 || cp == 0x2028 || cp == 0x2029 || 
        cp == 0x202F || cp == 0x205F || cp == 0x3000) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    return 0;
}

static uint32_t require_char(MsrestAPI* api, Value* args, int argc, const char* func) {
    if (argc != 1) api->msrest_panic("[STRINGS] %s() expects 1 argument, got %d", func, argc);
    lib_check_type(api, MSREST_STRING, args[0].type, func);
    
    const char* str = api->string_data(args[0]);
    if (api->string_chars(args[0]) != 1) {
        api->msrest_panic("[" LIB_NAME "] %s() expects a string of length 1", func);
    }
    return utf8_decode((uint8_t*)str);
}

// общая функция для upper/lower.

static Value change_case(MsrestAPI* api, Value* args, int argc, int to_upper) {
    if (argc != 1) api->msrest_panic("[" LIB_NAME "] %s() expects 1 argument, got %d", 
                                     to_upper ? "upper" : "lower", argc);
    lib_check_type(api, MSREST_STRING, args[0].type, to_upper ? "upper" : "lower");
    
    const char* str = api->string_data(args[0]);
    
    size_t char_len = (size_t)api->string_chars(args[0]);
    if (char_len == 0) return api->string_new_len("", 0);
    
    size_t byte_len;
    uint32_t* cp = decode_to_codepoints(str, char_len, &byte_len);
    if (!cp) api->msrest_panic("[" LIB_NAME "] %s() malloc failed", to_upper ? "upper" : "lower");
    
    for (size_t i = 0; i < char_len; i++) {
        uint32_t c = cp[i];
        if (to_upper) {
            if (c >= 'a' && c <= 'z') cp[i] = c - 32;
            else if (c >= 0x0430 && c <= 0x044F) cp[i] = c - 32;
            else if (c == 0x0451) cp[i] = 0x0401;
        } else {
            if (c >= 'A' && c <= 'Z') cp[i] = c + 32;
            else if (c >= 0x0410 && c <= 0x042F) cp[i] = c + 32;
            else if (c == 0x0401) cp[i] = 0x0451;
        }
    }
    
    size_t new_byte_len;
    char* buf = encode_codepoints(cp, char_len, &new_byte_len);
    free(cp);
    if (!buf) api->msrest_panic("[" LIB_NAME "] %s() malloc failed", to_upper ? "upper" : "lower");
    
    Value result = api->string_new_len(buf, new_byte_len);
    free(buf);
    return result;
}

// is_* проверки.

static int is_digit_char(uint32_t cp) {
    return (cp >= '0' && cp <= '9');
}

static int is_alpha_char(uint32_t cp) {
    if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z')) return 1;
    if ((cp >= 0x0410 && cp <= 0x042F) || (cp >= 0x0430 && cp <= 0x044F)) return 1;
    if (cp == 0x0401 || cp == 0x0451) return 1;
    return 0;
}

static int is_alnum_char(uint32_t cp) {
    return is_digit_char(cp) || is_alpha_char(cp);
}

static int is_space_char(uint32_t cp) {
    return is_unicode_space(cp);
}

static int is_upper_char(uint32_t cp) {
    if (cp >= 'A' && cp <= 'Z') return 1;
    if (cp >= 0x0410 && cp <= 0x042F) return 1;
    if (cp == 0x0401) return 1;
    return 0;
}

static int is_lower_char(uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return 1;
    if (cp >= 0x0430 && cp <= 0x044F) return 1;
    if (cp == 0x0451) return 1;
    return 0;
}

static int is_hex_char(uint32_t cp) {
    if (cp >= '0' && cp <= '9') return 1;
    if ((cp >= 'A' && cp <= 'F') || (cp >= 'a' && cp <= 'f')) return 1;
    return 0;
}



/* вспомогательные функции для управления буфером. */

static void* safe_realloc(MsrestAPI *api, void* ptr, size_t size) {
    void* p = realloc(ptr, size);
    if (!p) api->msrest_panic("[" LIB_NAME "] memory reallocation failed");
    return p;
}

static void ensure_capacity(MsrestAPI* api, char** buffer, size_t* capacity, size_t needed) {
    if (needed < *capacity) return;
    size_t nc = *capacity ? *capacity : 64;
    while (nc <= needed) {
        if (nc > SIZE_MAX / 2) { nc = needed + 1; break; }
        nc *= 2;
    }
    *buffer = safe_realloc(api, *buffer, nc);
    *capacity = nc;
}

static char* safe_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* result = malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, s, len + 1);
    return result;
}

static void* safe_malloc(MsrestAPI *api, size_t size) {
    void* p = malloc(size);
    if (!p) api->msrest_panic("[" LIB_NAME "] memory allocation failed");
    return p;
}

static inline int is_ascii_space(uint8_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}




// эскейп-анэсейкп функции-саппорты.

static int hex_val(uint8_t c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void append_bytes(MsrestAPI *api, char **buf, size_t *cap, size_t *len,
                         const char *src, size_t n) {
    ensure_capacity(api, buf, cap, *len + n);
    memcpy(*buf + *len, src, n);
    *len += n;
}

static void append_cp(MsrestAPI *api, char **buf, size_t *cap, size_t *len,
                      uint32_t cp) {
    char tmp[4];
    size_t n = utf8_encode(tmp, cp);
    append_bytes(api, buf, cap, len, tmp, n);
}




// EXPORT функции.


EXPORT Value length(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 1);
    return api->int_to_value(api->string_chars(args[0]));
}

EXPORT Value empty(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 1);
    return api->bool_to_value(api->string_empty(args[0]));
}

EXPORT Value upper(MsrestAPI* api, Value* args, int argc) {
    return change_case(api, args, argc, 1);
}

EXPORT Value lower(MsrestAPI* api, Value* args, int argc) {
    return change_case(api, args, argc, 0);
}

EXPORT Value ord(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 1);
    lib_check_type(api, MSREST_STRING, args[0].type, "ord");
    
    const char* str = api->string_data(args[0]);
    size_t char_len = (size_t)api->string_chars(args[0]);
    
    if (char_len == 0) api->msrest_panic("[" LIB_NAME "] ord() cannot convert an empty string\n");
    
    if (char_len == 1) {
        return api->int_to_value(utf8_decode((uint8_t*)str));
    }
    
    Value arr = api->array_new(char_len);
    size_t pos = 0;
    for (size_t i = 0; i < char_len; i++) {
        uint32_t cp = utf8_decode((uint8_t*)str + pos);
        api->array_set(arr, i, api->int_to_value(cp));
        pos += utf8_char_bytes(str, pos);
    }
    return arr;
}

EXPORT Value chr(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 1);
    uint8_t type = api->value_type(args[0]);
    
    if (type == MSREST_INT) {
        int32_t cp = (int32_t)api->value_to_int(args[0]);
        if (cp < 0 || cp > 0x10FFFF) {
            api->msrest_panic("[" LIB_NAME "] chr() code point out of range (0..0x10FFFF)");
        }
        char buf[5];
        size_t len = utf8_encode(buf, (uint32_t)cp);
        buf[len] = '\0';
        return api->string_new_len(buf, len);
    }
    
    if (type == MSREST_ARRAY) {
        uint32_t len = api->array_size(args[0]);
        uint32_t* cp = safe_malloc(api, len * sizeof(uint32_t));
        
        for (int i = 0; i < len; i++) {
            Value v = api->array_get(args[0], i);
            if (api->value_type(v) != MSREST_INT) {
                free(cp);
                api->msrest_panic("[" LIB_NAME "] chr() array must contain integers");
            }
            cp[i] = (uint32_t)api->value_to_int(v);
            if (cp[i] > 0x10FFFF) {
                free(cp);
                api->msrest_panic("[" LIB_NAME "] chr() code point out of range");
            }
        }
        
        size_t byte_len;
        char* buf = encode_codepoints(cp, len, &byte_len);
        free(cp);
        if (!buf) api->msrest_panic("[" LIB_NAME "] chr(): out of memory");
        
        Value result = api->string_new_len(buf, byte_len);
        free(buf);
        return result;
    }
    
    api->msrest_panic("[" LIB_NAME "] chr() expects an integer or array");
}

EXPORT Value contains(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 2);
    lib_check_type(api, MSREST_STRING, args[0].type, "contains");
    lib_check_type(api, MSREST_STRING, args[1].type, "contains");
    
    const char* str = api->string_data(args[0]);
    const char* sub = api->string_data(args[1]);
    return api->bool_to_value(strstr(str, sub) != NULL);
}

EXPORT Value find(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 2);
    lib_check_type(api, MSREST_STRING, args[0].type, "find");
    lib_check_type(api, MSREST_STRING, args[1].type, "find");
    
    const char* str = api->string_data(args[0]);
    const char* sub = api->string_data(args[1]);
    
    char* p = strstr(str, sub);
    if (!p) return api->int_to_value(-1);
    
    size_t byte_pos = (size_t)(p - str);
    size_t char_pos = 0;
    size_t pos = 0;
    while (pos < byte_pos) {
        pos += utf8_char_bytes(str, pos);
        char_pos++;
    }
    return api->int_to_value((int)char_pos);
}


EXPORT Value trim(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 1);
    lib_check_type(api, MSREST_STRING, args[0].type, "trim");

    const char* str = api->string_data(args[0]);

    size_t byte_len = api->string_bytes(args[0]);
    if (byte_len == 0) return api->string_new_len("", 0);

    size_t byte_start = 0;
    size_t byte_end   = byte_len;

    // FAST PATH, слева пробелы.
    while (byte_start < byte_end) {
        uint8_t c = (uint8_t)str[byte_start];
        if (c >= 0x80) goto unicode_left;
        if (!is_ascii_space(c)) break;
        byte_start++;
    }

    // FAST PATH, справа пробелы.
    while (byte_end > byte_start) {
        uint8_t c = (uint8_t)str[byte_end - 1];
        if (c >= 0x80) goto unicode_right;
        if (!is_ascii_space(c)) break;
        byte_end--;
    }

    goto done;

unicode_left:
    // продолжаем слева с юникодом.
    while (byte_start < byte_end) {
        uint32_t cp = utf8_decode((const uint8_t*)str + byte_start);
        if (!is_unicode_space(cp)) break;
        byte_start += utf8_char_bytes(str, byte_start);
    }

unicode_right:
    // продолжаем справа с юникодом.
    while (byte_end > byte_start) {
        size_t prev = byte_end - 1;
        while (prev > byte_start && ((uint8_t)str[prev] & 0xC0) == 0x80) prev--;

        uint32_t cp = utf8_decode((const uint8_t*)str + prev);
        if (!is_unicode_space(cp)) break;
        byte_end = prev;
    }

done:
    size_t result_len = byte_end - byte_start;
    char* result = safe_malloc(api, result_len + 1);
    memcpy(result, str + byte_start, result_len);
    result[result_len] = '\0';

    Value v = api->string_new_len(result, result_len);
    free(result);
    return v;
}

EXPORT Value is_digit(MsrestAPI* api, Value* args, int argc) {
    uint32_t cp = require_char(api, args, argc, "is_digit");
    return api->bool_to_value(is_digit_char(cp));
}

EXPORT Value is_alpha(MsrestAPI* api, Value* args, int argc) {
    uint32_t cp = require_char(api, args, argc, "is_alpha");
    return api->bool_to_value(is_alpha_char(cp));
}

EXPORT Value is_alnum(MsrestAPI* api, Value* args, int argc) {
    uint32_t cp = require_char(api, args, argc, "is_alnum");
    return api->bool_to_value(is_alnum_char(cp));
}

EXPORT Value is_space(MsrestAPI* api, Value* args, int argc) {
    uint32_t cp = require_char(api, args, argc, "is_space");
    return api->bool_to_value(is_space_char(cp));
}

EXPORT Value is_upper(MsrestAPI* api, Value* args, int argc) {
    uint32_t cp = require_char(api, args, argc, "is_upper");
    return api->bool_to_value(is_upper_char(cp));
}

EXPORT Value is_lower(MsrestAPI* api, Value* args, int argc) {
    uint32_t cp = require_char(api, args, argc, "is_lower");
    return api->bool_to_value(is_lower_char(cp));
}

EXPORT Value is_hex(MsrestAPI* api, Value* args, int argc) {
    uint32_t cp = require_char(api, args, argc, "is_hex");
    return api->bool_to_value(is_hex_char(cp));
}

EXPORT Value substr(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 3);
    lib_check_type(api, MSREST_STRING, args[0].type, "substr");
    lib_check_type(api, MSREST_INT, args[1].type, "substr");
    lib_check_type(api, MSREST_INT, args[2].type, "substr");
    
    const char* str = api->string_data(args[0]);
    size_t byte_len = strlen(str);
    size_t char_len = (size_t)api->string_chars(args[0]);
    
    int start_char = api->value_to_int(args[1]);
    int len_char = api->value_to_int(args[2]);
    
    if (start_char < 0 || start_char > (int)char_len) {
        api->msrest_panic("[" LIB_NAME "] substr(): start out of range");
    }
    
    // -1 (или любое отрицательное) - до конца строки.
    if (len_char < 0) {
        len_char = (int)char_len - start_char;
    }
    
    if (start_char + len_char > (int)char_len) {
        len_char = (int)char_len - start_char;
    }
    
    size_t byte_start = 0;
    for (int i = 0; i < start_char && byte_start < byte_len; i++) {
        byte_start += utf8_char_bytes(str, byte_start);
    }
    
    size_t byte_end = byte_start;
    for (int i = 0; i < len_char && byte_end < byte_len; i++) {
        byte_end += utf8_char_bytes(str, byte_end);
    }
    
    size_t result_len = byte_end - byte_start;
    char* result = safe_malloc(api, result_len + 1);
    
    memcpy(result, str + byte_start, result_len);
    result[result_len] = '\0';
    
    Value v = api->string_new_len(result, result_len);
    free(result);
    return v;
}

/* основная функция форматирования */
EXPORT Value format(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 2);
    lib_check_type(api, MSREST_STRING, args[0].type, "format");
    lib_check_type(api, MSREST_DICT, args[1].type, "format");
    
    const char* template = api->string_data(args[0]);
    size_t template_len = api->string_bytes(args[0]);
    Value dict = args[1];
    
    if (template_len == 0) return api->string_new_len("", 0);
    
    size_t cap = 1024;
    size_t len = 0;
    char* result = safe_malloc(api, cap);
    
    const char* p = template;
    const char* end = template + template_len;
    
    while (p < end) {
        if (*p == '{') {
            
            if (p + 1 < end && *(p + 1) == '{') {
                ensure_capacity(api, &result, &cap, len + 1);
                result[len++] = '{';
                p += 2;
                continue;
            }
            
            const char* key_start = p + 1;
            const char* key_end = key_start;
            
            while (key_end < end && *key_end != '}') key_end++;

            if (key_end == end) {
                free(result);
                api->msrest_panic("[" LIB_NAME "] format(): unclosed brace");
            }
            
            size_t full_len = key_end - key_start;
            char* full_cstr = safe_malloc(api, full_len + 1);
            memcpy(full_cstr, key_start, full_len);
            full_cstr[full_len] = '\0';
            
            char* dot = strchr(full_cstr, '.');
            char* key_cstr;
            int precision = -1;
            
            if (dot) {
                size_t key_len = dot - full_cstr;
                key_cstr = safe_malloc(api, key_len + 1);
                memcpy(key_cstr, full_cstr, key_len);
                key_cstr[key_len] = '\0';
                
                const char* prec_str = dot + 1;
                if (*prec_str == '\0') {
                    free(full_cstr);
                    free(key_cstr);
                    free(result);
                    api->msrest_panic("[" LIB_NAME "] format(): expected precision after '.'");
                }
                
                char* endptr;
                precision = (int)strtol(prec_str, &endptr, 10);
                if (*endptr != '\0' || precision < 0) {
                    free(full_cstr);
                    free(key_cstr);
                    free(result);
                    api->msrest_panic("[" LIB_NAME "] format(): invalid precision '%s'", prec_str);
                }
                
                free(full_cstr);
            } else {
                key_cstr = full_cstr;
            }
            
            size_t key_len = strlen(key_cstr);
            Value key_val = api->string_new_len(key_cstr, key_len);
            
            if (!api->dict_has(dict, key_val)) {
                free(result);
                api->msrest_panic("[" LIB_NAME "] format(): key '%s' not found", key_cstr);
                free(key_cstr);
            }

            Value val = api->dict_get(dict, key_val);
            uint8_t val_type = api->value_type(val);
            
            char* val_str;
            if (precision >= 0 && val_type == MSREST_FLOAT) {
                double num = api->value_to_double(val);
                char buf[128];
                if (precision == 0) {
                    snprintf(buf, sizeof(buf), "%.0f", num);
                } else {
                    snprintf(buf, sizeof(buf), "%.*f", precision, num);
                }
                val_str = safe_strdup(buf);
            } else {
                val_str = api->value_to_string(val);
            }
            
            size_t val_len = strlen(val_str);
            ensure_capacity(api, &result, &cap, len + val_len);
            memcpy(result + len, val_str, val_len);
            len += val_len;
            
            free(val_str);
            free(key_cstr);
            p = key_end + 1;
            
        } else if (*p == '}') {
            if (p + 1 < end && *(p + 1) == '}') {
                ensure_capacity(api, &result, &cap, len + 1);
                result[len++] = '}';
                p += 2;
                continue;
            }
            free(result);
            api->msrest_panic("[" LIB_NAME "] format(): unexpected '}'");
        } else {
            const char* next_open  = memchr(p, '{', end - p);
            const char* next_close = memchr(p, '}', end - p);
            const char* stop = next_open;
            if (!stop || (next_close && next_close < stop)) stop = next_close;
            if (!stop) stop = end;

            size_t chunk = stop - p;
            ensure_capacity(api, &result, &cap, len + chunk);
            memcpy(result + len, p, chunk);
            len += chunk;
            p = stop;
        }
    }
    
    result[len] = '\0';
    Value result_val = api->string_new_len(result, len);
    free(result);
    
    return result_val;
}


EXPORT Value hex_to_decimal(MsrestAPI* api, Value* args, int argc) 
{
    api->check_argc(argc, 1);
    lib_check_type(api, MSREST_STRING, args[0].type, "hex_to_decimal");

    const char *start_hex = api->string_data(args[0]);
    uint32_t string_len = api->string_bytes(args[0]);

    if (string_len == 0 || string_len > 16) {
        api->msrest_panic("[" LIB_NAME "] hex_to_decimal(): string must be 1..16 hex chars, got %d", string_len);
    }

    for (uint32_t i = 0; i < string_len; i++) {
        if (!isxdigit((unsigned char)start_hex[i])) {
            api->msrest_panic("[" LIB_NAME "] hex_to_decimal(): cannot convert non-hex number to decimal");
        }
    }

    errno = 0;
    unsigned long long u = strtoull(start_hex, NULL, 16);
    if (errno == ERANGE) {
        api->msrest_panic("[" LIB_NAME "] hex_to_decimal(): overflow");
    }

    if (u > (unsigned long long)INT64_MAX) {
        api->msrest_panic("[" LIB_NAME "] hex_to_decimal(): value exceeds int64 max");
    }

    return api->int_to_value((int64_t)u);
}



EXPORT Value oct_to_decimal(MsrestAPI* api, Value* args, int argc) 
{
    api->check_argc(argc, 1);
    lib_check_type(api, MSREST_STRING, args[0].type, "oct_to_decimal");

    const char *start_oct = api->string_data(args[0]);
    uint32_t string_len = api->string_bytes(args[0]);

    if (string_len == 0 || string_len > 21) {
        api->msrest_panic("[" LIB_NAME "] oct_to_decimal(): string must be 1..21 oct chars, got %d", string_len);
    }

    for (uint32_t i = 0; i < string_len; i++) {
        if (start_oct[i] < '0' || start_oct[i] > '7') {
            api->msrest_panic("[" LIB_NAME "] oct_to_decimal(): cannot convert non-octal number to decimal");
        }
    }

    errno = 0;
    unsigned long long u = strtoull(start_oct, NULL, 8);
    if (errno == ERANGE) {
        api->msrest_panic("[" LIB_NAME "] oct_to_decimal(): overflow");
    }

    if (u > (unsigned long long)INT64_MAX) {
        api->msrest_panic("[" LIB_NAME "] oct_to_decimal(): value exceeds int64 max");
    }

    return api->int_to_value((int64_t)u);
}


EXPORT Value bin_to_decimal(MsrestAPI* api, Value* args, int argc) 
{
    api->check_argc(argc, 1);
    lib_check_type(api, MSREST_STRING, args[0].type, "bin_to_decimal");

    const char *start_oct = api->string_data(args[0]);
    uint32_t string_len = api->string_bytes(args[0]);

    if (string_len == 0 || string_len > 63) {
        api->msrest_panic("[" LIB_NAME "] bin_to_decimal(): string must be 1..63 bin chars, got %d", string_len);
    }

    for (uint32_t i = 0; i < string_len; i++) {
        if (start_oct[i] != '0' && start_oct[i] != '1') {
            api->msrest_panic("[" LIB_NAME "] bin_to_decimal(): cannot convert non-bin number to decimal");
        }
    }

    errno = 0;
    unsigned long long u = strtoull(start_oct, NULL, 8);
    if (errno == ERANGE) {
        api->msrest_panic("[" LIB_NAME "] bin_to_decimal(): overflow");
    }

    if (u > (unsigned long long)INT64_MAX) {
        api->msrest_panic("[" LIB_NAME "] bin_to_decimal(): value exceeds int64 max");
    }

    return api->int_to_value((int64_t)u);
}




EXPORT Value join(MsrestAPI* api, Value* args, int argc) 
{
    api->check_argc(argc, 2);
    lib_check_type(api, MSREST_ARRAY, args[0].type, "join");
    lib_check_type(api, MSREST_STRING, args[1].type, "join");


    Value array = args[0];
    uint32_t size = api->array_size(array);
    if (size == 0) {
        return api->string_new_len("", 0);
    }

    for (int64_t i = 0; i < (int64_t)size; i++) {
        uint8_t eltype = api->value_type(api->array_get(array, i));
        lib_check_type(api, MSREST_STRING, eltype, "join");
    }

    const char *sep = api->string_data(args[1]);
    uint32_t sep_len  = api->string_bytes(args[1]);

    const char **ptrs = safe_malloc(api, (sizeof(const char*) * size));
    uint32_t *lens = safe_malloc(api, (sizeof(uint32_t) * size));

    uint32_t total = 0;
    for (uint32_t i = 0; i < size; i++) {
        Value v = api->array_get(array, i);
        ptrs[i] = api->string_data(v);
        lens[i] = api->string_bytes(v);

        total  += lens[i];
    }
    uint32_t result_len = total + sep_len * (size - 1);
    char* result = safe_malloc(api, result_len + 1);

    char* p = result;
    for (uint32_t i = 0; i < size; i++) {
        memcpy(p, ptrs[i], lens[i]);
        p += lens[i];
        if (i + 1 < size) {
            memcpy(p, sep, sep_len);
            p += sep_len;
        }
    }
    *p = '\0';

    free(ptrs);
    free(lens);
    
    return api->string_new_len(result, result_len);
}

EXPORT Value split(MsrestAPI* api, Value* args, int argc) 
{
    api->check_argc(argc, 2);
    lib_check_type(api, MSREST_STRING, args[0].type, "split");
    lib_check_type(api, MSREST_STRING, args[1].type, "split");

    const char *text = api->string_data(args[0]);
    uint32_t text_len = api->string_bytes(args[0]);

    const char *sep = api->string_data(args[1]);
    uint32_t sep_len = api->string_bytes(args[1]);
    if (sep_len == 0) api->msrest_panic("[" LIB_NAME "] split(): separator cannot be empty");

    Value out = api->array_new(0);

    const char *cursor = text;
    const char *end = text + text_len;

    while (1) {
        const char *found = strstr(cursor, sep);

        if (found == NULL) {
            uint32_t piece_len = end - cursor;
            api->array_push(out, api->string_new_len(cursor, piece_len));
            break;
        }

        uint32_t piece_len = found - cursor;
        api->array_push(out, api->string_new_len(cursor, piece_len));

        cursor = found + sep_len;
    }

    return out;
}


EXPORT Value escape(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 1);
    lib_check_type(api, MSREST_STRING, args[0].type, "escape");


    const char *str = api->string_data(args[0]);
    size_t byte_len = api->string_bytes(args[0]);

    if (byte_len == 0) return api->string_new_len("", 0);

    // худший случай: каждый байт → "\xHH" (4 байта). обычно сильно меньше.
    size_t cap = byte_len + 1;
    size_t len = 0;
    char *out = safe_malloc(api, cap);

    #define ESCAPE_CASE(ch, str) \
        case ch: append_bytes(api, &out, &cap, &len, str, 2)

    size_t i = 0;
    while (i < byte_len) {
        uint8_t c = (uint8_t)str[i];

        switch (c) {
            ESCAPE_CASE('\n', "\\n"); i++; continue;
            ESCAPE_CASE('\t', "\\t"); i++; continue;
            ESCAPE_CASE('\r', "\\r"); i++; continue;
            ESCAPE_CASE('\a', "\\a"); i++; continue;
            ESCAPE_CASE('\b', "\\b"); i++; continue;
            ESCAPE_CASE('\f', "\\f"); i++; continue;
            ESCAPE_CASE('\v', "\\v"); i++; continue;
            ESCAPE_CASE('\0', "\\0"); i++; continue;
            ESCAPE_CASE('\\', "\\\\"); i++; continue;
            ESCAPE_CASE('"',  "\\\""); i++; continue;
        }

        if (c < 0x20 || c == 0x7F) {
            char tmp[5];
            snprintf(tmp, sizeof(tmp), "\\x%02x", c);
            append_bytes(api, &out, &cap, &len, tmp, 4);
            i++;
            continue;
        }

        // всё остальное (включая UTF-8 и лидирующие байты) - как есть
        append_bytes(api, &out, &cap, &len, str + i, 1);
        i++;
    }

    #undef ESCAPE_CASE

    Value v = api->string_new_len(out, len);
    free(out);
    return v;
}


EXPORT Value unescape(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 1);
    lib_check_type(api, MSREST_STRING, args[0].type, "unescape");

    const char *str = api->string_data(args[0]);
    size_t byte_len = api->string_bytes(args[0]);

    if (byte_len == 0) return api->string_new_len("", 0);

    size_t cap = byte_len;
    size_t len = 0;
    char *out = safe_malloc(api, cap);

    for (size_t i = 0; i < byte_len; ) {
        uint8_t c = (uint8_t)str[i];

        if (c != '\\') {
            append_bytes(api, &out, &cap, &len, str + i, 1);
            i++;
            continue;
        }

        // c == '\\'
        if (i + 1 >= byte_len) {
            // висячий backslash оставляем как есть.
            append_bytes(api, &out, &cap, &len, str + i, 1);
            i++;
            continue;
        }

        #define BACK_ESCAPE(ch, strval) \
            case ch: append_bytes(api, &out, &cap, &len, strval, 1)

        uint8_t e = (uint8_t)str[i + 1];
        switch (e) {

            // логика предельно проста: берем простой текст,
            // (\n как 2 символа и транслируем в экранированный ссоответственный).
            BACK_ESCAPE('n', "\n"); i += 2; continue;
            BACK_ESCAPE('t', "\t"); i += 2; continue;
            BACK_ESCAPE('r', "\r"); i += 2; continue;
            BACK_ESCAPE('a', "\a"); i += 2; continue;
            BACK_ESCAPE('b', "\b"); i += 2; continue;
            BACK_ESCAPE('f', "\f"); i += 2; continue;
            BACK_ESCAPE('v', "\v"); i += 2; continue;
            BACK_ESCAPE('0', "\0"); i += 2; continue;
            BACK_ESCAPE('\\', "\\"); i += 2; continue;
            BACK_ESCAPE('"', "\""); i += 2; continue;
            BACK_ESCAPE('\'', "'"); i += 2; continue;

            case 'x': {
                // нужно ровно 2 hex.
                // здесь просто смотрим на эскейп, \xF3 например
                if (i + 3 >= byte_len) goto unknown;
                int h1 = hex_val((uint8_t)str[i + 2]);
                int h2 = hex_val((uint8_t)str[i + 3]);
                if (h1 < 0 || h2 < 0) goto unknown;

                char b = (char)((h1 << 4) | h2);
                append_bytes(api, &out, &cap, &len, &b, 1);
                i += 4;
                continue;
            }

            case 'u': {
                // ровно 4 hex.
                // \u37DA типа.
                if (i + 5 >= byte_len) goto unknown;
                uint32_t cp = 0;
                for (uint8_t k = 0; k < 4; k++) {
                    int h = hex_val((uint8_t)str[i + 2 + k]);
                    if (h < 0) goto unknown;
                    cp = (cp << 4) | (uint32_t)h;
                }
                append_cp(api, &out, &cap, &len, cp);
                i += 6;
                continue;
            }

            case 'U': {
                // ровно 8 hex, \U1234567C.
                if (i + 9 >= byte_len) goto unknown;
                uint32_t cp = 0;
                for (uint8_t k = 0; k < 8; k++) {
                    int h = hex_val((uint8_t)str[i + 2 + k]);
                    if (h < 0) goto unknown;
                    cp = (cp << 4) | (uint32_t)h;
                }
                if (cp > 0x10FFFF) goto unknown;
                append_cp(api, &out, &cap, &len, cp);
                i += 10;
                continue;
            }

            default:
            unknown:
                // неизвестный или невалидный escape оставляем как есть.
                append_bytes(api, &out, &cap, &len, str + i, 1); // '\'.
                append_bytes(api, &out, &cap, &len, str + i + 1, 1); // буква.
                i += 2;
                continue;
        }
    }

    #undef BACK_ESCAPE

    Value v = api->string_new_len(out, len);
    free(out);
    return v;
}




EXPORT Value replace_all(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 3);
    lib_check_type(api, MSREST_STRING, args[0].type, "replace_all");
    lib_check_type(api, MSREST_STRING, args[1].type, "replace_all");
    lib_check_type(api, MSREST_STRING, args[2].type, "replace_all");

    const char *mainstr_start = api->string_data(args[0]);
    const char *replace = api->string_data(args[1]);
    const char *to_what = api->string_data(args[2]);

    uint32_t mainstr_len = api->string_bytes(args[0]);
    uint32_t repl_len = api->string_bytes(args[1]);
    uint32_t towh_len = api->string_bytes(args[2]);

    if (mainstr_len == 0) return api->string_new_len("", 0);
    if (repl_len == 0) return args[0];

    uint32_t expected_len = 0;
    int64_t i = 0;

    while (i < mainstr_len) {
        if ((mainstr_start[i] == replace[0]) && (repl_len + i <= mainstr_len)) {
            if (!memcmp(mainstr_start + i, replace, (size_t)repl_len)) {
                expected_len += towh_len;
                i += repl_len;
            }
            else {
                expected_len += 1;
                i++;
            }
        }
        else {
            expected_len += 1;
            i++;
        }
    }

    char *buf = safe_malloc(api, (size_t)expected_len);
    int64_t c = 0;
    int64_t w = 0;

    while (c < mainstr_len) {
        if ((mainstr_start[c] == replace[0]) && (repl_len + c <= mainstr_len)) {
            if (!memcmp(mainstr_start + c, replace, (size_t)repl_len)) {
                memcpy(buf + w, to_what, towh_len);
                c += repl_len;
                w += towh_len;
            }
            else {
                buf[w] = mainstr_start[c];
                w++;
                c++;
            }
        }
        else {
            buf[w] = mainstr_start[c];
            w++;
            c++;
        }
    }

    Value out = api->string_new_len(buf, (size_t)expected_len);
    free(buf);
    return out;
}



EXPORT Value reverse(MsrestAPI* api, Value* args, int argc) 
{
    api->check_argc(argc, 1);
    lib_check_type(api, MSREST_STRING, args[0].type, "reverse");

    const char *text = api->string_data(args[0]);
    uint32_t text_len = api->string_bytes(args[0]);

    if (text_len == 0) return api->string_new_len("", 0);

    char *writer = safe_malloc(api, text_len);
    char *writer_end = writer + text_len;
    int64_t i = 0;

    // идем по массиву байт, получаем размер руны.
    while (i < text_len) {
        // n байт - размер руны.
        uint8_t cursize = (uint8_t)utf8_char_bytes(text, i);
        // двигаем руну. я с конца точки приема отступаю на n байт, и двигаю туда n байт.
        memmove(writer_end - cursize, (text + i), cursize);
        // это симметричный процесс.
        i += cursize;
        writer_end -= cursize;
    }

    Value out = api->string_new_len(writer, text_len);
    free(writer);
    return out;
}


EXPORT Value repeat(MsrestAPI* api, Value* args, int argc) 
{
    api->check_argc(argc, 2);
    lib_check_type(api, MSREST_STRING, args[0].type, "repeat");
    lib_check_type(api, MSREST_INT, args[1].type, "repeat");

    const char *text = api->string_data(args[0]);
    uint32_t text_len = api->string_bytes(args[0]);
    int64_t count = api->value_to_int(args[1]);

    if (text_len == 0) return api->string_new_len("", 0);
    if (count == 0) return api->string_new_len("", 0);
    if (count < 0) api->msrest_panic("[" LIB_NAME "] repeat(): Cannot use negative value as repeat count %lld", count);
    if (count >= UINT32_MAX) api->msrest_panic("[" LIB_NAME "] repeat(): repeat amount is too big: %lld", count);

    int64_t expected_len = text_len * count;
    if (expected_len >= (int64_t)UINT32_MAX) api->msrest_panic("[" LIB_NAME "] repeat(): final string is too big: %lld", expected_len);
    expected_len = (uint32_t)expected_len;

    char *out_str = safe_malloc(api, expected_len);
    int64_t c = 0;
    while (c < expected_len) {
        memcpy(out_str + c, text, (size_t)text_len);
        c += text_len;
    }

    Value out = api->string_new_len(out_str, expected_len);
    free(out_str);
    return out;
}

EXPORT Value ascii_letters(MsrestAPI* api, Value* args, int argc) 
{
    api->check_argc(argc, 0);
    return api->string_new_len("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", 52);
}

EXPORT Value ascii_lowercase(MsrestAPI* api, Value* args, int argc) 
{
    api->check_argc(argc, 0);
    return api->string_new_len("abcdefghijklmnopqrstuvwxyz", 26);
}

EXPORT Value ascii_uppercase(MsrestAPI* api, Value* args, int argc) 
{
    api->check_argc(argc, 0);
    return api->string_new_len("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26);
}

EXPORT Value hex_digits(MsrestAPI* api, Value* args, int argc) 
{
    api->check_argc(argc, 0);
    return api->string_new_len("0123456789ABCDEFabcdef", 22);
}

EXPORT Value oct_digits(MsrestAPI* api, Value* args, int argc) 
{
    api->check_argc(argc, 0);
    return api->string_new_len("01234567", 8);
}

EXPORT Value whitespace(MsrestAPI* api, Value* args, int argc)
{
    api->check_argc(argc, 0);
    return api->string_new_len(" \t\n\r\v\f", 6);
}