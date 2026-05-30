#ifndef TASK_REF_ARRAY_H
#define TASK_REF_ARRAY_H

#include "types.h"

typedef struct {
    TaskRef *data;
    int      count;
    int      capacity;
} TaskRefArray;

void tra_init(TaskRefArray *a);
void tra_free(TaskRefArray *a);
int  tra_append(TaskRefArray *a, int project_id, int task_id);
int  tra_remove(TaskRefArray *a, int project_id, int task_id);

#endif /* TASK_REF_ARRAY_H */
