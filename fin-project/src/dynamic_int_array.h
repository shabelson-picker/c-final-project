#ifndef DYNAMIC_INT_ARRAY_H
#define DYNAMIC_INT_ARRAY_H

typedef struct {
    int *data;
    int  count;
    int  capacity;
} DynamicIntArray;

void dia_init(DynamicIntArray *a);
void dia_free(DynamicIntArray *a);
int  dia_append(DynamicIntArray *a, int value);
int  dia_remove(DynamicIntArray *a, int value);
int  dia_insert(DynamicIntArray *a, int index, int value);
int  dia_sort_insert(DynamicIntArray *a, int value);
int  dia_find_index(const DynamicIntArray *a, int value);

#endif /* DYNAMIC_INT_ARRAY_H */
