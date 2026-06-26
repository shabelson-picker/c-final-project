#ifndef MILESTONE_H
#define MILESTONE_H

#include "types.h"

/// <summary>
/// A project milestone: a named deadline (in days from project start) with a
/// priority and the set of task IDs expected to complete by it.
/// </summary>
typedef struct {
    int  id;
    char name[MAX_NAME_LEN];
    int  deadline_day;  /* days from project start */
    int  priority;      /* higher = more important to the scheduler */

    int *task_ids;
    int  task_count;
} Milestone;

/// <summary>Allocate and initialise a milestone (empty task list).</summary>
/// <param name="id">Milestone ID.</param>
/// <param name="name">Display name (copied).</param>
/// <param name="deadline_day">Deadline in days from project start.</param>
/// <param name="priority">Scheduling priority (higher = more important).</param>
/// <returns>New milestone, or NULL on allocation failure.</returns>
Milestone *milestone_create(int id, const char *name, int deadline_day, int priority);

/// <summary>Free a milestone and its task-id list.</summary>
/// <param name="m">Milestone to free (NULL-safe).</param>
void       milestone_destroy(Milestone *m);

/// <summary>Attach a task ID to the milestone (grows the list).</summary>
/// <param name="m">Milestone.</param>
/// <param name="task_id">Task ID to associate.</param>
/// <returns>1 on success, 0 on allocation failure.</returns>
int        milestone_add_task(Milestone *m, int task_id);

/// <summary>Detach a task ID from the milestone (swap-remove).</summary>
/// <param name="m">Milestone.</param>
/// <param name="task_id">Task ID to remove.</param>
/// <returns>1 if removed, 0 if not present.</returns>
int        milestone_remove_task(Milestone *m, int task_id);

/// <summary>Test whether a task ID is attached to the milestone.</summary>
/// <param name="m">Milestone.</param>
/// <param name="task_id">Task ID to look for.</param>
/// <returns>1 if attached, 0 otherwise.</returns>
int        milestone_has_task(const Milestone *m, int task_id);

/// <summary>Print a one-line summary of the milestone to stdout.</summary>
/// <param name="m">Milestone to print.</param>
void       milestone_print(const Milestone *m);

#endif /* MILESTONE_H */
