/* << DLL strings >> */

/* < INCLUDE FILES > */
#include <stdio.h>
#include "API/msrest_api.h"
#include <direct.h>
#include <ctype.h>
#include <string.h>


// UTF-8 утилиты

static uint32_t utf8_decode(const uint8_t* data) {
    uint8_t c = data[0];
    if ((c & 0x80) == 0) return c;
    if ((c & 0xE0) == 0xC0) return ((c & 0x1F) << 6) | (data[1] & 0x3F);
    if ((c & 0xF0) == 0xE0) return ((c & 0x0F) << 12) | ((data[1] & 0x3F) << 6) | (data[2] & 0x3F);
    if ((c & 0xF8) == 0xF0) return ((c & 0x07) << 18) | ((data[1] & 0x3F) << 12) | 
                                 ((data[2] & 0x3F) << 6) | (data[3] & 0x3F);
    return 0xFFFD;
}

static inline size_t utf8_char_bytes(const char* str, size_t pos) {
    uint8_t c = (uint8_t)str[pos];
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

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

static uint32_t* decode_to_codepoints(const char* str, size_t char_len, size_t* out_byte_len) {
    uint32_t* cp = malloc(char_len * sizeof(uint32_t));
    if (!cp) return NULL;
    
    size_t byte_pos = 0;
    for (size_t i = 0; i < char_len; i++) {
        cp[i] = utf8_decode((uint8_t*)str + byte_pos);
        byte_pos += utf8_char_bytes(str, byte_pos);
    }
    if (out_byte_len) *out_byte_len = byte_pos;
    return cp;
}

static char* encode_codepoints(const uint32_t* cp, size_t char_len, size_t* out_byte_len) {
    size_t total = 0;
    for (size_t i = 0; i < char_len; i++) {
        if (cp[i] < 0x80) total += 1;
        else if (cp[i] < 0x800) total += 2;
        else if (cp[i] < 0x10000) total += 3;
        else total += 4;
    }
    
    char* buf = malloc(total + 1);
    if (!buf) return NULL;
    
    size_t pos = 0;
    for (size_t i = 0; i < char_len; i++) {
        pos += utf8_encode(buf + pos, cp[i]);
    }
    buf[pos] = '\0';
    if (out_byte_len) *out_byte_len = pos;
    return buf;
}

// проверки символов

static int is_unicode_space(uint32_t cp) {
    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\v' || cp == '\f') return 1;
    if (cp == 0x00A0 || cp == 0x1680 || cp == 0x2028 || cp == 0x2029 || 
        cp == 0x202F || cp == 0x205F || cp == 0x3000) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    return 0;
}

static uint32_t require_char(MsrestAPI* api, Value* args, int argc, const char* func) {
    if (argc != 1) api->msrest_panic("%s() expects 1 argument, got %d", func, argc);
    api->check_type(MSREST_STRING, args[0].type);
    
    const char* str = api->string_data(args[0]);
    if (api->string_chars(args[0]) != 1) {
        api->msrest_panic("%s() expects a string of length 1", func);
    }
    return utf8_decode((uint8_t*)str);
}

// общая функция для upper/lower

static Value change_case(MsrestAPI* api, Value* args, int argc, int to_upper) {
    if (argc != 1) api->msrest_panic("%s() expects 1 argument, got %d", 
                                     to_upper ? "upper" : "lower", argc);
    api->check_type(MSREST_STRING, args[0].type);
    
    const char* str = api->string_data(args[0]);
    if (!str) api->msrest_panic("%s() received NULL string", to_upper ? "upper" : "lower");
    
    size_t char_len = (size_t)api->string_chars(args[0]);
    if (char_len == 0) return api->string_new_len("", 0);
    
    size_t byte_len;
    uint32_t* cp = decode_to_codepoints(str, char_len, &byte_len);
    if (!cp) api->msrest_panic("%s() malloc failed", to_upper ? "upper" : "lower");
    
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
    if (!buf) api->msrest_panic("%s() malloc failed", to_upper ? "upper" : "lower");
    
    Value result = api->string_new_len(buf, new_byte_len);
    free(buf);
    return result;
}

// is_* проверки

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

// EXPORT функции

/* вспомогательные функции для управления буфером */

static void* safe_realloc(MsrestAPI *api, void* ptr, size_t size) {
    void* p = realloc(ptr, size);
    if (!p) api->msrest_panic("memory reallocation failed");
    return p;
}

static void ensure_capacity(MsrestAPI *api, char** buffer, size_t* capacity, size_t needed) {
    if (needed >= *capacity) {
        while (*capacity <= needed) {
            *capacity *= 2;
        }
        *buffer = safe_realloc(api, *buffer, *capacity);
    }
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
    if (!p) api->msrest_panic("memory allocation failed");
    return p;
}


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
    api->check_type(MSREST_STRING, args[0].type);
    
    const char* str = api->string_data(args[0]);
    size_t char_len = (size_t)api->string_chars(args[0]);
    
    if (char_len == 0) api->msrest_panic("ord() cannot convert an empty string");
    
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
    int type = api->value_type(args[0]);
    
    if (type == MSREST_INT) {
        int32_t cp = (int32_t)api->value_to_int(args[0]);
        if (cp < 0 || cp > 0x10FFFF) {
            api->msrest_panic("chr() code point out of range (0..0x10FFFF)");
        }
        char buf[5];
        size_t len = utf8_encode(buf, (uint32_t)cp);
        buf[len] = '\0';
        return api->string_new_len(buf, len);
    }
    
    if (type == MSREST_ARRAY) {
        int len = api->array_size(args[0]);
        uint32_t* cp = malloc(len * sizeof(uint32_t));
        if (!cp) api->msrest_panic("chr(): out of memory");
        
        for (int i = 0; i < len; i++) {
            Value v = api->array_get(args[0], i);
            if (api->value_type(v) != MSREST_INT) {
                free(cp);
                api->msrest_panic("chr() array must contain integers");
            }
            cp[i] = (uint32_t)api->value_to_int(v);
            if (cp[i] > 0x10FFFF) {
                free(cp);
                api->msrest_panic("chr() code point out of range");
            }
        }
        
        size_t byte_len;
        char* buf = encode_codepoints(cp, len, &byte_len);
        free(cp);
        if (!buf) api->msrest_panic("chr(): out of memory");
        
        Value result = api->string_new_len(buf, byte_len);
        free(buf);
        return result;
    }
    
    api->msrest_panic("chr() expects an integer or array");
}

EXPORT Value contains(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 2);
    api->check_type(MSREST_STRING, args[0].type);
    api->check_type(MSREST_STRING, args[1].type);
    
    const char* str = api->string_data(args[0]);
    const char* sub = api->string_data(args[1]);
    return api->bool_to_value(strstr(str, sub) != NULL);
}

EXPORT Value find(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 2);
    api->check_type(MSREST_STRING, args[0].type);
    api->check_type(MSREST_STRING, args[1].type);
    
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
    api->check_type(MSREST_STRING, args[0].type);
    
    const char* str = api->string_data(args[0]);
    size_t byte_len = strlen(str);
    size_t char_len = (size_t)api->string_chars(args[0]);
    
    if (byte_len == 0 || char_len == 0) return api->string_new_len("", 0);
    
    size_t byte_start = 0;
    size_t chars_skipped = 0;
    while (byte_start < byte_len && chars_skipped < char_len) {
        uint32_t cp = utf8_decode((uint8_t*)str + byte_start);
        if (!is_unicode_space(cp)) break;
        byte_start += utf8_char_bytes(str, byte_start);
        chars_skipped++;
    }
    
    if (byte_start >= byte_len) return api->string_new_len("", 0);
    
    size_t byte_end = byte_len;
    while (byte_end > byte_start) {
        size_t prev = byte_end;
        while (prev > byte_start && ((uint8_t)str[prev - 1] & 0xC0) == 0x80) prev--;
        if (prev <= byte_start) break;
        
        uint32_t cp = utf8_decode((uint8_t*)str + prev - 1);
        if (!is_unicode_space(cp)) break;
        byte_end = prev - 1;
    }
    
    size_t result_len = byte_end - byte_start;
    char* result = malloc(result_len + 1);
    if (!result) api->msrest_panic("trim(): out of memory");
    
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
    api->check_type(MSREST_STRING, args[0].type);
    api->check_type(MSREST_INT, args[1].type);
    api->check_type(MSREST_INT, args[2].type);
    
    const char* str = api->string_data(args[0]);
    size_t byte_len = strlen(str);
    size_t char_len = (size_t)api->string_chars(args[0]);
    
    int start_char = api->value_to_int(args[1]);
    int len_char = api->value_to_int(args[2]);
    
    if (start_char < 0 || start_char > (int)char_len) {
        api->msrest_panic("substr(): start out of range");
    }
    
    // -1 (или любое отрицательное) — до конца строки
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
    char* result = malloc(result_len + 1);
    if (!result) api->msrest_panic("substr(): out of memory");
    
    memcpy(result, str + byte_start, result_len);
    result[result_len] = '\0';
    
    Value v = api->string_new_len(result, result_len);
    free(result);
    return v;
}

/* основная функция форматирования */
EXPORT Value format(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 2);
    api->check_type(MSREST_STRING, args[0].type);
    api->check_type(MSREST_DICT, args[1].type);
    
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
                api->msrest_panic("format(): unclosed brace");
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
                    api->msrest_panic("format(): expected precision after '.'");
                }
                
                char* endptr;
                precision = (int)strtol(prec_str, &endptr, 10);
                if (*endptr != '\0' || precision < 0) {
                    free(full_cstr);
                    free(key_cstr);
                    free(result);
                    api->msrest_panic("format(): invalid precision '%s'", prec_str);
                }
                
                free(full_cstr);
            } else {
                key_cstr = full_cstr;
            }
            
            size_t key_len = strlen(key_cstr);
            Value key_val = api->string_new_len(key_cstr, key_len);
            
            if (!api->dict_has(dict, key_val)) {
                free(key_cstr);
                free(result);
                api->msrest_panic("format(): key '%s' not found", key_cstr);
            }

            Value val = api->dict_get(dict, key_val);
            int val_type = api->value_type(val);
            
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
            
            if (!val_str) {
                free(key_cstr);
                free(result);
                api->msrest_panic("format(): value_to_string returned NULL");
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
            api->msrest_panic("format(): unexpected '}'");
        } else {
            ensure_capacity(api, &result, &cap, len + 1);
            result[len++] = *p;
            p++;
        }
    }
    
    result[len] = '\0';
    Value result_val = api->string_new_len(result, len);
    free(result);
    
    return result_val;
}