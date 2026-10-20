#include "API/msrest_api.h"
#define LIB_NAME "COPY"


EXPORT uint32_t msrest_abi_version(void) {
    return MSREST_API_ABI_VERSION;
}


EXPORT Value up_copy(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    Value copyable = args[0];
    Value out;

    switch (copyable.type) {
        case MSREST_ARRAY: {
            uint32_t length = api->array_size(copyable);
            out = api->array_new(length);
            for (int64_t i = 0; i < (int64_t)length; i++) {
                Value imm = api->array_get(copyable, i);
                api->array_set(out, i, imm);
            }
            break;
        }
        case MSREST_DICT: {
            out = api->dict_new();
            Value keys = api->dict_keys(copyable);
            uint32_t keyslen = api->array_size(keys);

            for (int64_t i = 0; i < (int64_t)keyslen; i++) {
                Value key = api->array_get(keys, (uint32_t)i);
                Value v = api->dict_get(copyable, key);
                api->dict_set(out, key, v);
            }
            break;
        }

        case MSREST_STRING: {
            uint32_t str_len = api->string_bytes(copyable);
            const char *data = api->string_data(copyable);
            out = api->string_new_len(data, str_len);
            break;
        }

        default: {
            api->msrest_panic("[" LIB_NAME "] upcopy(): Unsupported type to copy (expected dictionary or array)");
            break;
        }

    }
    return out;
}