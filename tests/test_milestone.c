/* solo-ran verification: milestone status (forecast vs deadline).
 * Build a chain, attach milestones with different deadlines, schedule, and check
 * ON TRACK / AT RISK / LATE / NO DATA classification. Own main(); src/*.c minus main.c.
 */
#include <stdio.h>
#include "company.h"
#include "algorithms.h"

static int g_fail = 0;
static void check(const char *what, int got, int want) {
    int ok = (got == want);
    printf("  [%s] %-30s got=%d want=%d\n", ok ? "PASS" : "FAIL", what, got, want);
    if (!ok) g_fail = 1;
}
static Date mkdate(int y,int m,int d){ Date dt; dt.year=y; dt.month=m; dt.day=d; return dt; }

int main(void) {
    Company *c = company_create("MS");
    Project *p = company_add_project(c, "P", mkdate(2026,1,1));
    Task *a = project_add_task(p, "A", ""); task_set_pert(a, 5,5,5);
    Task *b = project_add_task(p, "B", ""); task_set_pert(b, 5,5,5);
    Milestone *m_late, *m_risk, *m_ok, *m_none;
    project_link_tasks(p, a->id, b->id);        /* A:0..5, B:5..10 */
    schedule_project(p, SCHED_EARLIEST_DEADLINE);

    printf("Milestone status (B forecast = day 10)\n");
    check("B ends day 10", b->sched_end, 10);

    m_late = project_add_milestone(p, "Tight",   8,  1);  /* forecast 10 > 8  -> LATE   */
    m_risk = project_add_milestone(p, "Close",   11, 1);  /* 10 within 2 of 11 -> AT RISK */
    m_ok   = project_add_milestone(p, "Comfy",   20, 1);  /* 10 << 20         -> ON TRACK */
    m_none = project_add_milestone(p, "Empty",   15, 1);  /* no tasks         -> NO DATA  */
    project_link_task_milestone(p, b->id, m_late->id);
    project_link_task_milestone(p, b->id, m_risk->id);
    project_link_task_milestone(p, b->id, m_ok->id);

    check("forecast = 10",        milestone_forecast_day(p, m_late), 10);
    check("tight  -> LATE",       milestone_status(p, m_late),  MS_LATE);
    check("close  -> AT RISK",    milestone_status(p, m_risk),  MS_AT_RISK);
    check("comfy  -> ON TRACK",   milestone_status(p, m_ok),    MS_ON_TRACK);
    check("empty  -> NO DATA",    milestone_status(p, m_none),  MS_NO_DATA);

    company_destroy(c);
    printf(g_fail ? "\nRESULT: FAIL\n" : "\nRESULT: PASS\n");
    return g_fail;
}
