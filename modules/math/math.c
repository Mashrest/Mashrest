#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "API/msrest_api.h"

#define LIB_NAME "MATH"


static inline void check_numeric(MsrestAPI *api, uint8_t got)
{
    if (got != MSREST_INT && got != MSREST_FLOAT)
        api->msrest_panic("[" LIB_NAME "] Type mismatch! Required: NUMERIC, Got: %s\n",
                          type_names[got]);
}


EXPORT Value msrest_sqroot(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    double x = api->value_to_double(args[0]);
    if (x < 0.0)
        api->msrest_panic("[" LIB_NAME "] sqroot(): argument must be >= 0\n");

    return api->float_to_value(sqrt(x));
}


EXPORT Value msrest_square(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    Value x = args[0];

    if (x.type == MSREST_INT) {
        int64_t v = x.integer;
        return api->int_to_value(v * v);
    }

    double v = api->value_to_double(x);
    return api->float_to_value(v * v);
}


EXPORT Value msrest_power(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    check_numeric(api, args[0].type);
    check_numeric(api, args[1].type);

    double base = api->value_to_double(args[0]);
    double exp  = api->value_to_double(args[1]);

    if (base < 0.0 && exp != floor(exp))
        api->msrest_panic("[" LIB_NAME "] power(): negative base with non-integer exponent\n");

    return api->float_to_value(pow(base, exp));
}


EXPORT Value msrest_pi(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->check_type(MSREST_INT, args[0].type);

    int precision = api->value_to_int(args[0]);

    if (precision < 0)
        api->msrest_panic("[" LIB_NAME "] pi(): precision must be >= 0\n");
    if (precision > 18)
        api->msrest_panic("[" LIB_NAME "] pi(): precision must be <= 18\n");

    const double Pi = 3.14159265358979323846264338327950288;
    double factor = pow(10.0, precision);
    double result = round(Pi * factor) / factor;

    return api->float_to_value(result);
}


EXPORT Value msrest_exponent(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->check_type(MSREST_INT, args[0].type);

    int precision = api->value_to_int(args[0]);

    if (precision < 0)
        api->msrest_panic("[" LIB_NAME "] exponent(): precision must be >= 0\n");
    if (precision > 20)
        api->msrest_panic("[" LIB_NAME "] exponent(): precision must be <= 20\n");

    const double E = 2.71828182845904523536028747135266249;
    double factor = pow(10.0, precision);
    double result = round(E * factor) / factor;

    return api->float_to_value(result);
}


EXPORT Value msrest_sinus(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    double x = api->value_to_double(args[0]);
    return api->float_to_value(sin(x));
}


EXPORT Value msrest_cosin(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    double x = api->value_to_double(args[0]);
    return api->float_to_value(cos(x));
}


EXPORT Value msrest_abs(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    if (args[0].type == MSREST_INT) {
        int64_t operand = api->value_to_int(args[0]);
        if (operand == INT64_MIN)
            return api->int_to_value(INT64_MAX);
        int64_t out = (operand < 0) ? -operand : operand;
        return api->int_to_value(out);
    }

    double num = api->value_to_double(args[0]);
    double out = signbit(num) ? -num : num;
    return api->float_to_value(out);
}


EXPORT Value msrest_max(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    check_numeric(api, args[0].type);
    check_numeric(api, args[1].type);

    if (args[0].type == MSREST_INT && args[1].type == MSREST_INT) {
        int64_t a = api->value_to_int(args[0]);
        int64_t b = api->value_to_int(args[1]);
        return api->int_to_value((a > b) ? a : b);
    }

    double a = api->value_to_double(args[0]);
    double b = api->value_to_double(args[1]);
    return api->float_to_value((a > b) ? a : b);
}


EXPORT Value msrest_min(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    check_numeric(api, args[0].type);
    check_numeric(api, args[1].type);

    if (args[0].type == MSREST_INT && args[1].type == MSREST_INT) {
        int64_t a = api->value_to_int(args[0]);
        int64_t b = api->value_to_int(args[1]);
        return api->int_to_value((a < b) ? a : b);
    }

    double a = api->value_to_double(args[0]);
    double b = api->value_to_double(args[1]);
    return api->float_to_value((a < b) ? a : b);
}


EXPORT Value msrest_clamp(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 3);
    check_numeric(api, args[0].type);
    check_numeric(api, args[1].type);
    check_numeric(api, args[2].type);

    double a      = api->value_to_double(args[0]);
    double number = api->value_to_double(args[1]);
    double b      = api->value_to_double(args[2]);

    if (a > b)
        api->msrest_panic("[" LIB_NAME "] clamp(): minimum is bigger than maximum\n");

    double result = (number > b) ? b : (number < a) ? a : number;

    if (args[0].type == MSREST_INT &&
        args[1].type == MSREST_INT &&
        args[2].type == MSREST_INT)
    {
        return api->int_to_value((int64_t)result);
    }

    return api->float_to_value(result);
}