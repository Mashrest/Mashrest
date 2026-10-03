
#include "API/msrest_api.h"

/*Дополнительные интеграции стандартных библиотек*/
#include <direct.h>
#include <stdio.h>

#define MAX_READFILE_SIZE 2147483648

EXPORT Value file_exists(MsrestAPI *api, Value *args, int argc)
{   
    api->check_argc(argc, 1);
    api->check_type(MSREST_STRING, args[0].type);

    if(api->string_empty(args[0])) {
        api->msrest_msg("Cannot check an empty string!\n"); 
        return api->bool_to_value(0);}
    
    FILE *f = fopen(api->string_data(args[0]), "r");
    if (!f) return api->bool_to_value(0);
    fclose(f);

    return api->bool_to_value(1);
}



EXPORT Value readfile(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->check_type(MSREST_STRING, args[0].type);

    FILE *f = fopen(api->string_data(args[0]), "r");
    if (!f) return api->bool_to_value(0);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc(size + 1);

    size_t read = fread(buffer, 1, size, f);
    buffer[size] = '\0';

    if (size > MAX_READFILE_SIZE) {
    fclose(f);
    api->msrest_panic("file too large for readfile\n");
}

    if (read != (size_t)size) {
        fclose(f);
        free(buffer);
        return api->bool_to_value(0);
    }

    fclose(f);
    Value v = api->string_to_value(buffer);
    free(buffer);

    return v;
}


EXPORT Value readlines(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);

    // проверяем тип первого аргумента (имя файла)
    api->check_type(MSREST_STRING, args[0].type);
    api->check_type(MSREST_STRING, args[1].type);

    // получаем режим
    const char *mode = api->string_data(args[1]);
    int clean_mode = 0; // 0 - all, 1 - clean
    
    // сравниваем режим (регистрозависимо, как в оригинале)
    if (strcmp(mode, "clean") == 0) { clean_mode = 1;} 
    else if (strcmp(mode, "all") == 0) { clean_mode = 0;} 
    else { api->msrest_panic("readlines: invalid mode. Use 'clean' or 'all'\n");
        return api->bool_to_value(0);
    }

    // открываем файл
    FILE *f = fopen(api->string_data(args[0]), "rb");
    if (!f) api->msrest_panic("readlines: cannot open file\n");
    
    // получаем размер файла
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        api->msrest_panic("readlines: memory allocation failed\n");}

    size_t read = fread(buffer, 1, size, f);
    buffer[read] = '\0';

    if (read != (size_t)size) {
        fclose(f);
        free(buffer);
        api->msrest_panic("readlines: read error\n");
    }

    // создаем массив для строк
    Value array = api->array_new(0);
    char *start = buffer;
    char *p = buffer;

    while (1) {
        if (*p == '\n' || *p == '\0') {
            if (p > start && p[-1] == '\r')
                p[-1] = '\0';

            char old = *p;
            if (old == '\0' && start == p)
                break;

            *p = '\0';
            // проверяем, является ли строка пустой
            int is_empty = 1;
            char *check = start;
            while (*check) {
                if (*check != ' ' && *check != '\t' && *check != '\r' && *check != '\n') {
                    is_empty = 0;
                    break;
                }
                check++;
            }

            // в clean режиме пропускаем пустые строки
            if (!(clean_mode && is_empty)) {
                Value line = api->string_to_value(start);
                api->array_push(array, line);
            }

            if (old == '\0')
                break;

            start = p + 1;
        }
        p++;
    }

    fclose(f);
    free(buffer);

    return array;
}


EXPORT Value writeto(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    api->check_type(MSREST_STRING, args[0].type);
    api->check_type(MSREST_STRING, args[1].type);
    FILE *f = fopen(api->string_data(args[0]), "wb");

    uint32_t byte_length = api->string_bytes(args[1]);

    if (!f) api->msrest_panic("Cannot open file");
    const char *text = api->string_data(args[1]);
    fwrite(text, 1, byte_length, f);

    fclose(f);
    return api->bool_to_value(1);
}

EXPORT Value append(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    api->check_type(MSREST_STRING, args[0].type);
    api->check_type(MSREST_STRING, args[1].type);

    const char *filename = api->string_data(args[0]);
    const char *content = api->string_data(args[1]);

    FILE *f = fopen(filename, "ab");
    if (!f) api->msrest_panic("appendfile: cannot open file for appending\n");
    
    size_t written = fwrite(content, 1, strlen(content), f);
    fclose(f);

    return api->int_to_value(written);
}
