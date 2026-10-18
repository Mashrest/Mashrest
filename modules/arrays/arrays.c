#include "API/msrest_api.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>  
#include <math.h> 

#define LIB_NAME "ARRAYS"
#define SEARCH_NOT_FOUND -1
#define SEARCH_CONTLEN_ZERO -2
#define SEARCH_STUB -67



EXPORT uint32_t msrest_abi_version(void) {
    return MSREST_API_ABI_VERSION;
}


typedef struct {
    uint32_t left;
    uint32_t right;
} Partition;

typedef struct {
    double *numbers;
    char *types;
    uint32_t left;
    uint32_t right;
} Sort_setting;

typedef struct {
    double *numbers;
    char *types;
} StructOfArrays;




void repr(int64_t *ptr, size_t s) 
{
    for (int r = 0; r<s; r++) {
        printf("%d number: %lld\n", r, (long long)ptr[r]);
    }
}

void repr_fi(double *ptr, char *type, size_t s) 
{
    for (int r = 0; r<s; r++) {
        if (type[r]) { 
            printf("%d number: %f\n", r, (double)ptr[r]);
        }
        else {
            printf("%d number: %lld\n", r, (uint64_t)ptr[r]);
        }
    }
}

void repr_char(char *ptr, size_t s) 
{
    for (int r = 0; r<s; r++) {
        printf("%d number: %d\n", r, (char)ptr[r]);
    }
}


static inline int64_t min_num(int64_t num1, int64_t num2)
{
    return (num1 < num2) ? num1 : num2;
}

static void* safe_malloc(MsrestAPI *api, size_t size) {
    void* p = malloc(size);
    if (!p) api->msrest_panic("[" LIB_NAME "] memory allocation failed");
    return p;
}

/* Меняет два элемента с некими индексами местами */
static void swap_entries(double *numbers, char *types, int i, int j)
{
    if (__builtin_expect(i == j, 0)) return;
    
    double tmp_num = numbers[i];
    numbers[i] = numbers[j];
    numbers[j] = tmp_num;
    
    char tmp_type = types[i];
    types[i] = types[j];
    types[j] = tmp_type;
}



/* < ФУНКЦИИ-ХЕЛПЕРЫ > */


/* проходит по элементам массива, записывая их типы в malloc-массив */
/* INTEGER - 0, FLOAT - 1 */
static char *u_create_type_mask(MsrestAPI *api, Value array, char *funcname)
{
    uint32_t size = api->array_size(array);
    char *types = safe_malloc(api, (size_t)size * sizeof(char));

    // проверяем элементы массива.
    for (int64_t i = 0; i<(int64_t)size; i++) {
        Value v = api->array_get(array, i);

        switch(v.type) {
            case MSREST_INT: { 
                types[i] = 0; break;
            }
            case MSREST_FLOAT: { 
                types[i] = 1; break;
            }
            default: { 
                free(types);
                api->msrest_panic("[" LIB_NAME "] %s(): Cannot sort non-number value!\n", funcname);
            }
        }
    }
    return types;
}

static double *u_create_numbers_arr(MsrestAPI *api, Value array, char *types, uint32_t size)
{
    double *numbers = safe_malloc(api, size * sizeof(double));

    for (int64_t i = 0; i<size; i++) {
        Value intermediate = api->array_get(array, i);
        if (types[i]) {
            numbers[i] = api->value_to_double(intermediate);
        }
        else { 
            numbers[i] = api->value_to_int(intermediate);
        }
    }
    return numbers;
}




static void u_bubble_sort(const Sort_setting *sort)
{
    uint32_t right = sort->right;
    char *types = sort->types;
    double *numbers = sort->numbers;

    for (int64_t s = 0; s < right; s++) {
        uint8_t swapped = 0;
        for (int64_t j = 0; j < right - s; j++) {
            if (numbers[j] > numbers[j+1]) {
                swap_entries(numbers, types, j+1, j);
                swapped = 1;
            }
        }
        if (!swapped) { break; }
    }
}

EXPORT Value bubble_sorted(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->lib_check_type(MSREST_ARRAY, args[0].type, LIB_NAME, "bubble_sorted");

    uint32_t size = api->array_size(args[0]);
    if (size == 0) return api->null_to_value();

    char *types = u_create_type_mask(api, args[0], "bubble_sorted");
    double *numbers = u_create_numbers_arr(api, args[0], types, size);
    Sort_setting sort = {.numbers = numbers, .types = types, .left = 0, .right = size - 1};
    u_bubble_sort(&sort);

    /* Создаем массив, получаем значение-держатель */
    Value output = api->array_new(size);

    /* Упаковываем числа в нужные ячейки массива */
    for (int64_t i = 0; i< (int64_t)size; i++) {
        if (types[i]) {
            api->array_set(output, i, api->float_to_value(numbers[i]));
        }
        else {
            api->array_set(output, i, api->int_to_value((intptr_t)numbers[i]));
        }
    }

    free(numbers);
    free(types);

    return output;
}


EXPORT Value bubble_sort(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->lib_check_type(MSREST_ARRAY, args[0].type, LIB_NAME, "bubble_sort");
    Value container =args[0];

    uint32_t size = api->array_size(container);
    if (size == 0) return api->null_to_value();

    char *types = u_create_type_mask(api, container, "bubble_sort");
    double *numbers = u_create_numbers_arr(api, container, types, size);
    Sort_setting sort = {.numbers = numbers, .types = types, .left = 0, .right = size - 1};
    u_bubble_sort(&sort);

    /* Упаковываем числа в нужные ячейки массива */
    for (int64_t i = 0; i<(int64_t)size; i++) {
        if (types[i]) {
            api->array_set(container, i, api->float_to_value(numbers[i]));
        }
        else {
            api->array_set(container, i, api->int_to_value((intptr_t)numbers[i]));
        }
    }

    free(numbers);
    free(types);

    return container;
}


void insertion_sort_ptr(const Sort_setting *sort)
{
    uint32_t lo = sort->left;
    uint32_t hi = sort->right;
    double *numbers = sort->numbers;
    char *types = sort->types;

    if (lo >= hi) return;
    
    for (int64_t i = lo + 1; i <= hi; i++) {
        double key_num = numbers[i];
        char key_type = types[i];
        
        int64_t j = i - 1;
        while (j >= lo && numbers[j] > key_num) {
            numbers[j + 1] = numbers[j];
            types[j + 1] = types[j];
            j--;
        }
        
        numbers[j + 1] = key_num;
        types[j + 1] = key_type;
    }
}

EXPORT Value insertion_sorted(MsrestAPI *api, Value *args, int argc) 
{
    api->check_argc(argc, 1);
    api->lib_check_type(MSREST_ARRAY, args[0].type, LIB_NAME, "insertion_sorted");

    Value container = args[0];
    uint32_t size = api->array_size(container);
    if (size == 0) return api->null_to_value();

    char *types = u_create_type_mask(api, container, "insertion_sorted");
    double *numbers = u_create_numbers_arr(api, container, types, size);

    Sort_setting sort = {.numbers = numbers, .types = types, .left = 0, .right = size - 1};
    insertion_sort_ptr(&sort);
    Value output = api->array_new(size);

    /* упаковываем числа в нужные ячейки массива. */
    for (int64_t i = 0; i < (uint32_t)size; i++) {
        if (types[i]) {
            api->array_set(output, i, api->float_to_value(numbers[i]));
        }
        else {
            api->array_set(output, i, api->int_to_value((intptr_t)numbers[i]));
        }
    }

    free(numbers);
    free(types);

    return output;
}



static double median_of_three(double *numbers, uint32_t a, uint32_t b, uint32_t c)
{
    double x = numbers[a];
    double y = numbers[b];
    double z = numbers[c];
    
    // если x между y и z.
    if ((x >= y && x <= z) || (x <= y && x >= z))
        return x;
    
    // если y между x и z.
    if ((y >= x && y <= z) || (y <= x && y >= z))
        return y;
    
    // иначе z.
    return z;
}


void heapify(StructOfArrays *soa, int lo, int i, int n)
{
    double *numbers = soa->numbers;
    char *types = soa->types;

    int largest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;
    
    if (left < n && numbers[lo + left] > numbers[lo + largest]) {
        largest = left;
    }
    if (right < n && numbers[lo + right] > numbers[lo + largest]) {
        largest = right;
    }
    
    if (largest != i) {
        swap_entries(numbers, types, lo + i, lo + largest);
        heapify(soa, lo, largest, n);
    }
}

void heap_sort_ptr(Sort_setting *sort)
{
    uint32_t lo = sort->left;
    uint32_t hi = sort->right;
    double *numbers = sort->numbers;
    char *types = sort->types;

    // размер кучи.
    int64_t n = hi - lo + 1;
    
    // построение кучи.
    StructOfArrays osoa = {.numbers = numbers, .types = types};
    for (int64_t i = n / 2 - 1; i >= 0; i--) {
        heapify(&osoa, lo, i, n);
    }
    
    // извлечение элементов.
    for (int64_t i = n - 1; i > 0; i--) {
        swap_entries(numbers, types, lo, lo + i);
        heapify(&osoa, lo, 0, i);
    }
}


Partition three_way_partition(StructOfArrays *soa, int lo, int hi, double pivot)
{
    int i = lo;
    int j = lo;
    int k = hi;
    double *numbers = soa->numbers;
    char *types = soa->types;
    
    while (j <= k) {
        if (numbers[j] < pivot) {
            swap_entries(numbers, types, i, j);
            i++;
            j++;
        } else if (numbers[j] > pivot) {
            swap_entries(numbers, types, j, k);
            k--;
        } else {
            j++;
        }
    }
    
    Partition result;
    result.left = i - 1;
    result.right = k + 1;
    return result;
}

static double u_adaptive_pivot(double *numbers, int64_t lo, int64_t hi)
{
    // медиана из 5 элементов для лучшего разбиения.
    int64_t mid = lo + (hi - lo) / 2;
    int64_t q1 = lo + (hi - lo) / 4;
    int64_t q3 = lo + 3 * (hi - lo) / 4;
    
    // простая медиана из 5.
    // возвращаем медиану из 5 элементов
    double a = numbers[lo];
    double b = numbers[q1];
    double c = numbers[mid];
    double d = numbers[q3];
    double e = numbers[hi];
    
    // для простоты - сортируем 5 элементов и берем средний.
    double arr[5] = {a, b, c, d, e};
    for (int64_t i = 0; i < 4; i++) {
        for (int64_t j = i + 1; j < 5; j++) {
            if (arr[i] > arr[j]) {
                double tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        }
    }
    return arr[2];
}





/* DEBUG ONLY */
EXPORT Value test_partition(MsrestAPI *api, Value *args, int argc)
{
    (void)args;
    (void)argc;
    
    // тестовые данные.
    double numbers[] = {5.0, 2.0, 8.0, 1.0, 9.0, 3.0};
    char types[] = {0, 0, 0, 0, 0, 0};
    int size = 6;
    
    // выбор pivot.
    int lo = 0;
    int hi = size - 1;
    int mid = (lo + hi) / 2;
    double pivot = median_of_three(numbers, lo, mid, hi);
    
    StructOfArrays osoa = {.numbers = numbers, .types = types};
    Partition p = three_way_partition(&osoa, lo, hi, pivot);
    
    for (int i = 0; i < size; i++) {
        printf("%f ", numbers[i]);
    }

    Value output = api->array_new(size);
    for (int i = 0; i < size; i++) {
        api->array_set(output, i, api->float_to_value(numbers[i]));
    }
    
    return output;
}




void pdq_recursive(StructOfArrays *soa, int lo, int hi, int depth_limit, int bad_count)
{
    double *numbers = soa->numbers;
    char *types = soa->types;
    // базовый случай: маленький массив.
    if (hi - lo + 1 <= 24) {
        Sort_setting sort = {.numbers = numbers, .types = types, .left = lo, .right = hi};
        insertion_sort_ptr(&sort);
        return;
    }
    
    // страховка: глубина исчерпана.
    if (depth_limit == 0) {
        Sort_setting sort = {.numbers = numbers, .types = types, .left = lo, .right = hi};
        heap_sort_ptr(&sort);
        return;
    }
    
    // выбор pivot (адаптивный, если были плохие разбиения).
    double pivot;
    if (bad_count > 0) {
        pivot = u_adaptive_pivot(numbers, lo, hi);
        bad_count--;  // уменьшаем счетчик плохих разбиений.
    } else {
        int64_t mid = lo + (hi - lo) / 2;
        pivot = median_of_three(numbers, lo, mid, hi);
    }
    
    // разбиение.
    Partition p = three_way_partition(soa, lo, hi, pivot);
    
    // анализ качества разбиения.
    int left_size = p.left - lo + 1;
    int right_size = hi - p.right + 1;
    
    // проверяем дисбаланс.
    if (left_size > 2 * right_size || right_size > 2 * left_size) bad_count += 2;
    
    
    // рекурсивные вызовы.
    pdq_recursive(soa, lo, p.left, depth_limit - 1, bad_count);
    pdq_recursive(soa, p.right, hi, depth_limit - 1, bad_count);
}




EXPORT Value pdq_sorted(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->lib_check_type(MSREST_ARRAY, args[0].type, LIB_NAME, "pdq_sorted");
    Value container = args[0];

    uint32_t size = api->array_size(container);
    if (size == 0) return api->null_to_value();

    char *types = u_create_type_mask(api, container, "pdq_sorted");
    double *numbers = u_create_numbers_arr(api, container, types, size);

    // вычисляем максимальную глубину рекурсии.
    int depth_limit = 2 * (int)(log2((double)size)) + 1;

    // ВЫЗЫВАЕМ PDQ С ГЛУБИНОЙ.
    int bad_count = 0; 
    StructOfArrays soa = {.numbers = numbers, .types = types};
    pdq_recursive(&soa, 0, size - 1, depth_limit, bad_count);

    Value output = api->array_new(size);
    for (int64_t i = 0; i<(int64_t)size; i++) {
        if (types[i]) {
            api->array_set(output, i, api->float_to_value(numbers[i]));
        }
        else {
            api->array_set(output, i, api->int_to_value((intptr_t)numbers[i]));
        }
    }

    free(numbers);
    free(types);

    return output;
}


EXPORT Value array_max(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    Value container = args[0];
    Value def = args[1];
    uint32_t arr_len = api->array_size(container);

    if (!arr_len) return def;

    double best_num = 0.0;
    char best_is_float = 0;
    int have_best = 0;

    for (uint32_t i = 0; i < arr_len; i++) {
        Value cur = api->array_get(container, i);
        double num;
        char is_float;

        switch (cur.type) {
            case MSREST_FLOAT:
                num = cur.floating;
                is_float = 1;
                break;
            case MSREST_INT:
                num = (double)cur.integer;
                is_float = 0;
                break;
            default:
                api->msrest_panic(
                    "[" LIB_NAME "] array_max(): expected FLOAT or INTEGER, got %s",
                    api->type_name(cur.type)
                );
        }

        if (!have_best || num > best_num) {
            best_num = num;
            best_is_float = is_float;
            have_best = 1;
        }
    }

    if (best_is_float) {
        return api->float_to_value(best_num);
    }
    return api->int_to_value((int64_t)best_num);
}


EXPORT Value array_min(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    Value container = args[0];
    Value def = args[1];
    uint32_t arr_len = api->array_size(container);

    if (!arr_len) return def;

    double best_num = 0.0;
    char best_is_float = 0;
    int have_best = 0;

    for (uint32_t i = 0; i < arr_len; i++) {
        Value cur = api->array_get(container, i);
        double num;
        char is_float;

        switch (cur.type) {
            case MSREST_FLOAT:
                num = cur.floating;
                is_float = 1;
                break;
            case MSREST_INT:
                num = (double)cur.integer;
                is_float = 0;
                break;
            default:
                api->msrest_panic(
                    "[" LIB_NAME "] array_max(): expected FLOAT or INTEGER, got %s",
                    api->type_name(cur.type)
                );
        }

        if (!have_best || num < best_num) {
            best_num = num;
            best_is_float = is_float;
            have_best = 1;
        }
    }

    if (best_is_float) {
        return api->float_to_value(best_num);
    }
    return api->int_to_value((int64_t)best_num);
}



/* < ФУНКЦИИ ПОИСКА В МАССИВАХ > */

typedef struct {
    Value container;
    Value element;
    uint32_t left;
    uint32_t right;
} Search_setting;

static int64_t u_binary_search(MsrestAPI *api, const Search_setting *s)
{
    double el = api->value_to_double(s->element);
    int64_t right = s->right;
    int64_t left = s->left;

    while (left <= right) {
        int64_t mid = (left + right) / 2;
        Value mid_val = api->array_get(s->container, (uint32_t)mid);
        double d = api->value_to_double(mid_val);
            
        if (d == el) {
            return mid;
        }
        else if (d < el) {
            left = mid + 1;
        }
        else {
            right = mid - 1;
        }
    }
    return SEARCH_NOT_FOUND;
}


EXPORT Value binary_search(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    Value container = args[0];
    Value element = args[1];
    uint8_t element_type = args[1].type;
    api->lib_check_type(MSREST_ARRAY, args[0].type, LIB_NAME, "binary_search");

    if (!api->soft_check_type(MSREST_INT, element_type) &&
        !api->soft_check_type(MSREST_FLOAT, element_type)) {
            api->msrest_panic("[" LIB_NAME "] binary_search(): unsupported value type %s", api->type_name(args[1].type));
        }
    
    uint32_t arr_len = api->array_size(container);
    if (arr_len == 0) return api->null_to_value();

    Search_setting set = {.container = container, .element = element, .left = 0, .right = arr_len - 1};
    int64_t val_idx = u_binary_search(api, &set);

    if (val_idx == SEARCH_NOT_FOUND) return api->null_to_value();
    return api->int_to_value(val_idx);
}



static int64_t u_exponential_search(MsrestAPI *api, const Search_setting *s)
{
    Value container = s->container;
    uint32_t arr_len = s->right;
    Value element = s->element;

    double srch_num = api->value_to_double(s->element);
    int64_t current_idx = 1;
    double cur_num = api->value_to_double(api->array_get(container, (uint32_t)current_idx));

    if (api->value_to_double(api->array_get(container, 0)) == srch_num)
        return 0;

    while (current_idx < (int64_t)arr_len && cur_num < srch_num) {
        current_idx *= 2;
        if (current_idx >= (int64_t)arr_len) break;
        cur_num = api->value_to_double(api->array_get(container, current_idx));
    }

    uint32_t left = current_idx / 2;
    uint32_t right = min_num(current_idx, (int64_t)arr_len - 1);

    Search_setting set = {.container = container, .element = element, .left = left, .right = right};
    int64_t idx = u_binary_search(api, &set);
    if (idx == SEARCH_NOT_FOUND) return SEARCH_NOT_FOUND;
    return idx;
}


EXPORT Value exponential_search(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    Value container = args[0];
    Value element = args[1];
    uint8_t element_type = args[1].type;
    api->lib_check_type(MSREST_ARRAY, args[0].type, LIB_NAME, "exponential_search");

    uint32_t arr_len = api->array_size(container);
    if (arr_len == 0) return api->null_to_value();

    if (!api->soft_check_type(MSREST_INT, element_type) &&
        !api->soft_check_type(MSREST_FLOAT, element_type)) {
            api->msrest_panic("[" LIB_NAME "] exponential_search(): unsupported value type %s", api->type_name(element_type));
        }

    Search_setting set = {.container = container, .element = element, .left = 0, .right = arr_len - 1};
    int64_t idx = u_exponential_search(api, &set);
    if (idx == SEARCH_NOT_FOUND) return api->null_to_value();
    return api->int_to_value(idx);
}

EXPORT Value linear_search(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    Value container = args[0];
    Value element = args[1];
    api->lib_check_type(MSREST_ARRAY, args[0].type, LIB_NAME, "linear_search");

    uint32_t arr_len = api->array_size(container);
    if (arr_len == 0) return api->null_to_value();

    for (uint32_t i = 0; i < arr_len; i++) {
        Value cur = api->array_get(container, i);
        if (api->value_equals(cur, element)) {
            return api->int_to_value((int64_t)i);
        }
    }
    return api->null_to_value();
}


static int64_t u_jump_search(MsrestAPI *api, const Search_setting *s)
{
    uint32_t arr_len = s->right;
    double el = api->value_to_double(s->element);
    uint32_t step = sqrt((double)arr_len);
    if (step < 1) step = 1;

    uint32_t prev = 0;
    uint32_t curr = step;

    while (curr < arr_len && (api->value_to_double(api->array_get(s->container, curr))) < el) {
        prev = curr;
        curr += step;
    }

    uint32_t end = (curr < arr_len) ? curr : arr_len - 1;

    for (uint32_t i = prev; i <= end; i++) {
        if (api->value_to_double(api->array_get(s->container, i)) == el) {
            return i;
        }
    }
    return SEARCH_NOT_FOUND;
}

EXPORT Value jump_search(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    Value container = args[0];
    Value element = args[1];
    uint8_t element_type = args[1].type;

    api->lib_check_type(MSREST_ARRAY, args[0].type, LIB_NAME, "jump_search");
    uint32_t arr_len = api->array_size(container);
    if (arr_len == 0) return api->null_to_value();

    if (!api->soft_check_type(MSREST_INT, element_type) &&
        !api->soft_check_type(MSREST_FLOAT, element_type)) {
            api->msrest_panic("[" LIB_NAME "] jump_search(): unsupported value type %s", api->type_name(element_type));
        }

    Search_setting set = {.container = container, .element = element, .left = 0, .right = arr_len - 1};
    int64_t idx = u_jump_search(api, &set);

    if (idx == SEARCH_NOT_FOUND) return api->null_to_value();
    return api->int_to_value(idx);
}

#define LOWER_BOUND 0
#define UPPER_BOUND 1

static int64_t u_bound(MsrestAPI *api, const Search_setting *s, char mode)
{
    double el = api->value_to_double(s->element);
    uint32_t left = s->left;
    uint32_t right = s->right;

    switch (mode) {
        case UPPER_BOUND: {
            while (left < right) {
                int64_t mid = (left + right) / 2;
                Value imm = api->array_get(s->container, (uint32_t)mid);
                if (__builtin_expect(!(imm.type == MSREST_FLOAT || imm.type == MSREST_INT), 0)) {
                    api->msrest_panic("[" LIB_NAME "] upper_bound(): Cannot check non-integer values");
                }
                if (api->value_to_double(imm) <= el) {
                    left = mid + 1;
                }
                else {
                    right = mid;
                }
            }
            return left;
        }
        case LOWER_BOUND: {
            while (left < right) {
                int64_t mid = (left + right) / 2;
                Value imm = api->array_get(s->container, (uint32_t)mid);
                if (__builtin_expect(!(imm.type == MSREST_FLOAT || imm.type == MSREST_INT), 0)) {
                    api->msrest_panic("[" LIB_NAME "] lower_bound(): Cannot check non-integer values");
                }
                if (api->value_to_double(imm) < el) {
                    left = mid + 1;
                }
                else {
                    right = mid;
                }
            }
            return left;
        }
        default: {
            // по идее __noreturn__, но компилятор может злиться на отсутствие возврата.
            api->msrest_panic("[" LIB_NAME "] Internal library error (bound-function)");
            return SEARCH_STUB;
        }
    }
}

EXPORT Value upper_bound(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    Value container = args[0];
    Value element = args[1];
    api->lib_check_type(MSREST_ARRAY, args[0].type, LIB_NAME, "upper_bound");

    uint32_t arr_len = api->array_size(container);
    if (arr_len == 0) return api->int_to_value(0);

    uint8_t element_type = args[1].type;
    if (!api->soft_check_type(MSREST_INT, element_type) &&
        !api->soft_check_type(MSREST_FLOAT, element_type)) {
        api->msrest_panic("[" LIB_NAME "] upper_bound(): unsupported value type %s",
                        api->type_name(element_type));
    }

    Search_setting set = {.container = container, .element = element, .left = 0, .right = arr_len};
    int64_t idx = u_bound(api, &set, UPPER_BOUND);
    return api->int_to_value(idx);
}

EXPORT Value lower_bound(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 2);
    Value container = args[0];
    Value element = args[1];
    api->lib_check_type(MSREST_ARRAY, args[0].type, LIB_NAME, "lower_bound");

    uint32_t arr_len = api->array_size(container);
    if (arr_len == 0) return api->int_to_value(0);

    uint8_t element_type = args[1].type;
    if (!api->soft_check_type(MSREST_INT, element_type) &&
        !api->soft_check_type(MSREST_FLOAT, element_type)) {
        api->msrest_panic("[" LIB_NAME "] lower_bound(): unsupported value type %s",
                        api->type_name(element_type));
    }

    Search_setting set = {.container = container, .element = element, .left = 0, .right = arr_len};
    int64_t idx = u_bound(api, &set, LOWER_BOUND);
    return api->int_to_value(idx);
}

#undef UPPER_BOUND
#undef LOWER_BOUND

EXPORT Value array_sum(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->lib_check_type(MSREST_ARRAY, args[0].type, LIB_NAME, "array_sum");
    Value container = args[0];

    uint32_t arr_len = api->array_size(container);
    if (arr_len == 0) return api->int_to_value(0);

    double sum = 0;
    double max_accuracy = ldexp(1.0, 53);

    for (int64_t i = 0; i < (int64_t)arr_len; i++) {
        Value cur_val = api->array_get(container, (uint32_t)i);
        if (cur_val.type != MSREST_INT && cur_val.type != MSREST_FLOAT) {
            api->msrest_panic("[" LIB_NAME "] array_sum(): unsupported type %s", api->type_name(cur_val.type));
        }
        double cur = api->value_to_double(cur_val);
        double add = sum + cur;
        if ((add > max_accuracy) || !isfinite(add)) {
            api->msrest_panic("[" LIB_NAME "] array_sum(): got overflow in answer");
        }
        else {
            sum = add;
        }
    }

    return api->float_to_value(sum);
}