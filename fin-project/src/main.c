#include <stdio.h>
#include <stdlib.h>
#include "company.h"
#include "menus.h"
#include "ui.h"
#include "roles.h"
#include "file_browser.h"
#include "constants.h"

static void goodbye(void) {
    cprintf(C_DIM, "\n  \"You met me at a very strange time in my life.\"\n\n");
}

/* Print the privileges a mask grants, comma-separated (dim), or "all". */
static void print_privs(uint32_t mask) {
    static const Privilege BITS[] = {
        PRIV_VIEW_OWN, PRIV_VIEW_PROJECT, PRIV_VIEW_PORTFOLIO, PRIV_EDIT_TASK,
        PRIV_EDIT_DEPS, PRIV_ASSIGN, PRIV_SCHEDULE, PRIV_REPORT, PRIV_ADMIN
    };
    int i, first = 1;
    if (mask == PRIV_ALL) { cprintf(C_DIM, "all privileges"); return; }
    for (i = 0; i < (int)(sizeof BITS / sizeof BITS[0]); i++) {
        if (mask & BITS[i]) {
            cprintf(C_DIM, "%s%s", first ? "" : ", ", priv_name(BITS[i]));
            first = 0;
        }
    }
    if (first) cprintf(C_DIM, "none");
}

/* Pick a role at startup as a selectable menu; sets the session privilege mask.
 * roles.cfg is looked up in the working directory, then next to the executable.
 * If it can't be read, only the all-powerful System Admin is offered. The menu
 * loops until a valid choice is entered (empty line / EOF defaults to Admin). */
static void choose_role(void) {
    Role roles[MAX_ROLES];
    int  n, i, sel = 0;

    n = roles_load("roles.cfg", roles, MAX_ROLES);
    if (n <= 0) {
        char dir[MAX_PATH_LEN], path[MAX_PATH_LEN];
        get_exe_dir(dir, MAX_PATH_LEN);
        if (dir[0]) {
            snprintf(path, sizeof path, "%s\\roles.cfg", dir);
            n = roles_load(path, roles, MAX_ROLES);
        }
    }
    if (n < 0) { cprintf(C_DIM, "  (roles.cfg not found - System Admin only)\n"); n = 0; }

    cprintf(C_BOLD C_CYAN, "\n  Choose your role:\n");
    printf("    [0] %-16s - ", "System Admin"); print_privs(PRIV_ALL);          printf("\n");
    for (i = 0; i < n; i++) {
        printf("    [%d] %-16.16s - ", i + 1, roles[i].name); print_privs(roles[i].privs); printf("\n");
    }

    for (;;) {
        int r = read_int("  Select role: ", &sel);
        if (r == -1) { sel = 0; break; }                  /* empty / EOF -> default admin */
        if (r == 1 && sel >= 0 && sel <= n) break;        /* valid selection */
        cprintf(C_YELLOW, "  Enter a number between 0 and %d.\n", n);
    }

    if (sel == 0) roles_set_current("System Admin", PRIV_ALL);
    else          roles_set_current(roles[sel - 1].name, roles[sel - 1].privs);

    cprintf(C_GREEN, "  Signed in as %s.\n", roles_current_name());
}

int main(void) {
    Company *c = NULL;
    int choice;

    screen_clear();
    printf("========================================\n");
    printf("  " C_BOLD C_CYAN "Project Mayhem Management" C_RESET "\n");
    printf("========================================\n");

    choose_role();

    while (!c) {
        printf(C_BOLD C_CYAN "\n  [1] New company\n  [2] Load company\n  [0] Exit\n" C_RESET);

        if (!read_int("  > ", &choice)) continue;

        switch (choice) {
            case 1: c = company_new_interactive();  break;
            case 2: c = company_load_interactive(); break;
            case 0: goodbye(); return 0;
            default: cprintf(C_RED, "  Invalid option.\n"); break;
        }
    }

    menu_company(c);

    company_destroy(c);
    goodbye();
    return 0;
}
