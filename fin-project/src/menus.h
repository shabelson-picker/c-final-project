#ifndef MENUS_H
#define MENUS_H

#include "company.h"

/* ---- Company-level menu ------------------------------------------------- */

/// <summary>Top-level company menu loop (projects, team, save, exit).</summary>
/// <param name="c">The open company.</param>
void menu_company(Company *c);

/// <summary>
/// Interactive role sign-in: lists the roles from roles.cfg and sets the active
/// session's privilege mask. Loops until a valid choice; empty/EOF defaults to
/// System Admin.
/// </summary>
/// <returns>1 if the user chose to exit the program, 0 once a role is set.</returns>
int choose_role(void);

/* ---- Project-level menus (require active project + company context) ------ */

/// <summary>Task menu: list/add/remove/edit/status/link/deps/manual-assign.</summary>
/// <param name="p">Active project.</param>
void menu_tasks(Project *p);

/// <summary>Milestone menu: list / add.</summary>
/// <param name="p">Active project.</param>
void menu_milestones(Project *p);

/// <summary>Schedule menu: generate schedule, report, Gantt, DAG, exports. Shows the Gantt on entry.</summary>
/// <param name="c">Company (needed for member names / assignment).</param>
/// <param name="project_idx">Index of the project to schedule.</param>
void menu_schedule(Company *c, int project_idx);

/* Breadcrumb + line input + the menu runners are in tui_framework.h. */

#endif /* MENUS_H */
