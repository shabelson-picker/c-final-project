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

    int        assignee_id;       /* -1 = unassigned                     */
    int        manually_assigned; /* 1 = pinned; scheduler won't reassign */
    int        milestone_id;     /* -1 = none                           */

    /* Set by scheduler */
    int        sched_start;     /* earliest start day from project day 0 */
    int        sched_end;       /* earliest end day                      */
    int        latest_start;    /* latest allowable start (for slack)    */
    int        slack;           /* latest_start - sched_start            */
    int        is_critical;     /* 1 if on critical path                 */
    int        topo_order;      /* 1-based topological position (0 = unscheduled) */
} Task;

/* ---- Lifecycle ---------------------------------------------------------- */

/// <summary>Allocate a task with empty dependency arrays and sentinel schedule fields.</summary>
/// <param name="id">Task ID (unique within its project).</param>
/// <param name="title">Title (copied).</param>
/// <param name="description">Description (copied).</param>
/// <returns>New task, or NULL on allocation failure.</returns>
Task *task_create(int id, const char *title, const char *description);

/// <summary>Free a task and its owned dependency arrays.</summary>
/// <param name="t">Task to free (NULL-safe).</param>
void  task_destroy(Task *t);

/* ---- Configuration ------------------------------------------------------ */

/// <summary>Set PERT estimates and recompute pert_expected = (min+4*likely+max)/6 and variance.</summary>
/// <param name="t">Task.</param>
/// <param name="min">Optimistic days.</param>
/// <param name="likely">Most-likely days.</param>
/// <param name="max">Pessimistic days.</param>
void  task_set_pert(Task *t, float min, float likely, float max);

/// <summary>Set the risk weight, clamped to [0.0, 1.0].</summary>
/// <param name="t">Task.</param>
/// <param name="risk">Risk weight (0 = none, 1 = critical).</param>
void  task_set_risk(Task *t, float risk);

/// <summary>Set the required-skills bitmask.</summary>
/// <param name="t">Task.</param>
/// <param name="skills">Bitmask of Skill values.</param>
void  task_set_skills(Task *t, uint32_t skills);

/* ---- Dependency management ---------------------------------------------- */

/// <summary>Add a logical predecessor ID to this task's pre list.</summary>
/// <param name="t">Task.</param>
/// <param name="pre_id">Predecessor task ID.</param>
/// <returns>1 on success, 0 on allocation failure.</returns>
int   task_add_pre(Task *t, int pre_id);

/// <summary>Add a logical successor ID to this task's post list.</summary>
/// <param name="t">Task.</param>
/// <param name="post_id">Successor task ID.</param>
/// <returns>1 on success, 0 on allocation failure.</returns>
int   task_add_post(Task *t, int post_id);

/// <summary>Add a plan-B alternative task ID.</summary>
/// <param name="t">Task.</param>
/// <param name="alt_id">Alternative task ID.</param>
/// <returns>1 on success, 0 on allocation failure.</returns>
int   task_add_alt(Task *t, int alt_id);

/// <summary>Remove a predecessor ID from the pre list.</summary>
/// <param name="t">Task.</param>
/// <param name="pre_id">Predecessor task ID.</param>
/// <returns>1 if removed, 0 if not present.</returns>
int   task_remove_pre(Task *t, int pre_id);

/// <summary>Remove a successor ID from the post list.</summary>
/// <param name="t">Task.</param>
/// <param name="post_id">Successor task ID.</param>
/// <returns>1 if removed, 0 if not present.</returns>
int   task_remove_post(Task *t, int post_id);

/* ---- Queries ------------------------------------------------------------ */

/// <summary>Test whether a member's skills cover this task's required skills.</summary>
/// <param name="t">Task.</param>
/// <param name="member_skills">Candidate member's skill bitmask.</param>
/// <returns>1 if all required skills are present, 0 otherwise.</returns>
int   task_can_be_done_by(const Task *t, uint32_t member_skills);

/* ---- Display ------------------------------------------------------------ */

/// <summary>Print a one-line task summary (status, PERT, risk, skills, flags, schedule) to stdout.</summary>
/// <param name="t">Task to print.</param>
void  task_print(const Task *t);

#endif /* TASK_H */
