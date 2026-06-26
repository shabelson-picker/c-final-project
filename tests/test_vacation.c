/* solo-ran verification: vacations as immovable time blocks.
 *
 * A vacation is a fixed_time, member-pinned task. The scheduler must keep its
 * window fixed and route the member's other work around it (no splitting -> a
 * task that would straddle the block is pushed entirely after it; a task that
 * fits before the block stays put).
 *
 * Pure in-memory; provides its own main(). Compile with src/*.c minus main.c.
 */
#include <stdio.h>
#include "company.h"
#include "algorithms.h"

static int g_fail = 0;
static void check(const char *what, int got, int want) {
    int ok = (got == want);
    printf("  [%s] %-36s got=%d want=%d\n", ok ? "PASS" : "FAIL", what, got, want);
    if (!ok) g_fail = 1;
}
static Date mkdate(int y, int m, int d) { Date dt; dt.year=y; dt.month=m; dt.day=d; return dt; }

int main(void) {
    Company *c = company_create("VacTest");
    TeamMember *carol = company_add_member(c, "Carol", "Engineer");
    Project *p1, *p2;
    Task *big, *vac1, *small, *vac2;
    team_member_set_skills(carol, SKILL_BACKEND);

    printf("Vacations as immovable blocks\n");

    /* Scenario 1: a 10-day task overlaps a day-3..7 vacation -> pushed to day 7. */
    p1  = company_add_project(c, "Overlap", mkdate(2026, 1, 1));
    big = project_add_task(p1, "Big", ""); task_set_pert(big, 10,10,10); task_set_skills(big, SKILL_BACKEND);
    vac1 = project_add_fixed_block(p1, "[Vacation] Carol", carol->id, 3, 4);  /* 3..7 */
    company_assign_member(c, 0, carol->id, -1);
    assign_members_greedy(c, 0, SCHED_EARLIEST_DEADLINE);

    check("vacation is fixed_time",   vac1->fixed_time, 1);
    check("vacation start stays 3",   vac1->sched_start, 3);
    check("vacation end stays 7",     vac1->sched_end,   7);
    check("big assigned to Carol",    big->assignee_id,  carol->id);
    check("big rerouted after vac (7)", big->sched_start, 7);
    check("big ends at 17",           big->sched_end,    17);

    /* Scenario 2: a 2-day task fits before the same-shaped vacation -> stays at 0.
     * Fresh company so Carol has no other-project commitments (otherwise the
     * cross-project calendar would correctly push her, as scenario 1 leaves her
     * booked until day 17). */
    {
        Company *c2 = company_create("VacTest2");
        TeamMember *dave = company_add_member(c2, "Dave", "Engineer");
        team_member_set_skills(dave, SKILL_BACKEND);
        p2    = company_add_project(c2, "Fits", mkdate(2026, 1, 1));
        small = project_add_task(p2, "Small", ""); task_set_pert(small, 2,2,2); task_set_skills(small, SKILL_BACKEND);
        vac2  = project_add_fixed_block(p2, "[Vacation] Dave", dave->id, 3, 4);  /* 3..7 */
        company_assign_member(c2, 0, dave->id, -1);
        assign_members_greedy(c2, 0, SCHED_EARLIEST_DEADLINE);

        check("small assigned to Dave",   small->assignee_id, dave->id);
        check("small fits before vac (0)", small->sched_start, 0);
        check("small ends at 2",          small->sched_end,    2);
        check("vac2 start stays 3",       vac2->sched_start,   3);
        company_destroy(c2);
    }

    company_destroy(c);
    printf(g_fail ? "\nRESULT: FAIL\n" : "\nRESULT: PASS\n");
    return g_fail;
}
