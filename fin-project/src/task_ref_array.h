#ifndef TASK_REF_ARRAY_H
#define TASK_REF_ARRAY_H

#include "types.h"

/// <summary>
/// Growable array of TaskRef pairs (project_id, task_id). Identifies tasks
/// across projects, since task IDs are only unique within a project.
/// </summary>
typedef struct {
    TaskRef *data;
    int      count;
    int      capacity;
} TaskRefArray;

/// <summary>Initialise an empty array (no allocation yet).</summary>
/// <param name="a">Array to initialise.</param>
void tra_init(TaskRefArray *a);

/// <summary>Free the backing buffer and reset to empty.</summary>
/// <param name="a">Array to free.</param>
void tra_free(TaskRefArray *a);

/// <summary>Append a (project_id, task_id) reference, growing capacity if needed.</summary>
/// <param name="a">Array.</param>
/// <param name="project_id">Project index the task belongs to.</param>
/// <param name="task_id">Task ID within that project.</param>
/// <returns>1 on success, 0 on allocation failure.</returns>
int  tra_append(TaskRefArray *a, int project_id, int task_id);

/// <summary>Remove the first matching (project_id, task_id) by swap-with-last (not order-preserving).</summary>
/// <param name="a">Array.</param>
/// <param name="project_id">Project index to match.</param>
/// <param name="task_id">Task ID to match.</param>
/// <returns>1 if found and removed, 0 otherwise.</returns>
int  tra_remove(TaskRefArray *a, int project_id, int task_id);

#endif /* TASK_REF_ARRAY_H */
