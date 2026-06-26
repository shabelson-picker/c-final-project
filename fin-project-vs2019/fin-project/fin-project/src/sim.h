#ifndef SIM_H
#define SIM_H

#include "company.h"

/* Day-by-day sandbox simulation of one project. Runs on an in-memory clone of
 * the company (cloned via a temp bundle), so the real project is NOT changed
 * unless the manager explicitly applies the sandbox at the end. Each turn is a
 * day; advancing auto-completes tasks whose scheduled end has passed, and the
 * manager can inject events (complete/cancel/add a task, an employee quits, hire
 * someone) and re-plan. */
void menu_simulate(Company *c, int project_idx);

/* Advance simulation to `day`: any assigned, not-yet-finished task whose
 * sched_end <= day is marked DONE. Returns the number newly completed.
 * Pure (no I/O) - unit-testable. */
int  sim_advance_day(Project *p, int day);

#endif /* SIM_H */
