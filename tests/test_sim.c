/* solo-ran verification: simulation day-advance core. */
#include <stdio.h>
#include "company.h"
#include "algorithms.h"
#include "sim.h"

static int g_fail = 0;
static void check(const char *what, int got, int want) {
    int ok = (got == want);
    printf("  [%s] %-28s got=%d want=%d\n", ok ? "PASS" : "FAIL", what, got, want);
    if (!ok) g_fail = 1;
}
static Date mkdate(int y,int m,int d){ Date dt; dt.year=y; dt.month=m; dt.day=d; return dt; }

int main(void) {
    Company *c = company_create("Sim");
    Project *p = company_add_project(c, "P", mkdate(2026,1,1));
    Task *a = project_add_task(p, "A", ""); task_set_pert(a,5,5,5); a->assignee_id = 1;
    Task *b = project_add_task(p, "B", ""); task_set_pert(b,5,5,5); b->assignee_id = 1;
    project_link_tasks(p, a->id, b->id);
    schedule_project(p, SCHED_EARLIEST_DEADLINE);   /* A 0..5, B 5..10 */

    printf("Simulation day-advance\n");
    check("day3: 0 completed",      sim_advance_day(p, 3), 0);
    check("day3: A in-progress",    a->status, STATUS_IN_PROGRESS);
    check("day3: B still todo",     b->status, STATUS_TODO);
    check("day5: 1 completed (A)",  sim_advance_day(p, 5), 1);
    check("day5: A done",           a->status, STATUS_DONE);
    check("day5: B in-progress",    b->status, STATUS_IN_PROGRESS);
    check("day10: 1 completed (B)", sim_advance_day(p, 10), 1);
    check("day10: B done",          b->status, STATUS_DONE);
    check("day11: nothing left",    sim_advance_day(p, 11), 0);

    company_destroy(c);
    printf(g_fail ? "\nRESULT: FAIL\n" : "\nRESULT: PASS\n");
    return g_fail;
}
