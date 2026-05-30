#ifndef COMPANY_H
#define COMPANY_H

#include "team.h"
#include "project.h"

typedef struct {
    char         name[MAX_NAME_LEN];
    char         save_dir[MAX_PATH_LEN];
    TeamMember **members;
    int          member_count;
    int          member_capacity;
    int          next_member_id;
    Project    **projects;
    int          project_count;
    int          project_capacity;

    /* Derived id->TeamMember* index for company_find_member (rebuilt lazily when
     * dirty; see company_find_member). Not persisted. */
    TeamMember **mem_by_id;
    int          mem_by_id_cap;
    int          mem_by_id_dirty;
} Company;

/// <summary>Allocate a company (owns its members and projects) with default capacities.</summary>
/// <param name="name">Company name (copied).</param>
/// <returns>New company, or NULL on allocation failure.</returns>
Company    *company_create(const char *name);

/// <summary>Free the company and everything it owns (all members and projects).</summary>
/// <param name="c">Company to free (NULL-safe).</param>
void        company_destroy(Company *c);

/// <summary>Create a company member with an auto-assigned ID.</summary>
/// <param name="c">Company.</param>
/// <param name="name">Member name.</param>
/// <param name="role">Member role.</param>
/// <returns>The new member, or NULL on failure.</returns>
TeamMember *company_add_member(Company *c, const char *name, const char *role);

/// <summary>Remove a member by ID (swap-remove from the company array).</summary>
/// <param name="c">Company.</param>
/// <param name="member_id">Member ID to remove.</param>
/// <returns>1 if removed, 0 if not found.</returns>
int         company_remove_member(Company *c, int member_id);

/// <summary>Look up a member by ID (linear scan).</summary>
/// <param name="c">Company.</param>
/// <param name="member_id">Member ID.</param>
/// <returns>The member, or NULL if not found.</returns>
TeamMember *company_find_member(Company *c, int member_id);

/// <summary>Create a project owned by the company.</summary>
/// <param name="c">Company.</param>
/// <param name="name">Project name.</param>
/// <param name="start_date">Project start date.</param>
/// <returns>The new project, or NULL on failure.</returns>
Project    *company_add_project(Company *c, const char *name, Date start_date);

/// <summary>Remove a project by index (swap-remove).</summary>
/// <param name="c">Company.</param>
/// <param name="project_idx">Index into the company's project array.</param>
/// <returns>1 if removed, 0 if the index is invalid.</returns>
int         company_remove_project(Company *c, int project_idx);

/// <summary>
/// Add a member to a project's roster and, optionally, assign them to a task.
/// Keeps both the master (p->member_ids) and apprentice (m->project_ids) sorted.
/// task_id = -1 means roster-only (no task assignment, no skill check).
/// </summary>
/// <param name="c">Company.</param>
/// <param name="project_idx">Target project index.</param>
/// <param name="member_id">Member to assign.</param>
/// <param name="task_id">Task ID to assign, or -1 for roster only.</param>
/// <returns>1 on success, 0 if the project/member/task is invalid or the member lacks the task's skills.</returns>
int         company_assign_member(Company *c, int project_idx,
                                  int member_id, int task_id);

/// <summary>Print the company summary (members, projects, per-project progress bar) to stdout.</summary>
/// <param name="c">Company.</param>
void        company_print_summary(const Company *c);

/// <summary>Interactively create a new company: prompt for name + parent directory, then save the bundle.</summary>
/// <returns>The new company, or NULL if cancelled / failed.</returns>
Company    *company_new_interactive(void);

/// <summary>Interactively load a company: prompt for its bundle directory and read it.</summary>
/// <returns>The loaded company, or NULL if cancelled / failed.</returns>
Company    *company_load_interactive(void);

#endif /* COMPANY_H */
