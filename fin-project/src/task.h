#ifndef TASK_H
#define TASK_H

#include "types.h"
#include "dynamic_int_array.h"

typedef struct {
    int        id;
    char       title[MAX_TITLE_LEN];
    char       description[MAX_DESC_LEN];
    TaskStatus status;

    /* PERT time estimates in days */
    float      pert_min;
    float      pert_likely;
    float      pert_max;
    float      pert_expected;   /* computed: (min + 4*likely + max) / 6 */
    float      pert_variance;   /* computed: ((max - min) / 6)^2      */

    float      risk;            /* 0.0 = no risk, 1.0 = critical       */
    uint32_t   required_skills; /* bitmask of Skill values             */

    /* Dependency arrays - owned, heap-allocated */
    DynamicIntArray pre_ids;      /* logical predecessors (DAG edges)         */
    DynamicIntArray post_ids;     /* logical successors  (DAG edges)          */
    DynamicIntArray alt_ids;      /* plan-B alternatives if cancelled         */
    DynamicIntArray work_pre_ids; /* resource constraints - must not enter DAG */

    int        assignee_id;     /* -1 = unassigned                     */
    int        milestone_id;    /* -1 = none                           */

    /* Set by scheduler */
    int        sched_start;     /* earliest start day from project day 0 */
    int        sched_end;       /* earliest end day                      */
    int        latest_start;    /* latest allowable start (for slack)    */
    int        slack;           /* latest_start - sched_start            */
    int        is_critical;     /* 1 if on critical path                 */
} Task;

/* Lifecycle */
Task *task_create(int id, const char *title, const char *description);
void  task_destroy(Task *t);

/* Configuration */
void  task_set_pert(Task *t, float min, float likely, float max);
void  task_set_risk(Task *t, float risk);
void  task_set_skills(Task *t, uint32_t skills);

/* Dependency management */
int   task_add_pre(Task *t, int pre_id);
int   task_add_post(Task *t, int post_id);
int   task_add_alt(Task *t, int alt_id);
int   task_remove_pre(Task *t, int pre_id);
int   task_remove_post(Task *t, int post_id);

/* Queries */
int   task_can_be_done_by(const Task *t, uint32_t member_skills);

/* Display */
void  task_print(const Task *t);

#endif /* TASK_H */
