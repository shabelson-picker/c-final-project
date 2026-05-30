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

    Task           *start_node;         /* [START] - zero-duration source node */
    Task           *end_node;           /* [END]   - zero-duration sink node   */

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

/// <summary>Allocate a project with START/END sentinel nodes and empty task/milestone arrays.</summary>
/// <param name="name">Project name (copied).</param>
/// <param name="start_date">Calendar start date (day 0 of the schedule).</param>
/// <returns>New project, or NULL on allocation failure.</returns>
Project   *project_create(const char *name, Date start_date);

/// <summary>Free a project and everything it owns (tasks, milestones, sentinels, member-id list).</summary>
/// <param name="p">Project to free (NULL-safe).</param>
void       project_destroy(Project *p);

/// <summary>Create a task and wire it as an island (START -> task -> END).</summary>
/// <param name="p">Project.</param>
/// <param name="title">Task title.</param>
/// <param name="desc">Task description.</param>
/// <returns>The new task, or NULL on failure.</returns>
Task      *project_add_task(Project *p, const char *title, const char *desc);

/// <summary>Create a milestone in the project.</summary>
/// <param name="p">Project.</param>
/// <param name="name">Milestone name.</param>
/// <param name="deadline_day">Deadline in days from project start.</param>
/// <param name="priority">Scheduling priority.</param>
/// <returns>The new milestone, or NULL on failure.</returns>
Milestone *project_add_milestone(Project *p, const char *name, int deadline_day, int priority);

/// <summary>Remove a task and detach it from all neighbours, reconnecting orphans to START/END.</summary>
/// <param name="p">Project.</param>
/// <param name="task_id">ID of the task to remove.</param>
/// <returns>1 if removed, 0 if not found.</returns>
int        project_remove_task(Project *p, int task_id);

/// <summary>Look up a task by ID (START/END IDs return the sentinel nodes).</summary>
/// <param name="p">Project.</param>
/// <param name="id">Task ID.</param>
/// <returns>The task, or NULL if not found.</returns>
Task      *project_find_task(const Project *p, int id);

/// <summary>Look up a milestone by ID.</summary>
/// <param name="p">Project.</param>
/// <param name="id">Milestone ID.</param>
/// <returns>The milestone, or NULL if not found.</returns>
Milestone *project_find_milestone(const Project *p, int id);

/// <summary>Add a dependency pre_id -> post_id, rejecting duplicates and cycles; detaches sentinels.</summary>
/// <param name="p">Project.</param>
/// <param name="pre_id">Predecessor task ID.</param>
/// <param name="post_id">Successor task ID.</param>
/// <returns>1 if the link was added, 0 if invalid / duplicate / would create a cycle.</returns>
int        project_link_tasks(Project *p, int pre_id, int post_id);

/// <summary>Associate a task with a milestone.</summary>
/// <param name="p">Project.</param>
/// <param name="task_id">Task ID.</param>
/// <param name="milestone_id">Milestone ID.</param>
/// <returns>1 on success, 0 if either was not found.</returns>
int        project_link_task_milestone(Project *p, int task_id, int milestone_id);

/// <summary>Print a full text summary of the project (tasks + milestones) to stdout.</summary>
/// <param name="p">Project.</param>
void       project_print_summary(const Project *p);

/// <summary>Check (via DFS) whether adding pre_id -> post_id would introduce a cycle; prints the cycle if so.</summary>
/// <param name="p">Project.</param>
/// <param name="pre_id">Proposed predecessor.</param>
/// <param name="post_id">Proposed successor.</param>
/// <returns>1 if a cycle would form, 0 if the edge is safe.</returns>
int        project_would_create_cycle(const Project *p, int pre_id, int post_id);

#endif /* PROJECT_H */
