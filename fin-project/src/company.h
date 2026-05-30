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
} Company;

Company    *company_create(const char *name);
void        company_destroy(Company *c);

TeamMember *company_add_member(Company *c, const char *name, const char *role);
int         company_remove_member(Company *c, int member_id);
TeamMember *company_find_member(Company *c, int member_id);

Project    *company_add_project(Company *c, const char *name, Date start_date);
int         company_remove_project(Company *c, int project_idx);

/* Assign member to project roster and optionally to a task.
 * task_id = -1 means roster only, no task assignment. */
int         company_assign_member(Company *c, int project_idx,
                                  int member_id, int task_id);

void        company_print_summary(const Company *c);

/* Interactive create/load - prompt user for name and directory */
Company    *company_new_interactive(void);
Company    *company_load_interactive(void);

#endif /* COMPANY_H */
