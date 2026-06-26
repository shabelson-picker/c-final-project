#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "roles.h"

/* Token <-> bit <-> display-name table. Order is irrelevant. */
static const struct { const char *token; uint32_t bit; const char *name; } PRIV_TABLE[] = {
    { "VIEW_OWN",       PRIV_VIEW_OWN,       "view own tasks"   },
    { "VIEW_PROJECT",   PRIV_VIEW_PROJECT,   "view project"     },
    { "VIEW_PORTFOLIO", PRIV_VIEW_PORTFOLIO, "view portfolio"   },
    { "EDIT_TASK",      PRIV_EDIT_TASK,      "edit tasks"       },
    { "EDIT_DEPS",      PRIV_EDIT_DEPS,      "edit dependencies"},
    { "ASSIGN",         PRIV_ASSIGN,         "assign members"   },
    { "SCHEDULE",       PRIV_SCHEDULE,       "run scheduling"   },
    { "REPORT",         PRIV_REPORT,         "reports/exports"  },
    { "ADMIN",          PRIV_ADMIN,          "admin"            }
};
static const int PRIV_TABLE_N = (int)(sizeof PRIV_TABLE / sizeof PRIV_TABLE[0]);

/* Trim leading/trailing ASCII whitespace in place; returns the start pointer. */
static char *trim(char *s) {
    char *end;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

uint32_t priv_from_token(const char *token) {
    int i;
    for (i = 0; i < PRIV_TABLE_N; i++)
        if (strcmp(token, PRIV_TABLE[i].token) == 0) return PRIV_TABLE[i].bit;
    return 0;
}

const char *priv_name(Privilege p) {
    int i;
    for (i = 0; i < PRIV_TABLE_N; i++)
        if (PRIV_TABLE[i].bit == (uint32_t)p) return PRIV_TABLE[i].name;
    return "unknown";
}

/* OR together the bits named in a comma-separated privilege list. */
static uint32_t parse_priv_list(char *list) {
    uint32_t mask = 0;
    char *tok = strtok(list, ",");
    while (tok) {
        mask |= priv_from_token(trim(tok));
        tok = strtok(NULL, ",");
    }
    return mask;
}

int roles_load(const char *path, Role *roles, int max_roles) {
    FILE *f = fopen(path, "r");
    char  line[512];
    int   count = 0;
    int   have_pending = 0;   /* a "role:" seen, awaiting its privileges */

    if (!f) return -1;

    while (fgets(line, sizeof line, f)) {
        char *s = trim(line);
        if (*s == '\0' || *s == '#') continue;

        if (strncmp(s, "role:", 5) == 0) {
            if (count >= max_roles) break;
            str_copy(roles[count].name, trim(s + 5), sizeof roles[count].name);
            roles[count].name[sizeof roles[count].name - 1] = '\0';
            roles[count].privs = 0;
            have_pending = 1;
        } else if (strncmp(s, "privileges:", 11) == 0 && have_pending) {
            roles[count].privs = parse_priv_list(s + 11);
            count++;              /* role is complete once its privileges land */
            have_pending = 0;
        }
    }

    fclose(f);
    return count;
}

/* ---- current session ---------------------------------------------------- */

static uint32_t g_privs = 0;
static char     g_role_name[64] = "";

void roles_set_current(const char *role_name, uint32_t privs) {
    g_privs = privs;
    str_copy(g_role_name, role_name ? role_name : "", sizeof g_role_name);
    g_role_name[sizeof g_role_name - 1] = '\0';
}

uint32_t    roles_current(void)      { return g_privs; }
const char *roles_current_name(void) { return g_role_name; }

int priv_has(Privilege p) {
    if (g_privs & PRIV_ADMIN) return 1;   /* admin implies everything */
    return (g_privs & (uint32_t)p) != 0;
}

int priv_require(Privilege p) {
    if (priv_has(p)) return 1;
    printf("  Access denied: '%s' role lacks privilege [%s].\n",
           g_role_name[0] ? g_role_name : "(none)", priv_name(p));
    return 0;
}
