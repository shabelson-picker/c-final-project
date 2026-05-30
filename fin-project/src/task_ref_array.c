#include <stdlib.h>
#include "task_ref_array.h"

void tra_init(TaskRefArray *a) {
    a->data     = NULL;
    a->count    = 0;
    a->capacity = 0;
}

void tra_free(TaskRefArray *a) {
    free(a->data);
    a->data     = NULL;
    a->count    = 0;
    a->capacity = 0;
}

int tra_append(TaskRefArray *a, int project_id, int task_id) {
    if (a->count == a->capacity) {
        int new_cap = (a->capacity == 0) ? 1 : a->capacity * 2;
        TaskRef *tmp = (TaskRef *)realloc(a->data, new_cap * sizeof(TaskRef));
        if (!tmp) return 0;
        a->data     = tmp;
        a->capacity = new_cap;
    }
    a->data[a->count].project_id = project_id;
    a->data[a->count].task_id    = task_id;
    a->count++;
    return 1;
}

int tra_remove(TaskRefArray *a, int project_id, int task_id) {
    int i;
    for (i = 0; i < a->count; i++) {
        if (a->data[i].project_id == project_id && a->data[i].task_id == task_id) {
            a->data[i] = a->data[--a->count];
            return 1;
        }
    }
    return 0;
}
