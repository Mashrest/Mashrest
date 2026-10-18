#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "API/msrest_api.h"

#define LIB_NAME "MATH"


EXPORT uint32_t msrest_abi_version(void) {
    return MSREST_API_ABI_VERSION;
}


static inline void check_finite_arg(MsrestAPI *api, Value v, const char *fn)
{
    if (v.type == MSREST_FLOAT) {
        double d = v.floating;
        if (!isfinite(d))
            api->msrest_panic("[" LIB_NAME "] %s(): argument is not finite", fn);
    }
}


static inline double finite_or_panic(MsrestAPI *api, double v, const char *fn)
{
    if (!isfinite(v))
        api->msrest_panic("[" LIB_NAME "] %s(): result is not finite (Inf/NaN)", fn);
    return v;
}

static inline void check_numeric(MsrestAPI *api, uint8_t got)
{
    if (got != MSREST_INT && got != MSREST_FLOAT)
        api->msrest_panic("[" LIB_NAME "] Type mismatch! Required: NUMERIC, Got: %s",
                          api->type_name(got));
}




EXPORT Value msrest_sqroot(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    double x = api->value_to_double(args[0]);
    if (x < 0.0)
        api->msrest_panic("[" LIB_NAME "] sqroot(): argument must be >= 0");

    double r = sqrt(x);
    return api->float_to_value(finite_or_panic(api, r, "sqroot"));
}


EXPORT Value msrest_square(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    Value x = args[0];

    if (x.type == MSREST_INT) {
        int64_t v = x.integer;
        int64_t out;

        if (__builtin_mul_overflow(v, v, &out))
            api->msrest_panic("[" LIB_NAME "] square(): got integer overflow");
        return api->int_to_value(out);
    }

    double v = api->value_to_double(x);
    double vv = v * v;
    if (!isfinite(vv)) { api->msrest_panic("[" LIB_NAME "] square(): Got float overflow"); }
    return api->float_to_value(vv);
}


EXPORT Value msrest_power(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    check_numeric(api, args[0].type);
    check_numeric(api, args[1].type);

    check_finite_arg(api, args[0], "power");
    check_finite_arg(api, args[1], "power");

    double base = api->value_to_double(args[0]);
    double exp = api->value_to_double(args[1]);

    if (base < 0.0 && exp != floor(exp))
        api->msrest_panic("[" LIB_NAME "] power(): negative base with non-integer exponent");

    double r = pow(base, exp);
    return api->float_to_value(finite_or_panic(api, r, "power"));
}


EXPORT Value msrest_const_pi(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->lib_check_type(MSREST_INT, args[0].type, LIB_NAME, "pi");

    int precision = api->value_to_int(args[0]);

    if (precision < 0)
        api->msrest_panic("[" LIB_NAME "] const_pi(): precision must be >= 0");
    if (precision > 18)
        api->msrest_panic("[" LIB_NAME "] const_pi(): precision must be <= 18");

    const double Pi = 3.14159265358979323846264338327950288;
    double factor = pow(10.0, precision);
    double result = round(Pi * factor) / factor;

    return api->float_to_value(result);
}


EXPORT Value msrest_const_exp(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->lib_check_type(MSREST_INT, args[0].type, LIB_NAME, "exp");

    int precision = api->value_to_int(args[0]);

    if (precision < 0)
        api->msrest_panic("[" LIB_NAME "] const_exp(): precision must be >= 0");
    if (precision > 20)
        api->msrest_panic("[" LIB_NAME "] const_exp(): precision must be <= 20");

    const double E = 2.71828182845904523536028747135266249;
    double factor = pow(10.0, precision);
    double result = round(E * factor) / factor;

    return api->float_to_value(result);
}


EXPORT Value msrest_exp(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    double x = api->value_to_double(args[0]);
    if (!isfinite(x))
        api->msrest_panic("[" LIB_NAME "] exp(): non-finite argument");

    double r = exp(x);
    return api->float_to_value(finite_or_panic(api, r, "exp"));
}


EXPORT Value msrest_to_radians(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    double deg = api->value_to_double(args[0]);
    if (!isfinite(deg))
        api->msrest_panic("[" LIB_NAME "] to_radians(): non-finite argument");

    double r = deg * (M_PI / 180.0);
    return api->float_to_value(finite_or_panic(api, r, "to_radians"));
}


EXPORT Value msrest_to_degrees(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    double deg = api->value_to_double(args[0]);
    if (!isfinite(deg))
        api->msrest_panic("[" LIB_NAME "] to_degrees(): non-finite argument");

    double r = deg * (180.0 / M_PI);
    return api->float_to_value(finite_or_panic(api, r, "to_degrees"));
}


EXPORT Value msrest_sinus(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    double x = api->value_to_double(args[0]);
    if (!isfinite(x))
        api->msrest_panic("[" LIB_NAME "] sin(): non-finite argument");

    double r = sin(x);
    return api->float_to_value(finite_or_panic(api, r, "sin"));
}


EXPORT Value msrest_tang(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    double x = api->value_to_double(args[0]);
    if (!isfinite(x))
        api->msrest_panic("[" LIB_NAME "] tan(): non-finite argument");

    double r = tan(x);
    return api->float_to_value(finite_or_panic(api, r, "tan"));
}


EXPORT Value msrest_cosin(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    double x = api->value_to_double(args[0]);
    if (!isfinite(x))
        api->msrest_panic("[" LIB_NAME "] cos(): non-finite argument");

    double r = cos(x);
    return api->float_to_value(finite_or_panic(api, r, "cos"));
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

    check_finite_arg(api, args[0], "abs");
    double num = api->value_to_double(args[0]);

    double out = fabs(num);
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

    check_finite_arg(api, args[0], "max");
    check_finite_arg(api, args[1], "max");

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

    check_finite_arg(api, args[0], "min");
    check_finite_arg(api, args[1], "min");

    double a = api->value_to_double(args[0]);
    double b = api->value_to_double(args[1]);
    return api->float_to_value((a < b) ? a : b);
}


EXPORT Value msrest_clamp(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 3);
    Value min = args[0];
    Value num = args[1];
    Value max = args[2];
    check_numeric(api, min.type);
    check_numeric(api, num.type);
    check_numeric(api, max.type);

    uint8_t only_int = min.type == MSREST_INT && num.type == MSREST_INT && max.type == MSREST_INT;

    if (only_int) {
        int64_t a = min.integer;
        int64_t number = num.integer;
        int64_t b = max.integer;
        if (a > b)
            api->msrest_panic("[" LIB_NAME "] clamp(): minimum is bigger than maximum");
        int64_t r = (number > b) ? b : (number < a) ? a : number;
        return api->int_to_value(r);
    }

    check_finite_arg(api, min, "clamp");
    check_finite_arg(api, num, "clamp");
    check_finite_arg(api, max, "clamp");

    double a = api->value_to_double(min);
    double number = api->value_to_double(num);
    double b = api->value_to_double(max);

    if (a > b)
        api->msrest_panic("[" LIB_NAME "] clamp(): minimum is bigger than maximum");

    double result = (number > b) ? b : (number < a) ? a : number;
    return api->float_to_value(result);
}


EXPORT Value msrest_factorial(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->lib_check_type(MSREST_INT, args[0].type, LIB_NAME, "factorial");
    int64_t n = args[0].integer;

    if (n < 0)
        api->msrest_panic("[" LIB_NAME "] factorial(): negative argument");
    if (n > 20)
        api->msrest_panic("[" LIB_NAME "] factorial(): integer overflow");

    static const int64_t fact[21] = {
        1LL,                     // 0!
        1LL,                     // 1!
        2LL,                     // 2!
        6LL,                     // 3!
        24LL,                    // 4!
        120LL,                   // 5!
        720LL,                   // 6!
        5040LL,                  // 7!
        40320LL,                 // 8!
        362880LL,                // 9!
        3628800LL,               // 10!
        39916800LL,              // 11!
        479001600LL,             // 12!
        6227020800LL,            // 13!
        87178291200LL,           // 14!
        1307674368000LL,         // 15!
        20922789888000LL,        // 16!
        355687428096000LL,       // 17!
        6402373705728000LL,      // 18!
        121645100408832000LL,    // 19!
        2432902008176640000LL    // 20!
    };

    return api->int_to_value(fact[n]);
}


EXPORT Value msrest_sum_progression(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 3);
    api->lib_check_type(MSREST_INT, args[0].type, LIB_NAME, "sum_progression");
    api->lib_check_type(MSREST_INT, args[1].type, LIB_NAME, "sum_progression");
    api->lib_check_type(MSREST_INT, args[2].type, LIB_NAME, "sum_progression");

    int64_t a = args[0].integer;  // первый член
    int64_t step = args[1].integer;  // шаг
    int64_t n = args[2].integer;  // количество членов.

    if (n < 0) api->msrest_panic("[" LIB_NAME "] sum_progression(): n must be >= 0");
    if (n == 0) return api->int_to_value(0);

    // an = a + (n-1)*step - последний член.
    int64_t nm1, an, tmp;
    if (__builtin_sub_overflow(n, 1, &nm1) ||
        __builtin_mul_overflow(nm1, step, &tmp) ||
        __builtin_add_overflow(a, tmp, &an))
        api->msrest_panic("[" LIB_NAME "] sum_progression(): overflow computing last term");

    // s = a + an - сумма первого и последнего.
    int64_t s;
    if (__builtin_add_overflow(a, an, &s))
        api->msrest_panic("[" LIB_NAME "] sum_progression(): overflow computing a+an");

    // S = n * s / 2, делим чётный множитель до умножения.
    int64_t half, other;
    if ((n & 1) == 0)      { half = n / 2; other = s; }
    else if ((s & 1) == 0) { half = s / 2; other = n; }
    else                    api->msrest_panic("[" LIB_NAME "] sum_progression(): internal parity error");

    int64_t result;
    if (__builtin_mul_overflow(half, other, &result))
        api->msrest_panic("[" LIB_NAME "] sum_progression(): overflow computing sum");

    return api->int_to_value(result);
}

EXPORT Value msrest_round(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    // round от целого - это само целое.
    if (args[0].type == MSREST_INT)
        return args[0];

    double x = api->value_to_double(args[0]);

    if (!isfinite(x))
        api->msrest_panic("[" LIB_NAME "] round(): non-finite argument");

    return api->float_to_value(round(x));
}

EXPORT Value msrest_trunc(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    // trunc от целого - это само целое.
    if (args[0].type == MSREST_INT)
        return args[0];

    double x = api->value_to_double(args[0]);

    if (!isfinite(x))
        api->msrest_panic("[" LIB_NAME "] trunc(): non-finite argument");

    return api->float_to_value(trunc(x));
}


EXPORT Value msrest_floor(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    // floor от целого - это само целое.
    if (args[0].type == MSREST_INT)
        return args[0];

    double x = api->value_to_double(args[0]);

    if (!isfinite(x))
        api->msrest_panic("[" LIB_NAME "] floor(): non-finite argument");

    return api->float_to_value(floor(x));
}


EXPORT Value msrest_ceil(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    // ceil от целого - это само целое.
    if (args[0].type == MSREST_INT)
        return args[0];

    double x = api->value_to_double(args[0]);

    if (!isfinite(x))
        api->msrest_panic("[" LIB_NAME "] ceil(): non-finite argument");

    return api->float_to_value(ceil(x));
}


EXPORT Value msrest_frac(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    // frac от целого - 0
    if (args[0].type == MSREST_INT)
        return api->int_to_value(0);

    double x = api->value_to_double(args[0]);

    if (!isfinite(x))
        api->msrest_panic("[" LIB_NAME "] frac(): non-finite argument");

    return api->float_to_value(x - floor(x));
}


EXPORT Value msrest_sign(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    check_numeric(api, args[0].type);

    if (args[0].type == MSREST_INT) {
        if (args[0].integer > 0) {
            return api->int_to_value(1);
        }
        else if (args[0].integer == 0) {
            return api->int_to_value(0);
        }
        else { 
            return api->int_to_value(-1);
        }
    }

    double x = api->value_to_double(args[0]);

    if (!isfinite(x))
        api->msrest_panic("[" LIB_NAME "] sign(): non-finite argument");

    if (x < 0) {
        return api->int_to_value(-1);
    }
    else if (x == 0) {
        return api->int_to_value(0);
    }
    return api->int_to_value(1);
}


EXPORT Value msrest_is_even(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->lib_check_type(MSREST_INT, args[0].type, LIB_NAME, "is_even");

    int64_t number = api->value_to_int(args[0]);
    return api->bool_to_value(!(number & 1));
}

EXPORT Value msrest_is_odd(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->lib_check_type(MSREST_INT, args[0].type, LIB_NAME, "is_odd");

    int64_t number = api->value_to_int(args[0]);
    return api->bool_to_value((number & 1));
}


EXPORT Value msrest_fmod(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    check_numeric(api, args[0].type);
    check_numeric(api, args[1].type);

    double a = api->value_to_double(args[0]);
    double b = api->value_to_double(args[1]);

    if (!isfinite(a) || !isfinite(b))
        api->msrest_panic("[" LIB_NAME "] fmod(): non-finite argument");
    if (b == 0.0)
        api->msrest_panic("[" LIB_NAME "] fmod(): division by zero");

    return api->float_to_value(fmod(a, b));
}

