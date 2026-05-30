/* solo-ran verification: task splitting.
 * Chain X(2) -> A(10) -> Y(3). Split A 60/40 -> A becomes part1 (6), new part2 (4),
 * rechained X -> A1 -> B -> Y. Verify durations, the new dependency wiring, and the
 * resulting CPM schedule. Pure in-memory; own main(). Compile with src/*.c minus main.c.
 */
#include <stdio.h>
#include "company.h"
#include "algorithms.h"

static int g_fail = 0;
static void check(const char *what, int got, int want) {
    int ok = (got == want);
    printf("  [%s] %-34s got=%d want=%d\n", ok ? "PASS" : "FAIL", what, got, want);
    if (!ok) g_fail = 1;
}
static Date mkdate(int y,int m,int d){ Date dt; dt.year=y; dt.month=m; dt.day=d; return dt; }
static int contains(const DynamicIntArray *a, int id) {
    int i; for (i = 0; i < a->count; i++) if (a->data[i] == id) return 1; return 0;
}
static int exp_days(const Task *t) { return (int)(t->pert_expected + 0.5f); }

int main(void) {
    Company *c = company_create("SplitTest");
    Project *p = company_add_project(c, "Chain", mkdate(2026,1,1));
    Task *x = project_add_task(p, "X", ""); task_set_pert(x, 2,2,2);
    Task *a = project_add_task(p, "A", ""); task_set_pert(a, 10,10,10);
    Task *y = project_add_task(p, "Y", ""); task_set_pert(y, 3,3,3);
    Task *b;
    int aid = a->id, bid;

    project_link_tasks(p, x->id, a->id);
    project_link_tasks(p, a->id, y->id);
    a->assignee_id = 7;   /* pretend assigned; should carry to part 2 */

    printf("Task splitting\n");
    b = project_split_task(p, aid, 0.6f);
    check("split returned a task", b != NULL, 1);
    if (!b) { printf("\nRESULT: FAIL\n"); return 1; }
    bid = b->id;

    check("part1 duration 6", exp_days(a), 6);
    check("part2 duration 4", exp_days(b), 4);
    check("part2 inherits assignee", b->assignee_id, 7);

    /* wiring: X -> A1 -> B -> Y */
    check("X -> A1 (A1 has X as pre)", contains(&a->pre_ids,  x->id), 1);
    check("A1 -> B  (A1 post has B)",  contains(&a->post_ids, bid),   1);
    check("B  -> Y  (B post has Y)",   contains(&b->post_ids, y->id), 1);
    check("Y pre is now B, not A1",    contains(&y->pre_ids,  bid),   1);
    check("Y no longer depends on A1", contains(&y->pre_ids,  aid),   0);

    /* schedule (pure CPM): X 0..2, A1 2..8, B 8..12, Y 12..15 */
    schedule_project(p, SCHED_EARLIEST_DEADLINE);
    check("X starts 0",  x->sched_start, 0);
    check("A1 starts 2", a->sched_start, 2);
    check("B starts 8",  b->sched_start, 8);
    check("Y starts 12", y->sched_start, 12);
    check("Y ends 15 (makespan preserved)", y->sched_end, 15);

    company_destroy(c);
    printf(g_fail ? "\nRESULT: FAIL\n" : "\nRESULT: PASS\n");
    return g_fail;
}
