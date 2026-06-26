#ifndef ROLES_H
#define ROLES_H

#include <stdint.h>

/* Privilege bits. Vocabulary matches roles.cfg. ADMIN implies every other
 * privilege (see priv_has). Combine with | into a role's privilege mask. */
typedef enum {
    PRIV_VIEW_OWN       = 1u << 0,  /* view own assigned tasks                  */
    PRIV_VIEW_PROJECT   = 1u << 1,  /* view a full project (tasks/schedule/Gantt)*/
    PRIV_VIEW_PORTFOLIO = 1u << 2,  /* view all projects across the company     */
    PRIV_EDIT_TASK      = 1u << 3,  /* create / edit / remove tasks             */
    PRIV_EDIT_DEPS      = 1u << 4,  /* add / remove dependencies                */
    PRIV_ASSIGN         = 1u << 5,  /* assign members, manage roster/vacations  */
    PRIV_SCHEDULE       = 1u << 6,  /* run scheduling algorithms                */
    PRIV_REPORT         = 1u << 7,  /* reports and exports                      */
    PRIV_ADMIN          = 1u << 8   /* system-level; implies all of the above   */
} Privilege;

#define PRIV_ALL   0x1FFu           /* all nine bits set */
#define MAX_ROLES  16

typedef struct {
    char     name[64];
    uint32_t privs;
} Role;

/* Parse a roles.cfg-format file into roles[] (up to max_roles).
 * Returns the number of roles parsed, or -1 if the file cannot be opened.
 * Unknown privilege tokens are skipped; comment (#) and blank lines ignored. */
int        roles_load(const char *path, Role *roles, int max_roles);

/* Map a single privilege token (e.g. "VIEW_PROJECT") to its bit; 0 if unknown. */
uint32_t   priv_from_token(const char *token);

/* Display name for a single-bit privilege (for "access denied" messages). */
const char *priv_name(Privilege p);

/* ---- current session ---------------------------------------------------- */

/* Set the active session's role name + privilege mask. */
void       roles_set_current(const char *role_name, uint32_t privs);

/* The active mask / role name (name is "" before any login). */
uint32_t   roles_current(void);
const char *roles_current_name(void);

/* 1 if the session holds privilege p (ADMIN satisfies any p); else 0. */
int        priv_has(Privilege p);

/* priv_has(p); if not, print an "access denied" line naming p and return 0. */
int        priv_require(Privilege p);

#endif /* ROLES_H */
