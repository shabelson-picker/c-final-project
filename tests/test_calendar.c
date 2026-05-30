/* solo-ran verification: cross-project member calendar.
 *
 * Builds an in-memory company with ONE backend specialist (Carol) shared across
 * two projects and checks that greedy assignment serializes her across projects
 * instead of booking her from day 0 in both (the keystone bug).
 *
 * Pure in-memory: no disk I/O, never touches saved bundles. Provides its own
 * main(); compile with every src/*.c EXCEPT main.c.
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

/* Add a backend task of the given whole-day duration; return it. */
static Task *add_backend_task(Project *p, const char *title, float days) {
    Task *t = project_add_task(p, title, "");
    task_set_pert(t, days, days, days);     /* pert_expected == days */
    task_set_skills(t, SKILL_BACKEND);
    return t;
}

static Date mkdate(int y, int m, int d) { Date dt; dt.year = y; dt.month = m; dt.day = d; return dt; }

int main(void) {
    Company *c = company_create("CalTest");
    TeamMember *carol;
    Project *A, *B, *C;
    Task *ta, *tb, *tc;
    int ai, bi, ci;

    /* One backend specialist: the only person who can do any of these tasks, so
     * greedy is forced onto her in every project (no escape to another member). */
    carol = company_add_member(c, "Carol", "Engineer");
    team_member_add_skill(carol, SKILL_BACKEND);

    /* Project A and B share the SAME start date -> day frames align, so a correct
     * calendar must serialize B strictly after A. C starts 3 days after A -> tests
     * the date-offset arithmetic (not just same-frame serialization). */
    A = company_add_project(c, "Apollo",   mkdate(2026, 1, 1));
    B = company_add_project(c, "Borealis", mkdate(2026, 1, 1));
    C = company_add_project(c, "Cobalt",   mkdate(2026, 1, 4));   /* A.start + 3 */
    ai = 0; bi = 1; ci = 2;

    ta = add_backend_task(A, "A-build", 5.0f);   /* abs 0..5                       */
    tb = add_backend_task(B, "B-build", 6.0f);   /* abs 5..11 (after A)            */
    tc = add_backend_task(C, "C-build", 4.0f);   /* abs 11..15 (after A and B)     */

    /* Roster Carol onto each project (task_id -1 = roster only); greedy assigns. */
    company_assign_member(c, ai, carol->id, -1);
    company_assign_member(c, bi, carol->id, -1);
    company_assign_member(c, ci, carol->id, -1);

    /* Schedule in order A, B, C. Each run sees commitments locked by prior runs. */
    assign_members_greedy(c, ai, SCHED_EARLIEST_DEADLINE);
    assign_members_greedy(c, bi, SCHED_EARLIEST_DEADLINE);
    assign_members_greedy(c, ci, SCHED_EARLIEST_DEADLINE);

    printf("Cross-project member calendar\n");
    check("A assigned to Carol", ta->assignee_id, carol->id);
    check("A starts day 0",      ta->sched_start, 0);
    check("A ends day 5",        ta->sched_end,   5);

    check("B assigned to Carol", tb->assignee_id, carol->id);
    /* Same start date: Carol is busy in A until day 5, so B must wait to day 5. */
    check("B serialized after A (start=5)", tb->sched_start, 5);
    check("B ends day 11",                  tb->sched_end,   11);

    check("C assigned to Carol", tc->assignee_id, carol->id);
    /* By the time C is scheduled Carol is committed to A AND B, so she frees on
     * absolute day 11 (B's end). C starts 3 days after A, so in C's frame day 11
     * is 11-3 = 8. This proves the floor accumulates across multiple prior
     * projects and respects the calendar offset. */
    check("C after A+B, calendar-offset (start=8)", tc->sched_start, 8);
    check("C ends day 12",                          tc->sched_end,   12);

    company_destroy(c);

    printf(g_fail ? "\nRESULT: FAIL\n" : "\nRESULT: PASS\n");
    return g_fail;
}
