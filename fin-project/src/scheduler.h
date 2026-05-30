#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "project.h"
#include "company.h"

typedef enum {
    SCHED_EARLIEST_DEADLINE,        /* schedule tasks to hit milestones soonest */
    SCHED_RISK_WEIGHTED_CRITICAL    /* weight durations by risk; flag critical path */
} ScheduleStrategy;

/*
 * Run a forward+backward pass over the task DAG to compute
 * sched_start, sched_end, latest_start, slack, is_critical on every task.
 * Returns 1 on success, 0 if a dependency cycle is detected.
 */
int schedule_project(Project *p, ScheduleStrategy strategy);

/*
 * Greedy member assignment: for each unassigned task (in schedule order),
 * pick the first member whose skill bitmask satisfies required_skills.
 */
void assign_members_greedy(Company *c, int project_idx);

/* Print a text report of the computed schedule */
void schedule_print_report(const Project *p);

#endif /* SCHEDULER_H */
