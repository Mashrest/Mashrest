#include "API/msrest_api.h"
#define LIB_NAME "COPY"

EXPORT Value up_copy(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    Value copyable = args[0];
    api->msrest_msg("there");
    switch (copyable.type) {
        case MSREST_ARRAY: {
            uint32_t length = api->array_size(copyable);
            Value out = api->array_new(length);
            for (int64_t i = 0; i < (int64_t)length; i++) {
                Value imm = api->array_get(copyable, i);
                api->array_set(out, i, imm);
            }
            return out;
        }
        case MSREST_DICT: {
            Value out = api->dict_new();
            Value keys = api->dict_keys(copyable);
            uint32_t keyslen = api->array_size(keys);

            for (int64_t i = 0; i < (int64_t)keyslen; i++) {
                Value key = api->array_get(keys, (int)i);
                Value v = api->dict_get(copyable, key);
                api->dict_set(out, key, v);
            }
            return out;
        }

        default: {
            api->msrest_panic("[" LIB_NAME "] upcopy(): Unsupported type to copy (expected dictionary or array)");
        }

    }
}