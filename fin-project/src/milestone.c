#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "milestone.h"

Milestone *milestone_create(int id, const char *name, int deadline_day, int priority) {
    Milestone *m = (Milestone *)malloc(sizeof(Milestone));
    if (!m) return NULL;

    memset(m, 0, sizeof(Milestone));
    m->id           = id;
    m->deadline_day = deadline_day;
    m->priority     = priority;
    strncpy(m->name, name, MAX_NAME_LEN - 1);

    return m;
}

void milestone_destroy(Milestone *m) {
    if (!m) return;
    free(m->task_ids);
    free(m);
}

int milestone_add_task(Milestone *m, int task_id) {
    int *tmp = (int *)realloc(m->task_ids, (m->task_count + 1) * sizeof(int));
    if (!tmp) return 0;
    m->task_ids = tmp;
    m->task_ids[m->task_count++] = task_id;
    return 1;
}

void milestone_print(const Milestone *m) {
    printf("[M%d] %-30s  deadline: day %-4d  priority: %d  tasks: %d\n",
           m->id, m->name, m->deadline_day, m->priority, m->task_count);
}
