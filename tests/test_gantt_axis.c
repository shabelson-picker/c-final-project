/* Throwaway visual check: per-project Gantt on the absolute company timeline.
 * Two dated projects; the EARLIER one (Apollo, Jan 1) fixes company day 0.
 * We render the LATER one (Borealis, Jan 20 -> day0 = 19), so the ruler should
 * start at 0, the dashed ':' project-start line should sit ~day 19, and the
 * bars + milestone should be shifted right onto that absolute axis.
 * Provides its own main(); compile with src/*.c minus main.c.
 */
#include <stdio.h>
#include "company.h"
#include "renderer.h"

static Date mkdate(int y, int m, int d) { Date dt; dt.year=y; dt.month=m; dt.day=d; return dt; }

/* Fake a scheduled task (project-relative offsets), like test_portfolio does. */
static Task *sched_task(Project *p, const char *title, int start, int end,
                        int topo, int crit) {
    Task *x = project_add_task(p, title, "");
    task_set_pert(x, (float)(end - start) - 1, (float)(end - start), (float)(end - start) + 2);
    x->sched_start = start;
    x->sched_end   = end;
    x->topo_order  = topo;
    x->is_critical = crit;
    return x;
}

int main(void) {
    Company *c = company_create("Timeline Co");
    Project *a = company_add_project(c, "Apollo (Jan1)",  mkdate(2026, 1, 1));
    Project *b = company_add_project(c, "Borealis (Jan20)", mkdate(2026, 1, 20));

    sched_task(a, "anchor", 0, 5, 1, 0);      /* fixes company day 0 at Jan 1 */

    sched_task(b, "Kickoff",  0,  4, 1, 1);
    sched_task(b, "Build",    4, 14, 2, 1);
    sched_task(b, "Test",    14, 20, 3, 0);
    project_add_milestone(b, "Ship", 20, 1);

    printf("Rendering Borealis: starts company-day 19, so expect a ':' line ~19,\n");
    printf("bars shifted right, ruler numbered from 0.\n");
    render_gantt(b, c, 60);

    company_destroy(c);
    return 0;
}
