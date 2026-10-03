#include "API/msrest_api.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>  
#include <math.h> 


typedef struct {
    int left;
    int right;
} Partition;

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

void swap_elements(double *numbers, char *types, int j) {
    // меняем числа.
    double temp_num = numbers[j];
    numbers[j] = numbers[j+1];
    numbers[j+1] = temp_num;
    
    // меняем типы.
    char temp_type = types[j];
    types[j] = types[j+1];
    types[j+1] = temp_type;
}

void swap_ints(int64_t *i, int j)
{
    int64_t temp = i[j];
    i[j] = i[j+1];
    i[j+1] = temp;
}

EXPORT Value bubble_sort_int(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->check_type(MSREST_ARRAY, args[0].type);

    uint32_t size = api->array_size(args[0]);

    /* Первая проверка на числовые типы */
    for (int i = 0; i<size; i++) {
        Value v = api->array_get(args[0], i);
        if (!(api->value_type(v) == MSREST_INT)) {
            api->msrest_panic("Cannot sort non-number values!\n");
        }
    }

    /* Аллоцируем память на нужное количество чистых чисел без обертки */
    /* Мы знаем, что тут только числа, float будет позже */
    int64_t *integers = malloc(size * sizeof(int64_t));

    /* Распаковываем по адресу в массиве */
    for (int i = 0; i<size; i++) {
        integers[i] = api->value_to_int(api->array_get(args[0], i));
    }

    /* Сортируем внутри числового массива, круто */
    for (int s = 0; s<size-1; s++) {
        uint8_t swapped = 0;
        for (int j = 0; j < size - 1 - s; j++) {
            if (integers[j] > integers[j+1]) {
                swap_ints(integers, j);
                swapped = 1;
            }
        }
        if (swapped == 0) {
            break;
        }
    }

    /* Создаем массив, получаем значение-держатель */
    Value output = api->array_new(size);

    /* Упаковываем числа в нужные ячейки массива */
    for (int i = 0; i<size; i++) {
        Value num = api->int_to_value(integers[i]);
        api->array_set(output, i, num);
    }

    free(integers);
    return output;
}

EXPORT Value bubble_sort(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->check_type(MSREST_ARRAY, args[0].type);

    uint32_t size = api->array_size(args[0]);

    char *types = malloc((size_t)size * sizeof(char));
    if (types == NULL) { api->msrest_panic("Cannot allocate memory! \n");}

    for (int i = 0; i<size; i++) {
        Value v = api->array_get(args[0], i);

        switch(api->value_type(v)) {
            case MSREST_INT: { types[i] = 0; break;
            }
            case MSREST_FLOAT: { types[i] = 1; break;
            }
            default: { 
                free(types);
                api->msrest_panic("Cannot sort non-number value!\n");
            }
        }
    }

    double *numbers = malloc(size * sizeof(double));

    for (int i = 0; i<size; i++) {
        Value intermediate = api->array_get(args[0], i);
        if (types[i]) {
            numbers[i] = api->value_to_double(intermediate);
        }
        else { 
            numbers[i] = api->value_to_int(intermediate);
        }
    }

    for (int s = 0; s<size-1; s++) {
        uint8_t swapped = 0;
        for (int j = 0; j < size - 1 - s; j++) {
            if (numbers[j] > numbers[j+1]) {
                swap_elements(numbers, types, j);
                swapped = 1;
            }
        }
        if (!swapped) { break; }
    }

    /* Создаем массив, получаем значение-держатель */
    Value output = api->array_new(size);

    /* Упаковываем числа в нужные ячейки массива */
    for (int i = 0; i<size; i++) {
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

void insertion_sort_ptr(double *numbers, char *types, int lo, int hi)
{
    if (lo >= hi) return;
    
    for (int i = lo + 1; i <= hi; i++) {
        double key_num = numbers[i];
        char key_type = types[i];
        
        int j = i - 1;
        while (j >= lo && numbers[j] > key_num) {
            numbers[j + 1] = numbers[j];
            types[j + 1] = types[j];
            j--;
        }
        
        numbers[j + 1] = key_num;
        types[j + 1] = key_type;
    }
}

EXPORT Value insertion_sort(MsrestAPI *api, Value *args, int argc) 
{
    api->check_argc(argc, 1);
    api->check_type(MSREST_ARRAY, args[0].type);

    uint32_t size = api->array_size(args[0]);
    char *types = malloc((size_t)size * sizeof(char));
    if (types == NULL) { api->msrest_panic("Cannot allocate memory! \n");}

    for (int i = 0; i<size; i++) {
        Value v = api->array_get(args[0], i);

        switch(api->value_type(v)) {
            case MSREST_INT: { types[i] = 0; break;
            }
            case MSREST_FLOAT: { types[i] = 1; break;
            }
            default: { 
                free(types);
                api->msrest_panic("Cannot sort non-number value!\n");
            }
        }
    }


    double *numbers = malloc(size * sizeof(double));

    for (int i = 0; i<size; i++) {
        Value intermediate = api->array_get(args[0], i);
        if (types[i]) {
            numbers[i] = api->value_to_double(intermediate);
        }
        else { 
            numbers[i] = api->value_to_int(intermediate);
        }
    }

    insertion_sort_ptr(numbers, types, 0, size - 1);  // прямая функция с указателями.

    /* создаем массив, получаем значение-держатель. */
    Value output = api->array_new(size);

    /* упаковываем числа в нужные ячейки массива. */
    for (int i = 0; i<size; i++) {
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



double median_of_three(double *numbers, uint32_t a, uint32_t b, uint32_t c)
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

void swap_entries(double *numbers, char *types, int i, int j)
{
    if (i == j) return;
    
    double tmp_num = numbers[i];
    numbers[i] = numbers[j];
    numbers[j] = tmp_num;
    
    char tmp_type = types[i];
    types[i] = types[j];
    types[j] = tmp_type;
}

void heapify(double *numbers, char *types, int lo, int i, int n)
{
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
        heapify(numbers, types, lo, largest, n);
    }
}

void heap_sort_ptr(double *numbers, char *types, int lo, int hi)
{
    // размер кучи.
    int n = hi - lo + 1;
    
    // построение кучи.
    for (int i = n / 2 - 1; i >= 0; i--) {
        heapify(numbers, types, lo, i, n);
    }
    
    // извлечение элементов.
    for (int i = n - 1; i > 0; i--) {
        swap_entries(numbers, types, lo, lo + i);
        heapify(numbers, types, lo, 0, i);
    }
}


Partition three_way_partition(double *numbers, char *types, int lo, int hi, double pivot)
{
    int i = lo;
    int j = lo;
    int k = hi;
    
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

double adaptive_pivot(double *numbers, int lo, int hi)
{
    // медиана из 5 элементов для лучшего разбиения.
    int mid = lo + (hi - lo) / 2;
    int q1 = lo + (hi - lo) / 4;
    int q3 = lo + 3 * (hi - lo) / 4;
    
    // простая медиана из 5.
    // возвращаем медиану из 5 элементов
    double a = numbers[lo];
    double b = numbers[q1];
    double c = numbers[mid];
    double d = numbers[q3];
    double e = numbers[hi];
    
    // для простоты - сортируем 5 элементов и берем средний.
    double arr[5] = {a, b, c, d, e};
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 5; j++) {
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
    
    Partition p = three_way_partition(numbers, types, lo, hi, pivot);
    
    for (int i = 0; i < size; i++) {
        printf("%f ", numbers[i]);
    }

    Value output = api->array_new(size);
    for (int i = 0; i < size; i++) {
        api->array_set(output, i, api->float_to_value(numbers[i]));
    }
    
    return output;
}

void pdq_recursive(double *numbers, char *types, int lo, int hi, int depth_limit, int bad_count)
{
    // базовый случай: маленький массив.
    if (hi - lo + 1 <= 24) {
        insertion_sort_ptr(numbers, types, lo, hi);
        return;
    }
    
    // страховка: глубина исчерпана.
    if (depth_limit == 0) {
        heap_sort_ptr(numbers, types, lo, hi);
        return;
    }
    
    // выбор pivot (адаптивный, если были плохие разбиения).
    double pivot;
    if (bad_count > 0) {
        pivot = adaptive_pivot(numbers, lo, hi);
        bad_count--;  // уменьшаем счетчик плохих разбиений.
    } else {
        int mid = lo + (hi - lo) / 2;
        pivot = median_of_three(numbers, lo, mid, hi);
    }
    
    // разбиение.
    Partition p = three_way_partition(numbers, types, lo, hi, pivot);
    
    // анализ качества разбиения.
    int left_size = p.left - lo + 1;
    int right_size = hi - p.right + 1;
    
    // проверяем дисбаланс.
    if (left_size > 2 * right_size || right_size > 2 * left_size) bad_count += 2;
    
    
    // рекурсивные вызовы.
    pdq_recursive(numbers, types, lo, p.left, depth_limit - 1, bad_count);
    pdq_recursive(numbers, types, p.right, hi, depth_limit - 1, bad_count);
}



EXPORT Value pdq_sort(MsrestAPI *api, Value *args, int argc)
{
    api->check_argc(argc, 1);
    api->check_type(MSREST_ARRAY, args[0].type);

    uint32_t size = api->array_size(args[0]);
    char *types = malloc((size_t)size * sizeof(char));

    if (types == NULL)
        api->msrest_panic("[ARRAYS] Cannot allocate memory! \n");

    // проверяем элементы массива.
    for (int i = 0; i<size; i++) {
        Value v = api->array_get(args[0], i);

        switch(api->value_type(v)) {
            case MSREST_INT: { 
                types[i] = 0; break;
            }
            case MSREST_FLOAT: { 
                types[i] = 1; break;
            }
            default: { 
                free(types);
                api->msrest_panic("[ARRAYS] Cannot sort non-number value!\n");
            }
        }
    }

    double *numbers = malloc(size * sizeof(double));

    for (int i = 0; i<size; i++) {
        Value intermediate = api->array_get(args[0], i);
        if (types[i]) {
            numbers[i] = api->value_to_double(intermediate);
        }
        else { 
            numbers[i] = api->value_to_int(intermediate);
        }
    }

    // вычисляем глубину рекурсии.
    int depth_limit = 2 * (int)(log2((double)size)) + 1;

    // ВЫЗЫВАЕМ PDQ С ГЛУБИНОЙ.
    int bad_count = 0; 
    pdq_recursive(numbers, types, 0, size - 1, depth_limit, bad_count);

    Value output = api->array_new(size);
    for (int i = 0; i<size; i++) {
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
