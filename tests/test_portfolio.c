/* solo-ran verification (visual): portfolio Gantt.
 * Two dated projects with a known offset + a no-date project. Eyeball that bars
 * land on the shared absolute timeline and the no-date row shows "(no start date)".
 * Provides its own main(); compile with src/*.c minus main.c.
 */
#include <stdio.h>
#include "company.h"
#include "algorithms.h"
#include "renderer.h"

static Date mkdate(int y, int m, int d) { Date dt; dt.year=y; dt.month=m; dt.day=d; return dt; }

static void add_task(Project *p, const char *t, float days) {
    Task *x = project_add_task(p, t, "");
    task_set_pert(x, days, days, days);
    x->sched_start = 0;
    x->sched_end   = (int)days;   /* pretend scheduled, project-relative */
}

int main(void) {
    Company *c = company_create("Portfolio");
    Project *a = company_add_project(c, "Apollo (Jan1, 10d)", mkdate(2026, 1, 1));
    Project *b = company_add_project(c, "Borealis (Jan15, 20d)", mkdate(2026, 1, 15));
    Project *d = company_add_project(c, "Cobalt (no date)", mkdate(0, 0, 0));
    add_task(a, "a", 10);
    add_task(b, "b", 20);
    add_task(d, "d", 5);

    printf("Portfolio Gantt (visual check: Apollo bar left, Borealis bar shifted right + longer, Cobalt no bar)\n");
    render_portfolio_gantt(c, 60);

    company_destroy(c);
    return 0;
}
