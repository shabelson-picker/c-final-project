/* solo-ran verification: executive HTML report (structure check).
 * Builds a small company with an over-allocated member, exports the executive
 * report, and the run_test harness output is inspected for the expected sections.
 * Own main(); compile with src/*.c minus main.c.
 */
#include <stdio.h>
#include "company.h"
#include "report_exporter.h"

static Date mkdate(int y,int m,int d){ Date dt; dt.year=y; dt.month=m; dt.day=d; return dt; }
static void task(Project *p, const char *t, int s, int e, int who, TaskStatus st) {
    Task *x = project_add_task(p, t, "");
    x->sched_start=s; x->sched_end=e; x->assignee_id=who; x->status=st;
}

int main(void) {
    Company *c = company_create("Mayhem Inc");
    TeamMember *carol = company_add_member(c, "Carol Wu", "Engineer");
    TeamMember *dave  = company_add_member(c, "Dave Lin", "Designer");
    Project *a = company_add_project(c, "Apollo",   mkdate(2026,1,1));
    Project *b = company_add_project(c, "Borealis", mkdate(2026,1,1));
    task(a, "A1", 0, 5, carol->id, STATUS_DONE);
    task(a, "A2", 3, 9, carol->id, STATUS_IN_PROGRESS);   /* overlaps A1 -> overalloc */
    task(b, "B1", 0, 4, dave->id,  STATUS_DONE);
    (void)b;

    export_executive_report_html(c, "tests\\bin\\exec_report.html");
    company_destroy(c);
    return 0;
}
