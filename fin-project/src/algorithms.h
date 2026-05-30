#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include "project.h"
#include "company.h"

typedef enum {
    SCHED_EARLIEST_DEADLINE,
    SCHED_RISK_WEIGHTED_CRITICAL
} ScheduleStrategy;

/* ---- CPM scheduling pipeline -------------------------------------------- */

/* Topological sort considering logical (pre_ids) and resource (work_pre_ids)
 * edges. Returns heap-allocated index array in topo order; caller frees.
 * Sets *out_count = -1 and returns NULL if a cycle is detected. */
int *topo_sort(Project *p, int *out_count);

/* Forward pass: compute sched_start and sched_end for every task. */
void forward_pass(Project *p, const int *order, int n, ScheduleStrategy s);

/* Backward pass: compute latest_start, slack, and is_critical. */
void backward_pass(Project *p, const int *order, int n, ScheduleStrategy s);

/* Run the full CPM pipeline (topo sort + forward + backward pass).
 * Returns 1 on success, 0 if a cycle is detected. */
int schedule_project(Project *p, ScheduleStrategy strategy);

/* Print a text schedule report to stdout. */
void schedule_print_report(const Project *p);

/* ---- Greedy member assignment ------------------------------------------- */

/* Assign company members to project tasks using a greedy algorithm:
 * - Tasks processed in sched_start order
 * - Each task gets the earliest-free qualified member on the project roster
 * - Resource conflicts resolved via work_pre_ids (separate from DAG)
 * - Iterates up to MAX_ASSIGN_PASSES re-scheduling rounds
 * - Warns if no qualified member exists for a task */
void assign_members_greedy(Company *c, int project_idx);

#endif /* ALGORITHMS_H */
