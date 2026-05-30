#ifndef PROJECT_H
#define PROJECT_H

#include "task.h"
#include "milestone.h"
#include "dynamic_int_array.h"

/* START_NODE_ID and END_NODE_ID are defined in constants.h */

typedef struct {
    char            name[MAX_NAME_LEN];
    char            save_dir[256];      /* bundle directory; empty = not set */
    Date            start_date;

    Task           *start_node;         /* [START] — zero-duration source node */
    Task           *end_node;           /* [END]   — zero-duration sink node   */

    Task          **tasks;
    int             task_count;
    int             task_capacity;

    Milestone     **milestones;
    int             milestone_count;
    int             milestone_capacity;

    DynamicIntArray member_ids;         /* IDs of company members on this project */

    int             next_task_id;       /* user tasks start at 1 */
    int             next_milestone_id;
} Project;

Project   *project_create(const char *name, Date start_date);
void       project_destroy(Project *p);

Task      *project_add_task(Project *p, const char *title, const char *desc);
Milestone *project_add_milestone(Project *p, const char *name, int deadline_day, int priority);

int        project_remove_task(Project *p, int task_id);

Task      *project_find_task(Project *p, int id);
Milestone *project_find_milestone(Project *p, int id);

int        project_link_tasks(Project *p, int pre_id, int post_id);
int        project_link_task_milestone(Project *p, int task_id, int milestone_id);

void       project_print_summary(const Project *p);
int        project_would_create_cycle(const Project *p, int pre_id, int post_id);

#endif /* PROJECT_H */
