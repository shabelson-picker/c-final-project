#include <stdlib.h>
#include "dynamic_int_array.h"

void dia_init(DynamicIntArray *a) {
    a->data     = NULL;
    a->count    = 0;
    a->capacity = 0;
}

void dia_free(DynamicIntArray *a) {
    free(a->data);
    a->data     = NULL;
    a->count    = 0;
    a->capacity = 0;
}

int dia_append(DynamicIntArray *a, int value) {
    if (a->count == a->capacity) {
        int new_cap = (a->capacity == 0) ? 1 : a->capacity * 2;
        int *tmp = (int *)realloc(a->data, new_cap * sizeof(int));
        if (!tmp) return 0;
        a->data     = tmp;
        a->capacity = new_cap;
    }
    a->data[a->count++] = value;
    return 1;
}
/* qsort/bsearch comparator for ints (ascending). File-local. */
static int cmp(const void* a, const void* b)
{
    int* val_a = (int*)(a);
    int* val_b = (int*)(b);
    return *val_a - *val_b;
}

/// <summary>
/// returns index of value, -1 if doesn't exist
/// </summary>
/// <param name="a"></param>
/// <param name="value"></param>
/// <returns></returns>
int  dia_find_index(const DynamicIntArray* a, int value)
{
    int *fnd_ptr = bsearch(&value, a->data, (size_t)a->count, sizeof(value), cmp);
    if (fnd_ptr == NULL) return -1;
    return (int)(fnd_ptr - a->data);
}
int dia_remove(DynamicIntArray *a, int value) {
    int i;
    for (i = 0; i < a->count; i++) {
        if (a->data[i] == value) {
            a->data[i] = a->data[--a->count];
            return 1;
        }
    }
    return 0;
}

/* Order-preserving remove for a sorted array: shift the tail down so the
 * array stays sorted (keeps dia_find_index / bsearch valid). */
int dia_sort_remove(DynamicIntArray *a, int value) {
    int i = dia_find_index(a, value);
    if (i == -1) return 0;
    for (; i < a->count - 1; i++)
        a->data[i] = a->data[i + 1];
    a->count--;
    return 1;
}


/* Insert value at index, shifting the tail up. Falls back to append for an
 * out-of-range index or an empty array. Returns 1 on success, 0 on failure. */
int dia_insert(DynamicIntArray* a,int index, int value)
{
    if (index < 0) return 0;
    if (index > a->count || a->count ==0)
    {
        
        return dia_append(a, value);
    }
    if (!dia_append(a, value)) return 0;
    for (int i = a->count - 1;i > index;i--) 
    {
        a->data[i] = a->data[i - 1];
    }
    a->data[index] = value;
    return 1;
    
}
/// <summary>
/// under the assumption of a pre-sorted array - insert the value to its sorted place.
/// </summary>
/// <param name="a"></param>
/// <param name="value"></param>
/// <returns></returns>
int dia_sort_insert(DynamicIntArray* a, int value)
{

    if (a->count == 0) {
        dia_append(a, value);
        return 1;
    }
    if (a->data[a->count - 1] < value)
    {
        dia_append(a, value);
        return 1;
    }
    for (int i = 0;i < a->count;i++)
    {
        if (value < a->data[i])
        {
            dia_insert(a, i, value);
            return 1;
        }

    }
    return 0;

}
