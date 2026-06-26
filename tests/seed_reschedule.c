/* One-shot: re-plan the SHA_inc demo company with the unified company scheduler
 * and save it, so the bundled demo data shows the clean (non-ratcheted) plan. */
#include <stdio.h>
#include "company.h"
#include "algorithms.h"
#include "persistence.h"

int main(void) {
    Company *c = company_load("my_companies\\SHA_inc");
    if (!c) { printf("load failed\n"); return 2; }
    schedule_company(c, SCHED_EARLIEST_DEADLINE);
    printf(company_save(c) ? "saved OK\n" : "save FAILED\n");
    company_destroy(c);
    return 0;
}
