#include <stdio.h>
#include <stdlib.h>
#include "reports.h"
#include "ui.h"

/* One assigned interval in absolute days. */
typedef struct { long start; long end; } Interval;

static int cmp_interval(const void *a, const void *b) {
    long d = ((const Interval *)a)->start - ((const Interval *)b)->start;
    return (d > 0) - (d < 0);
}

int compute_member_workload(const Company *c, WorkloadStat *out, int max) {
    int mi, n = 0;

    for (mi = 0; mi < c->member_count && n < max; mi++) {
        int        mid = c->members[mi]->id;
        Interval  *iv;
        int        cap = 8, cnt = 0, i, pi, ti;
        WorkloadStat *w = &out[n];

        w->member_id     = mid;
        w->task_count    = 0;
        w->committed_days = 0;
        w->first_day     = 0;
        w->last_day      = 0;
        w->overallocated = 0;

        iv = (Interval *)malloc((size_t)cap * sizeof(Interval));
        if (!iv) { n++; continue; }

        /* Gather this member's assignments as absolute intervals. */
        for (pi = 0; pi < c->project_count; pi++) {
            Project *p = c->projects[pi];
            long base;
            if (!date_is_valid(p->start_date)) continue;
            base = date_to_days(p->start_date);
            for (ti = 0; ti < p->task_count; ti++) {
                Task *t = p->tasks[ti];
                if (t->assignee_id != mid) continue;
                if (cnt == cap) {
                    Interval *tmp = (Interval *)realloc(iv, (size_t)(cap *= 2) * sizeof(Interval));
                    if (!tmp) break;
                    iv = tmp;
                }
                iv[cnt].start = base + t->sched_start;
                iv[cnt].end   = base + t->sched_end;
                w->committed_days += (t->sched_end - t->sched_start);
                cnt++;
            }
        }

        w->task_count = cnt;
        if (cnt > 0) {
            qsort(iv, (size_t)cnt, sizeof(Interval), cmp_interval);
            w->first_day = (int)iv[0].start;
            w->last_day  = (int)iv[cnt - 1].end;
            for (i = 1; i < cnt; i++) {
                if (iv[i].start < iv[i - 1].end) w->overallocated = 1;  /* absolute-time clash */
                if (iv[i].end > w->last_day)     w->last_day = (int)iv[i].end;
            }
        }

        free(iv);
        n++;
    }
    return n;
}

void render_workload_report(const Company *c) {
    WorkloadStat stats[256];
    int n = compute_member_workload(c, stats, 256);
    int i;

    cprintf(C_BOLD, "\n=== Resource Workload (across all projects) ===\n");
    cprintf(C_BOLD, "%-18s  %5s  %9s  %-22s  %s\n",
            "Member", "Tasks", "Days", "Active window", "Status");
    printf("---------------------------------------------------------------------------\n");
    for (i = 0; i < n; i++) {
        TeamMember *m = company_find_member((Company *)c, stats[i].member_id);
        char span[32];
        if (stats[i].task_count > 0) {
            Date f = date_from_days(stats[i].first_day);
            snprintf(span, sizeof span, "%04d-%02d-%02d (+%dd)",
                     f.year, f.month, f.day, stats[i].last_day - stats[i].first_day);
        } else {
            snprintf(span, sizeof span, "-");
        }
        cprintf(ui_zebra(i), "%-18.18s  %5d  %7dd  %-22s  ",
                m ? m->name : "?", stats[i].task_count, stats[i].committed_days, span);
        if (stats[i].overallocated) cprintf(C_RED,    "OVERALLOCATED\n");
        else if (stats[i].task_count == 0) cprintf(C_DIM, "idle\n");
        else                        cprintf(C_GREEN,  "ok\n");
    }
}
