#include <stdio.h>
#include <stdlib.h>
#include "company.h"
#include "menus.h"
#include "ui.h"

static void goodbye(void) {
    cprintf(C_DIM, "\n  \"You met me at a very strange time in my life.\"\n\n");
}

int main(void) {
    Company *c = NULL;
    int choice;

    screen_clear();
    printf("========================================\n");
    printf("  " C_BOLD C_CYAN "Project Mayhem Management" C_RESET "\n");
    printf("========================================\n");

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
