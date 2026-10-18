#include "API/msrest_api.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define STRINGBUILDER_TYPE "StringBuilder"
#define LIB_NAME "STRING_BUILDER"


EXPORT uint32_t msrest_abi_version(void) {
    return MSREST_API_ABI_VERSION;
}


/* Внутренние структуры */

typedef struct {
    char *buffer;      // данные.
    size_t bytes;
    size_t chars;      // количество рун.
    size_t capacity;   // размер буфера в байтах.
} StringBuilderData;

/* вспомогательные функции */

   static uint32_t count_utf8_chars(const uint8_t *data, size_t bytes) {
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

static void destroy_stringbuilder(void *ptr) {
    StringBuilderData *sb = (StringBuilderData*)ptr;
    if (sb->buffer) {
        free(sb->buffer);
    }
    free(sb);
}

static StringBuilderData* require_stringbuilder(MsrestAPI *api, Value value) {
    void *payload = api->native_payload(value, STRINGBUILDER_TYPE);
    if (!payload) {
        api->msrest_panic("Expected StringBuilder object");
    }
    return (StringBuilderData*)payload;
}

static void ensure_capacity(MsrestAPI *api, StringBuilderData *sb, size_t needed) {
    if (needed <= sb->capacity) return;
    
    // стратегия удвоения (как в Java).
    size_t new_capacity = sb->capacity * 2;
    if (new_capacity < needed) new_capacity = needed;
    if (new_capacity < 16) new_capacity = 16;
    
    char *new_buffer = realloc(sb->buffer, new_capacity);
    if (!new_buffer) {
        api->msrest_panic("[" LIB_NAME "]: out of memory");
    }
    
    sb->buffer = new_buffer;
    sb->capacity = new_capacity;
}

static void append_string(MsrestAPI *api, StringBuilderData *sb, const char *str) {
    if (!str) return;
    
    size_t len = strlen(str);
    if (len == 0) return;
    
    ensure_capacity(api, sb, sb->bytes + len + 1);
    memcpy(sb->buffer + sb->bytes, str, len);
    sb->bytes += len;
    sb->buffer[sb->bytes] = '\0';
    
    sb->chars += count_utf8_chars((const uint8_t*)str, len);
}

static void* safe_malloc(MsrestAPI *api, size_t size) {
    void* p = malloc(size);
    if (!p) api->msrest_panic("[" LIB_NAME "] memory allocation failed");
    return p;
}

/* Экспортируемые функции */

/* 
   StringBuilder::new([initial_capacity])
   Создает новый StringBuilder
   Параметры:
     - initial_capacity: (опционально) начальная емкость, по умолчанию 16
   Возвращает: новый StringBuilder объект
*/

EXPORT Value new(MsrestAPI *api, Value *args, int argc) {
    api->check_argc(argc, 1);
    
    size_t initial_capacity = 16;
    if (argc == 1) {
        intptr_t cap = api->value_to_int(args[0]);
        if (cap < 0) {
            api->msrest_panic("[" LIB_NAME "] new(): capacity must be non-negative");
        }
        if (cap == 0) {
            initial_capacity = 16;
        } else {
            initial_capacity = (size_t)cap;
        }
    }
    
    StringBuilderData *sb = safe_malloc(api, sizeof(StringBuilderData));

    
    sb->capacity = initial_capacity;
    sb->bytes = 0; 
    sb->chars = 0;   
    sb->buffer = malloc(initial_capacity);
    if (!sb->buffer) {
        free(sb);
        api->msrest_panic("StringBuilder::new(): out of memory");
    }
    sb->buffer[0] = '\0';
    
    // создаем нативный объект
    return api->native_new(STRINGBUILDER_TYPE, sb, destroy_stringbuilder);
}

/*
   StringBuilder::append(self, value)
     self: StringBuilder объект
     value: значение для добавления (строка, число, <1>, null)
   Возвращает self (для цепочек вызовов, ненавижу, но надо)
*/
EXPORT Value append(MsrestAPI *api, Value *args, int argc) {
    api->check_argc(argc, 2);
    
    StringBuilderData *sb = require_stringbuilder(api, args[0]);
    Value value = args[1];
    
    // определяем тип значения и преобразуем в строку.
    uint8_t type = api->value_type(value);
    
    switch (type) {
        case MSREST_STRING: {
            const char *str = api->string_data(value);
            append_string(api, sb, str);
            break;
        }
        
        case MSREST_INT: {
            intptr_t num = api->value_to_int(value);
            char buf[64];
            snprintf(buf, sizeof(buf), "%lld", (long long)num);
            append_string(api, sb, buf);
            break;
        }
        
        case MSREST_FLOAT: {
            double num = api->value_to_double(value);
            char buf[128];
            snprintf(buf, sizeof(buf), "%g", num);
            append_string(api, sb, buf);
            break;
        }
        
        case MSREST_BOOL: {
            uint8_t b = api->value_to_bool(value);
            append_string(api, sb, b ? "<1>" : "<0>");
            break;
        }
        
        case MSREST_NULL: {
            append_string(api, sb, "null");
            break;
        }
        
        default: {
            // пытаемся преобразовать через value_to_string (я не помню, что там).
            char *str = api->value_to_string(value);
            if (str) {
                append_string(api, sb, str);
                free(str);
            } else {
                api->msrest_panic("[" LIB_NAME "] append(): unsupported type %s", api->type_name(type));
            }
            break;
        }
    }
    
    return args[0];
}


/*
   StringBuilder::append_char(self, ch)
     self: StringBuilder объект
     ch: символ (как число или строка из 1 символа)
   Возвращает: self
*/
EXPORT Value append_char(MsrestAPI *api, Value *args, int argc) {
    api->check_argc(argc, 2);
    
    StringBuilderData *sb = require_stringbuilder(api, args[0]);
    
    uint8_t type = api->value_type(args[1]);
    const char *char_str = NULL;
    size_t char_len = 0;
    
    if (type == MSREST_INT) {
        // интерпретируем как кодпоинт Unicode (а кто меня осудит?).
        intptr_t codepoint = api->value_to_int(args[1]);
        if (codepoint < 0 || codepoint > 0x10FFFF) {
            api->msrest_panic("[" LIB_NAME "] append_char(): invalid codepoint");
        }
        
        // конвертируем кодпоинт в UTF-8.
        // TODO рефакторинг.
        static char utf8_buf[5];
        if (codepoint < 0x80) {
            utf8_buf[0] = (char)codepoint;
            char_len = 1;
        } else if (codepoint < 0x800) {
            utf8_buf[0] = (char)(0xC0 | (codepoint >> 6));
            utf8_buf[1] = (char)(0x80 | (codepoint & 0x3F));
            char_len = 2;
        } else if (codepoint < 0x10000) {
            utf8_buf[0] = (char)(0xE0 | (codepoint >> 12));
            utf8_buf[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            utf8_buf[2] = (char)(0x80 | (codepoint & 0x3F));
            char_len = 3;
        } else {
            utf8_buf[0] = (char)(0xF0 | (codepoint >> 18));
            utf8_buf[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
            utf8_buf[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            utf8_buf[3] = (char)(0x80 | (codepoint & 0x3F));
            char_len = 4;
        }
        char_str = utf8_buf;
        
    } else if (type == MSREST_STRING) {
        // берем первый символ из строки.
        const char *str = api->string_data(args[1]);
        if (!str || !str[0]) {
            return args[0];
        }
        
        // определяем длину первого символа в UTF-8.
        uint8_t c = (uint8_t)str[0];
        if ((c & 0x80) == 0) char_len = 1;
        else if ((c & 0xE0) == 0xC0) char_len = 2;
        else if ((c & 0xF0) == 0xE0) char_len = 3;
        else if ((c & 0xF8) == 0xF0) char_len = 4;
        else char_len = 1;
        
        char_str = str;
    } else {
        api->msrest_panic("[" LIB_NAME "] append_char(): expected integer or string, got %s", api->type_name(type));
    }
    
    // добавляем символ.
    ensure_capacity(api, sb, sb->bytes + char_len + 1);
    memcpy(sb->buffer + sb->bytes, char_str, char_len);
    sb->bytes += char_len;
    sb->chars++;
    sb->buffer[sb->bytes] = '\0';
    
    return args[0];
}

/*
   StringBuilder::to_string(self)
   Преобразует StringBuilder в строку Mashrest
     self: StringBuilder объект
*/

EXPORT Value build(MsrestAPI *api, Value *args, int argc) {
    api->check_argc(argc, 1);
    
    StringBuilderData *sb = require_stringbuilder(api, args[0]);
    return api->string_new_len(sb->buffer, sb->bytes);
}

/*
   StringBuilder::clear(self)
   Очищает содержимое, но сохраняет память
   Параметры:
     self: StringBuilder объект
   Возвращает: null (для конвенции вызовов)
*/
EXPORT Value clear(MsrestAPI *api, Value *args, int argc) {
    api->check_argc(argc, 1);

    StringBuilderData *sb = require_stringbuilder(api, args[0]);
    sb->bytes = 0;
    sb->chars = 0;
    sb->buffer[0] = '\0';
    
    return api->null_to_value();
}

/*
   StringBuilder::length(self)
   Возвращает текущую длину строки
     self: StringBuilder объект
   Возвращает: длина
*/
EXPORT Value length(MsrestAPI *api, Value *args, int argc) {
    api->check_argc(argc, 1);
    
    StringBuilderData *sb = require_stringbuilder(api, args[0]);
    return api->int_to_value((intptr_t)sb->chars);
}

EXPORT Value bytes(MsrestAPI *api, Value *args, int argc) {
    api->check_argc(argc, 1);
    
    StringBuilderData *sb = require_stringbuilder(api, args[0]);
    return api->int_to_value((intptr_t)sb->bytes);
}

/*
   StringBuilder::capacity(self)
   Возвращает текущую емкость
   Параметры:
     self: StringBuilder объект
   Возвращает: емкость
*/
EXPORT Value capacity(MsrestAPI *api, Value *args, int argc) {
    api->check_argc(argc, 1);
    
    StringBuilderData *sb = require_stringbuilder(api, args[0]);
    return api->int_to_value((intptr_t)sb->capacity);
}

/*
   StringBuilder::ensure_capacity(self, min_capacity)
   Гарантирует, что емкость не меньше указанной
     self: StringBuilder объект
     min_capacity: минимальная емкость
*/
EXPORT Value ensure(MsrestAPI *api, Value *args, int argc) {
    api->check_argc(argc, 2);
    
    StringBuilderData *sb = require_stringbuilder(api, args[0]);
    size_t min_cap = (size_t)api->value_to_int(args[1]);
    
    ensure_capacity(api, sb, min_cap);
    return api->null_to_value();
}

/*
   StringBuilder::reset(self)
   Полностью сбрасывает StringBuilder (очищает и уменьшает емкость до 16).
   Параметры:
     self: StringBuilder объект
*/
EXPORT Value reset(MsrestAPI *api, Value *args, int argc) {
    api->check_argc(argc, 2);
    
    StringBuilderData *sb = require_stringbuilder(api, args[0]);
    
    // освобождаем старый буфер.
    if (sb->buffer) {
        free(sb->buffer);
    }
    
    // создаем новый с дефолтной емкостью.
    sb->capacity = 16;
    sb->bytes = 0; 
    sb->chars = 0;
    sb->buffer = safe_malloc(api, 16);
    sb->buffer[0] = '\0';
    
    return api->null_to_value();
}

/*
   StringBuilder::reverse(self)
   Разворачивает строку
   Параметры:
     self: StringBuilder объект
   Возвращает: self
*/
EXPORT Value reverse(MsrestAPI *api, Value *args, int argc) {
    api->check_argc(argc, 1);
    
    StringBuilderData *sb = require_stringbuilder(api, args[0]);
    
    if (sb->chars <= 1) return args[0];  // 0 или 1 символ - ничего не делаем.
    
    // выделяем временный буфер для перевернутой строки.
    char *new_buffer = safe_malloc(api, sb->bytes + 1);
    
    // проходим по символам справа налево.
    size_t write_pos = 0;
    size_t read_pos = sb->bytes;
    
    while (read_pos > 0) {
        // находим начало текущего символа (идем назад).
        size_t char_start = read_pos - 1;
        
        // определяем длину символа (ищем начало UTF-8 последовательности).
        while (char_start > 0 && (sb->buffer[char_start] & 0xC0) == 0x80) {
            char_start--;
        }
        
        // определяем длину символа.
        uint8_t first = (uint8_t)sb->buffer[char_start];
        size_t char_len;
        if ((first & 0x80) == 0) char_len = 1;
        else if ((first & 0xE0) == 0xC0) char_len = 2;
        else if ((first & 0xF0) == 0xE0) char_len = 3;
        else if ((first & 0xF8) == 0xF0) char_len = 4;
        else char_len = 1;
        
        // копируем символ целиком в новый буфер.
        memcpy(new_buffer + write_pos, sb->buffer + char_start, char_len);
        write_pos += char_len;
        read_pos = char_start;
    }
    
    new_buffer[write_pos] = '\0';
    
    // заменяем старый буфер на новый.
    free(sb->buffer);
    sb->buffer = new_buffer;
    sb->bytes = write_pos;
    // chars остается тем же (количество символов не изменилось).
    
    return args[0];
}

/*
   StringBuilder::substring(self, start [, end])
   Возвращает подстроку как новый StringBuilder
   Параметры:
     self: StringBuilder объект
     start: начальный индекс
     end: (опционально) конечный индекс (не включительно)
   Возвращает: новый StringBuilder
*/
EXPORT Value msrest_substring(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 3);

    StringBuilderData *sb = require_stringbuilder(api, args[0]);

    intptr_t start = api->value_to_int(args[1]);
    if (start < 0 || start > (intptr_t)sb->chars)
        api->msrest_panic("[" LIB_NAME "] substring(): start out of range\n");

    intptr_t end = api->value_to_int(args[2]);
    if (end == -1) {
        end = (intptr_t)sb->chars;
    } else if (end < 0 || end > (intptr_t)sb->chars) {
        api->msrest_panic("[" LIB_NAME "] substring(): end out of range\n");
    }

    if (start > end)
        api->msrest_panic("[" LIB_NAME "] substring(): start > end\n");

    size_t start_char = (size_t)start;
    size_t end_char   = (size_t)end;

    size_t start_byte = 0;
    size_t end_byte   = 0;
    size_t current_char = 0;
    const uint8_t *data = (const uint8_t*)sb->buffer;
    size_t pos = 0;

    while (pos < sb->bytes && current_char < start_char) {
        uint8_t c = data[pos];
        if ((c & 0x80) == 0)         pos += 1;
        else if ((c & 0xE0) == 0xC0) pos += 2;
        else if ((c & 0xF0) == 0xE0) pos += 3;
        else if ((c & 0xF8) == 0xF0) pos += 4;
        else                         pos += 1;
        current_char++;
    }
    start_byte = pos;

    while (pos < sb->bytes && current_char < end_char) {
        uint8_t c = data[pos];
        if ((c & 0x80) == 0)         pos += 1;
        else if ((c & 0xE0) == 0xC0) pos += 2;
        else if ((c & 0xF0) == 0xE0) pos += 3;
        else if ((c & 0xF8) == 0xF0) pos += 4;
        else                         pos += 1;
        current_char++;
    }
    end_byte = pos;

    size_t byte_len = end_byte - start_byte;
    size_t char_len = end_char - start_char;

    StringBuilderData *new_sb = safe_malloc(api, sizeof(StringBuilderData));

    size_t cap = byte_len + 1;
    if (cap < 16) cap = 16;

    new_sb->capacity = cap;
    new_sb->bytes    = byte_len;
    new_sb->chars    = char_len;
    new_sb->buffer   = malloc(cap);
    if (!new_sb->buffer) {
        free(new_sb);
        api->msrest_panic("[" LIB_NAME "] substring(): out of memory\n");
    }

    memcpy(new_sb->buffer, (const char*)sb->buffer + start_byte, byte_len);
    new_sb->buffer[byte_len] = '\0';

    return api->native_new(STRINGBUILDER_TYPE, new_sb, destroy_stringbuilder);
}


/*

ъ

*/