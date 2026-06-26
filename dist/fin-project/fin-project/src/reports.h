#ifndef REPORTS_H
#define REPORTS_H

#include "company.h"

/* Resource analytics. Inspired by what lighter PM tools are criticized for
 * lacking: a clear per-member workload / over-allocation picture across projects. */

typedef struct {
    int member_id;
    int task_count;      /* assigned tasks across all dated projects        */
    int committed_days;  /* sum of assigned task durations                  */
    int first_day;       /* absolute start day of earliest assignment       */
    int last_day;        /* absolute end day of latest assignment           */
    int overallocated;   /* 1 if any two assignments overlap in absolute time */
} WorkloadStat;

/* Compute one WorkloadStat per company member into out[0..max).
 * Uses absolute days (project start_date + task sched_start/end), so conflicts
 * are detected across projects, not just within one. Members on projects without
 * a valid start_date contribute nothing. Returns the number of members written. */
int  compute_member_workload(const Company *c, WorkloadStat *out, int max);

/* Print a per-member workload table (task count, committed days, span, and an
 * OVERALLOCATED flag) to stdout. */
void render_workload_report(const Company *c);

#endif /* REPORTS_H */
