/* solo-ran verification: roles.cfg parsing + privilege model.
 * Parses the real roles.cfg (../../roles.cfg relative to tests\bin) and checks
 * masks + priv_has semantics (ADMIN implies all). Provides its own main().
 * Compile with src/*.c minus main.c.
 */
#include <stdio.h>
#include <string.h>
#include "roles.h"

static int g_fail = 0;
static void check(const char *what, long got, long want) {
    int ok = (got == want);
    printf("  [%s] %-34s got=0x%lx want=0x%lx\n", ok ? "PASS" : "FAIL", what,
           (unsigned long)got, (unsigned long)want);
    if (!ok) g_fail = 1;
}
static void check_b(const char *what, int got, int want) {
    int ok = (got == want);
    printf("  [%s] %-34s got=%d want=%d\n", ok ? "PASS" : "FAIL", what, got, want);
    if (!ok) g_fail = 1;
}

static uint32_t mask_of(const Role *r, int n, const char *name) {
    int i;
    for (i = 0; i < n; i++) if (strcmp(r[i].name, name) == 0) return r[i].privs;
    return 0xFFFFFFFFu;  /* sentinel: not found */
}

int main(void) {
    Role roles[MAX_ROLES];
    int  n = roles_load("roles.cfg", roles, MAX_ROLES);  /* run_test.ps1 runs from fin-project root */

    printf("Roles parsing + privileges (n=%d)\n", n);
    check_b("role count", n, 5);

    check("Executive mask",      mask_of(roles, n, "Executive"),
          PRIV_VIEW_PORTFOLIO | PRIV_REPORT);
    check("Project Manager mask", mask_of(roles, n, "Project Manager"),
          PRIV_VIEW_PROJECT | PRIV_EDIT_TASK | PRIV_EDIT_DEPS | PRIV_ASSIGN | PRIV_SCHEDULE | PRIV_REPORT);
    check("Team Lead mask",      mask_of(roles, n, "Team Lead"),
          PRIV_VIEW_PROJECT | PRIV_EDIT_TASK | PRIV_EDIT_DEPS | PRIV_ASSIGN | PRIV_SCHEDULE);
    check("Developer mask",      mask_of(roles, n, "Developer"),
          PRIV_VIEW_OWN | PRIV_VIEW_PROJECT);

    check("token VIEW_PROJECT",  priv_from_token("VIEW_PROJECT"), PRIV_VIEW_PROJECT);
    check("token unknown -> 0",  priv_from_token("NOPE"), 0);

    /* priv_has semantics */
    roles_set_current("Developer", mask_of(roles, n, "Developer"));
    check_b("dev has VIEW_PROJECT",  priv_has(PRIV_VIEW_PROJECT), 1);
    check_b("dev lacks SCHEDULE",    priv_has(PRIV_SCHEDULE), 0);
    check_b("dev lacks ADMIN",       priv_has(PRIV_ADMIN), 0);

    roles_set_current("System Admin", PRIV_ALL);
    check_b("admin has SCHEDULE",    priv_has(PRIV_SCHEDULE), 1);
    check_b("admin has ADMIN",       priv_has(PRIV_ADMIN), 1);

    /* ADMIN bit alone implies any privilege */
    roles_set_current("X", PRIV_ADMIN);
    check_b("ADMIN-only implies REPORT", priv_has(PRIV_REPORT), 1);

    printf(g_fail ? "\nRESULT: FAIL\n" : "\nRESULT: PASS\n");
    return g_fail;
}
