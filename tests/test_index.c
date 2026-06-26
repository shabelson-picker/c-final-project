/* solo-ran verification: id->pointer index correctness across mutations.
 * Exercises the lazy-rebuild + dirty-flag path: add, find, remove (swap-remove
 * shuffles the array), split (adds), and confirm find still returns the right
 * task/member by id (or NULL when absent). Pure in-memory; own main().
 */
#include <stdio.h>
#include "company.h"
#include "algorithms.h"

static int g_fail = 0;
static void check(const char *what, int got, int want) {
    int ok = (got == want);
    printf("  [%s] %-40s got=%d want=%d\n", ok ? "PASS" : "FAIL", what, got, want);
    if (!ok) g_fail = 1;
}
static Date mkdate(int y,int m,int d){ Date dt; dt.year=y; dt.month=m; dt.day=d; return dt; }

int main(void) {
    Company *c = company_create("IdxTest");
    Project *p = company_add_project(c, "P", mkdate(2026,1,1));
    Task *t1 = project_add_task(p, "T1", "");
    Task *t2 = project_add_task(p, "T2", "");
    Task *t3 = project_add_task(p, "T3", "");
    Task *t4 = project_add_task(p, "T4", "");
    int id1=t1->id, id2=t2->id, id3=t3->id, id4=t4->id;
    TeamMember *m1, *m2, *m3;

    printf("id->pointer index\n");

    /* find by id returns the right object (builds index on first call) */
    check("find T1 by id", project_find_task(p, id1) == t1, 1);
    check("find T4 by id", project_find_task(p, id4) == t4, 1);
    check("find absent id 999 -> NULL", project_find_task(p, 999) == NULL, 1);
    check("START sentinel resolves", project_find_task(p, START_NODE_ID) != NULL, 1);

    /* remove T2 (swap-remove moves T4 into its slot); index must rebuild */
    project_remove_task(p, id2);
    check("after remove: T2 absent", project_find_task(p, id2) == NULL, 1);
    check("after remove: T4 still found", project_find_task(p, id4) == t4, 1);
    check("after remove: T1 still found", project_find_task(p, id1) == t1, 1);
    check("after remove: T3 still found", project_find_task(p, id3) == t3, 1);

    /* split T1 -> adds a new task; index must pick it up */
    {
        Task *b = project_split_task(p, id1, 0.5f);
        check("split made a task", b != NULL, 1);
        if (b) check("find split part2 by id", project_find_task(p, b->id) == b, 1);
    }

    /* members: add 3, find by id, remove middle, find again */
    m1 = company_add_member(c, "A", "r");
    m2 = company_add_member(c, "B", "r");
    m3 = company_add_member(c, "C", "r");
    check("find member m2", company_find_member(c, m2->id) == m2, 1);
    check("find member m3", company_find_member(c, m3->id) == m3, 1);
    check("find absent member 999 -> NULL", company_find_member(c, 999) == NULL, 1);
    {
        int idm2 = m2->id;
        company_remove_member(c, idm2);
        check("after remove: m2 absent", company_find_member(c, idm2) == NULL, 1);
        check("after remove: m1 found", company_find_member(c, m1->id) == m1, 1);
        check("after remove: m3 found", company_find_member(c, m3->id) == m3, 1);
    }

    company_destroy(c);
    printf(g_fail ? "\nRESULT: FAIL\n" : "\nRESULT: PASS\n");
    return g_fail;
}
