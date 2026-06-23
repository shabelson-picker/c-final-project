#include <stdio.h>
#include "company.h"
#include "tui_framework.h"
#include "menus.h"
#include "ui.h"

int main(void) {
    Company *c = NULL;
    int choice;

    screen_clear();
    crumb_set_title("PROJECT MAYHEM MANAGEMENT");
    print_banner();

    /* Pick a company first, so signing out (then re-signing in as another role)
     * keeps the same company loaded - no reload on each role switch. */
    while (!c) {
        int r;
        printf(C_BOLD C_CYAN "\n  [1] New company\n  [2] Load company\n  [0] Exit\n" C_RESET);

        r = read_int("  > ", &choice);
        if (r == -1 && feof(stdin)) { goodbye(); return 0; }  /* EOF -> quit */
        if (r != 1) continue;                                 /* empty/invalid -> re-prompt */

        switch (choice) {
            case 1: c = company_new_interactive();  break;
            case 2: c = company_load_interactive(); break;
            case 0: goodbye(); return 0;
            default: cprintf(C_RED, "  Invalid option.\n"); break;
        }
    }

    /* Session loop: sign in as a role and run the company menu. "Sign out" from
     * the company menu returns here to re-run role sign-in (company stays
     * loaded); choosing Exit on the role menu quits. */
    while (!choose_role())
        menu_company(c);

    company_destroy(c);
    goodbye();
    return 0;
}
