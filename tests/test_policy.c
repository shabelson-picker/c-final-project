/* solo-ran verification: assignment policies.
 *
 * Two qualified members - a backend specialist and a multi-skill generalist -
 * and two independent backend tasks. The policy decides who gets them:
 *   EARLIEST_FREE / BALANCED -> spread one each (makespan 5)
 *   BEST_FIT                 -> both stack on the specialist (makespan 10),
 *                               keeping the generalist free.
 *
 * Pure in-memory; provides its own main(). Compile with src/*.c minus main.c.
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

static Date mkdate(int y, int m, int d) { Date dt; dt.year=y; dt.month=m; dt.day=d; return dt; }

static int count_for(const Project *p, int member_id) {
    int i, n = 0;
    for (i = 0; i < p->task_count; i++)
        if (p->tasks[i]->assignee_id == member_id) n++;
    return n;
}
static int makespan(const Project *p) {
    int i, mx = 0;
    for (i = 0; i < p->task_count; i++)
        if (p->tasks[i]->sched_end > mx) mx = p->tasks[i]->sched_end;
    return mx;
}

int main(void) {
    Company *c = company_create("PolicyTest");
    TeamMember *spec, *gen;
    Project *p;
    Task *t1, *t2;

    spec = company_add_member(c, "Spec", "Engineer");
    team_member_add_skill(spec, SKILL_BACKEND);

    gen = company_add_member(c, "Gen", "Lead");
    team_member_add_skill(gen, SKILL_BACKEND);
    team_member_add_skill(gen, SKILL_FRONTEND);
    team_member_add_skill(gen, SKILL_QA);
    team_member_add_skill(gen, SKILL_DEVOPS);   /* surplus 3 for a backend task */

    p  = company_add_project(c, "Solo", mkdate(2026, 1, 1));
    t1 = project_add_task(p, "T1", ""); task_set_pert(t1, 5,5,5); task_set_skills(t1, SKILL_BACKEND);
    t2 = project_add_task(p, "T2", ""); task_set_pert(t2, 5,5,5); task_set_skills(t2, SKILL_BACKEND);
    (void)t1; (void)t2;

    company_assign_member(c, 0, spec->id, -1);
    company_assign_member(c, 0, gen->id, -1);

    printf("Assignment policies\n");

    assign_set_policy(ASSIGN_EARLIEST_FREE);
    assign_members_greedy(c, 0, SCHED_EARLIEST_DEADLINE);
    check("EARLIEST: spec gets 1", count_for(p, spec->id), 1);
    check("EARLIEST: gen gets 1",  count_for(p, gen->id),  1);
    check("EARLIEST: makespan 5",  makespan(p), 5);

    assign_set_policy(ASSIGN_BALANCED);
    assign_members_greedy(c, 0, SCHED_EARLIEST_DEADLINE);
    check("BALANCED: spec gets 1", count_for(p, spec->id), 1);
    check("BALANCED: gen gets 1",  count_for(p, gen->id),  1);
    check("BALANCED: makespan 5",  makespan(p), 5);

    assign_set_policy(ASSIGN_BEST_FIT);
    assign_members_greedy(c, 0, SCHED_EARLIEST_DEADLINE);
    check("BEST_FIT: spec gets 2", count_for(p, spec->id), 2);
    check("BEST_FIT: gen gets 0",  count_for(p, gen->id),  0);
    check("BEST_FIT: makespan 10", makespan(p), 10);

    company_destroy(c);
    printf(g_fail ? "\nRESULT: FAIL\n" : "\nRESULT: PASS\n");
    return g_fail;
}
