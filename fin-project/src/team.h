#ifndef TEAM_H
#define TEAM_H

#include "types.h"
#include "dynamic_int_array.h"
#include "task_ref_array.h"

typedef struct {
    int             id;
    char            name[MAX_NAME_LEN];
    char            role[MAX_NAME_LEN];     /* role name - maps to roles.cfg */
    uint32_t        skills;                 /* bitmask of Skill values */
    float           availability;           /* 0.0 - 1.0 fraction of a workday */
    DynamicIntArray project_ids;            /* company project IDs this member is on */
    TaskRefArray    tasks;                  /* (project_id, task_id) assignments */
} TeamMember;

TeamMember *team_member_create(int id, const char *name, const char *role);
void        team_member_destroy(TeamMember *m);
void        team_member_add_skill(TeamMember *m, Skill skill);
void        team_member_remove_skill(TeamMember *m, Skill skill);
int         team_member_has_skills(const TeamMember *m, uint32_t required);
void        team_member_print(const TeamMember *m);

#endif /* TEAM_H */
