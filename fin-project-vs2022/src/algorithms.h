#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include "project.h"
#include "company.h"

typedef enum {
    SCHED_EARLIEST_DEADLINE,
    SCHED_RISK_WEIGHTED_CRITICAL,
    SCHED_PESSIMISTIC
} ScheduleStrategy;

/* How greedy assignment ranks qualified members for a task. All three use the
 * same keys (availability, current workload, skill-fit surplus, member id) in a
 * different priority order:
 *  - EARLIEST_FREE: free day first  -> tightest makespan (default).
 *  - BALANCED:      workload first   -> spread work evenly across the team.
 *  - BEST_FIT:      skill surplus first -> specialists first, keep generalists free. */
typedef enum {
    ASSIGN_EARLIEST_FREE,
    ASSIGN_BALANCED,
    ASSIGN_BEST_FIT
} AssignPolicy;

/* Set the policy used by the next assign_members_greedy run. Module-level so
 * callers need not thread it through every layer. */
void          assign_set_policy(AssignPolicy policy);

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
void assign_members_greedy(Company *c, int project_idx, ScheduleStrategy strategy);

/* Company-wide schedule: clears every project, then schedules them in start-date
 * order carrying ONE shared member calendar (each member's tasks across projects
 * are serialized on a single timeline). Unlike per-project scheduling + the coarse
 * floor, this reads no other project's sched_end, so it is deterministic and
 * idempotent (no drift on re-runs) and cannot double-book a shared member. */
void schedule_company(Company *c, ScheduleStrategy strategy);

/* Suggest a company member NOT on the project roster who has all of a task's
 * required skills. Returns the member id, or -1 if none qualifies. Pure (no I/O);
 * the UI uses this to offer pulling a worker onto the project after assignment. */
int suggest_company_member(const Company *c, const Project *p, const Task *t);

#endif /* ALGORITHMS_H */
