/* solo-ran verification (visual): unified company task Gantt, color-coded.
 * Two dated projects; eyeball that every task appears as one row on a shared
 * absolute timeline, Apollo tasks in one color and Borealis in another, plus a
 * legend. Provides its own main(); compile with src/*.c minus main.c.
 */
#include <stdio.h>
#include "company.h"
#include "algorithms.h"
#include "renderer.h"

static Date mkdate(int y,int m,int d){ Date dt; dt.year=y; dt.month=m; dt.day=d; return dt; }
static void seed(Project *p, const char *t, int s, int e) {
    Task *x = project_add_task(p, t, "");
    task_set_pert(x, (float)(e-s),(float)(e-s),(float)(e-s));
    x->sched_start = s; x->sched_end = e;
}

int main(void) {
    Company *c = company_create("Co");
    Project *a = company_add_project(c, "Apollo",   mkdate(2026,1,1));
    Project *b = company_add_project(c, "Borealis", mkdate(2026,1,10)); /* +9 days */
    seed(a, "A-design", 0, 5);
    seed(a, "A-build",  5, 8);
    seed(b, "B-spec",   0, 4);
    seed(b, "B-ship",   4, 6);

    printf("Unified company Gantt (visual: Apollo rows one color, Borealis another, shifted +9d)\n");
    render_company_gantt(c, 50);

    company_destroy(c);
    return 0;
}
