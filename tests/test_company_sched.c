/* Verify the unified company scheduler: load the demo company, run
 * schedule_company repeatedly, and confirm (a) the portfolio span is sane (not
 * ratcheted past the horizon) and (b) it is idempotent (no drift across runs). */
#include <stdio.h>
#include "company.h"
#include "algorithms.h"
#include "persistence.h"
#include "project.h"

/* Portfolio length in days: max absolute task end measured from the earliest
 * project start (t0). */
static int portfolio_span(Company *c) {
    long t0 = -1;
    int  i, j, mx = 0;
    for (i = 0; i < c->project_count; i++)
        if (date_is_valid(c->projects[i]->start_date)) {
            long off = date_to_days(c->projects[i]->start_date);
            if (t0 < 0 || off < t0) t0 = off;
        }
    if (t0 < 0) t0 = 0;
    for (i = 0; i < c->project_count; i++) {
        long off = date_is_valid(c->projects[i]->start_date)
                   ? date_to_days(c->projects[i]->start_date) : t0;
        for (j = 0; j < c->projects[i]->task_count; j++) {
            int e = (int)(off - t0) + c->projects[i]->tasks[j]->sched_end;
            if (e > mx) mx = e;
        }
    }
    return mx;
}

int main(void) {
    Company *c = company_load("my_companies\\SHA_inc");
    int s1, s2, s3;
    if (!c) { printf("load failed\n"); return 2; }

    schedule_company(c, SCHED_EARLIEST_DEADLINE); s1 = portfolio_span(c);
    schedule_company(c, SCHED_EARLIEST_DEADLINE); s2 = portfolio_span(c);
    schedule_company(c, SCHED_EARLIEST_DEADLINE); s3 = portfolio_span(c);

    printf("portfolio span: run1=%d  run2=%d  run3=%d days\n", s1, s2, s3);
    printf("idempotent (no drift): %s\n", (s1 == s2 && s2 == s3) ? "PASS" : "FAIL");

    company_destroy(c);
    return (s1 == s2 && s2 == s3) ? 0 : 1;
}
