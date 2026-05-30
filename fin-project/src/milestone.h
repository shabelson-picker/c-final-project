#ifndef MILESTONE_H
#define MILESTONE_H

#include "types.h"

typedef struct {
    int  id;
    char name[MAX_NAME_LEN];
    int  deadline_day;  /* days from project start */
    int  priority;      /* higher = more important to the scheduler */

    int *task_ids;
    int  task_count;
} Milestone;

Milestone *milestone_create(int id, const char *name, int deadline_day, int priority);
void       milestone_destroy(Milestone *m);
int        milestone_add_task(Milestone *m, int task_id);
void       milestone_print(const Milestone *m);

#endif /* MILESTONE_H */
