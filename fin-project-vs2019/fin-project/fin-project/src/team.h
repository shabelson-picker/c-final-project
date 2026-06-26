#ifndef TEAM_H
#define TEAM_H

#include "types.h"
#include "dynamic_int_array.h"

/// <summary>
/// A company team member. skills is a bitmask of Skill values. project_ids is a
/// DERIVED list of project indices the member is on (rebuilt from each project's
/// roster on load - not the source of truth).
/// </summary>
typedef struct {
    int             id;
    char            name[MAX_NAME_LEN];
    char            role[MAX_NAME_LEN];     /* role name - maps to roles.cfg */
    uint32_t        skills;                 /* bitmask of Skill values */
    float           availability;           /* 0.0 - 1.0 fraction of a workday */
    DynamicIntArray project_ids;            /* derived: company project indices */
} TeamMember;

/// <summary>Allocate a team member with availability defaulted to 1.0.</summary>
/// <param name="id">Member ID.</param>
/// <param name="name">Display name (copied).</param>
/// <param name="role">Role name (copied).</param>
/// <returns>New member, or NULL on allocation failure.</returns>
TeamMember *team_member_create(int id, const char *name, const char *role);

/// <summary>Free a member and its owned arrays.</summary>
/// <param name="m">Member to free (NULL-safe).</param>
void        team_member_destroy(TeamMember *m);

/// <summary>Set the member's skill bitmask (combine Skill values with |).
/// Bits outside the defined skill set are ignored.</summary>
/// <param name="m">Member.</param>
/// <param name="skills">Bitmask of Skill values the member has (0 = none).</param>
void        team_member_set_skills(TeamMember *m, uint32_t skills);

/// <summary>Test whether the member has ALL required skills.</summary>
/// <param name="m">Member.</param>
/// <param name="required">Bitmask of required skills.</param>
/// <returns>1 if the member covers every required bit, 0 otherwise.</returns>
int         team_member_has_skills(const TeamMember *m, uint32_t required);

/// <summary>Print a one-line summary (name, role, availability, skills) to stdout.</summary>
/// <param name="m">Member to print.</param>
void        team_member_print(const TeamMember *m);

#endif /* TEAM_H */
