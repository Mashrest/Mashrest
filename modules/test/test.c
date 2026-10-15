#include "API/msrest_api.h"
#include <stdlib.h>
#include <stdint.h>

#define TEST_TYPE "TestObject"



EXPORT uint32_t msrest_abi_version(void) {
    return MSREST_API_ABI_VERSION;
}


typedef struct
{
    int value;
} TestObject;

static void destroy_test(void *ptr)
{
    free(ptr);
}

static TestObject *require_test(MsrestAPI *api, Value value)
{
    TestObject *obj =
        (TestObject *)api->native_payload(value, TEST_TYPE);

    if(obj == NULL)
        api->msrest_panic("NULL native object");

    return obj;
}

EXPORT Value new(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 0);

    TestObject *obj = malloc(sizeof(TestObject));

    if(obj == NULL)
        api->msrest_panic("[TEST] new(): Out of memory");

    obj->value = 0;

    return api->native_new(TEST_TYPE, obj, destroy_test);
}

EXPORT Value get(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    TestObject *obj = require_test(api, args[0]);

    return api->int_to_value(obj->value);
}

EXPORT Value set(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);

    TestObject *obj = require_test(api, args[0]);
    obj->value = (int)api->value_to_int(args[1]);

    return api->null_to_value();
}

EXPORT Value inc(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);

    TestObject *obj = require_test(api, args[0]);
    obj->value++;

    return api->int_to_value(obj->value);
}

EXPORT Value address(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    TestObject *obj = require_test(api, args[0]);

    return api->int_to_value((intptr_t)obj);
}

