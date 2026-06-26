/* Smoke test for the table-driven menu dispatch (run_table_menu). Builds a
 * company in memory and drives menu_company via piped stdin, bypassing the
 * interactive entry/dir-browser. Verifies rendering + that a chosen option
 * dispatches to the right action. Own main(); links src/*.c minus main.c. */
#include <stdio.h>
#include "company.h"
#include "menus.h"

int main(void) {
    Company *c = company_create("SmokeCo");
    if (!c) { printf("alloc failed\n"); return 2; }
    snprintf(c->save_dir, sizeof c->save_dir, "%s", "tests\\bin\\smoke_co");
    company_add_member(c, "Alice", "Dev");

    /* stdin (piped by the runner) drives the menu: e.g. "3" -> Save, "0" -> back. */
    menu_company(c);

    company_destroy(c);
    printf("SMOKE_DONE\n");
    return 0;
}
