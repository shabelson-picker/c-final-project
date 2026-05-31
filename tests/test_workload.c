/* solo-ran verification: resource workload / over-allocation report.
 * Carol is assigned two tasks that overlap in absolute time across two projects
 * (must flag OVERALLOCATED); Dave has two non-overlapping tasks (ok); Eve is idle.
 * Pure in-memory; own main(). Compile with src/*.c minus main.c.
 */
#include <stdio.h>
#include "company.h"
#include "reports.h"

static int g_fail = 0;
static void check(const char *what, int got, int want) {
    int ok = (got == want);
    printf("  [%s] %-34s got=%d want=%d\n", ok ? "PASS" : "FAIL", what, got, want);
    if (!ok) g_fail = 1;
}
static Date mkdate(int y,int m,int d){ Date dt; dt.year=y; dt.month=m; dt.day=d; return dt; }
static void assign(Project *p, const char *title, int s, int e, int who) {
    Task *x = project_add_task(p, title, "");
    x->sched_start = s; x->sched_end = e; x->assignee_id = who;
}
static WorkloadStat *find(WorkloadStat *w, int n, int id) {
    int i; for (i = 0; i < n; i++) if (w[i].member_id == id) return &w[i]; return NULL;
}

int main(void) {
    Company *c = company_create("WL");
    TeamMember *carol = company_add_member(c, "Carol", "Eng");
    TeamMember *dave  = company_add_member(c, "Dave",  "Eng");
    TeamMember *eve   = company_add_member(c, "Eve",   "Eng");
    Project *a = company_add_project(c, "A", mkdate(2026,1,1));
    Project *b = company_add_project(c, "B", mkdate(2026,1,1));   /* same start -> frames align */
    WorkloadStat stats[16];
    int n;
    WorkloadStat *wc, *wd, *we;

    /* Carol: [0,5] in A and [3,8] in B -> overlap in absolute time */
    assign(a, "A-carol", 0, 5, carol->id);
    assign(b, "B-carol", 3, 8, carol->id);
    /* Dave: [10,15] in A and [0,2] in B -> no overlap */
    assign(a, "A-dave", 10, 15, dave->id);
    assign(b, "B-dave",  0,  2, dave->id);
    /* Eve: nothing */

    /* date_from_days is the inverse of date_to_days (used to show spans as dates) */
    {
        Date in = mkdate(2026, 5, 31), out = date_from_days(date_to_days(in));
        check("date round-trip Y", out.year,  2026);
        check("date round-trip M", out.month, 5);
        check("date round-trip D", out.day,   31);
    }

    n = compute_member_workload(c, stats, 16);
    printf("Resource workload (n=%d)\n", n);
    check("member count", n, 3);

    wc = find(stats, n, carol->id);
    wd = find(stats, n, dave->id);
    we = find(stats, n, eve->id);
    check("found all three", (wc && wd && we) ? 1 : 0, 1);
    if (!(wc && wd && we)) { printf("\nRESULT: FAIL\n"); company_destroy(c); return 1; }

    check("Carol 2 tasks",        wc->task_count, 2);
    check("Carol 10 committed days", wc->committed_days, 10);
    check("Carol OVERALLOCATED",  wc->overallocated, 1);
    check("Carol span width 8 (abs days)", wc->last_day - wc->first_day, 8);

    check("Dave 2 tasks",         wd->task_count, 2);
    check("Dave NOT overallocated", wd->overallocated, 0);
    check("Dave 7 committed days", wd->committed_days, 7);

    check("Eve idle (0 tasks)",   we->task_count, 0);
    check("Eve not overallocated", we->overallocated, 0);

    render_workload_report(c);   /* visual sanity check of the table */

    company_destroy(c);
    printf(g_fail ? "\nRESULT: FAIL\n" : "\nRESULT: PASS\n");
    return g_fail;
}
