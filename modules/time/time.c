#include "API/msrest_api.h"
#include "time.h"
#include <time.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

// Функция: time.now()
// Возвращает текущее время в миллисекундах
EXPORT Value time_now(MsrestAPI *api, Value *args, int argc)
{
    (void)args;
    (void)argc;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    
    // Переводим в миллисекунды
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    
    return api->int_to_value((intptr_t)ms);
}

// Функция: time.sleep(ms)
// Приостанавливает выполнение на ms миллисекунд
EXPORT Value time_sleep(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->check_type(MSREST_INT, args[0].type);
    
    uint64_t ms = (uint64_t)api->value_to_int(args[0]);
    
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep(ms * 1000); // микросекунды
#endif
    
    return api->null_to_value();
}