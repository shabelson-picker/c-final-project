#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "milestone.h"

Milestone *milestone_create(int id, const char *name, int deadline_day, int priority) {
    Milestone *m = (Milestone *)malloc(sizeof(Milestone));
    if (!m) return NULL;

    memset(m, 0, sizeof(Milestone));
    m->id           = id;
    m->deadline_day = deadline_day;
    m->priority     = priority;
    str_copy(m->name, name, MAX_NAME_LEN);

    return m;
}

void milestone_destroy(Milestone *m) {
    if (!m) return;
    free(m->task_ids);
    free(m);
}

int milestone_add_task(Milestone *m, int task_id) {
    int *tmp;
    if (milestone_has_task(m, task_id)) return 1;   /* already attached - idempotent */
    tmp = (int *)realloc(m->task_ids, (m->task_count + 1) * sizeof(int));
    if (!tmp) return 0;
    m->task_ids = tmp;
    m->task_ids[m->task_count++] = task_id;
    return 1;
}

int milestone_has_task(const Milestone *m, int task_id) {
    int i;
    for (i = 0; i < m->task_count; i++)
        if (m->task_ids[i] == task_id) return 1;
    return 0;
}

int milestone_remove_task(Milestone *m, int task_id) {
    int i;
    for (i = 0; i < m->task_count; i++) {
        if (m->task_ids[i] == task_id) {
            m->task_ids[i] = m->task_ids[--m->task_count];  /* swap-remove (order irrelevant) */
            return 1;
        }
    }
    return 0;
}

void milestone_print(const Milestone *m) {
    printf("[M%d] %-30s  deadline: day %-4d  priority: %d  tasks: %d\n",
           m->id, m->name, m->deadline_day, m->priority, m->task_count);
}
