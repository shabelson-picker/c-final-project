#include <stdio.h>
#include <stdlib.h>
#include "company.h"
#include "menus.h"

int main(void) {
    Company *c = NULL;
    int choice;

    printf("========================================\n");
    printf("  Project Forge\n");
    printf("========================================\n");

    while (!c) {
        printf("\n  [1] New company\n");
        printf("  [2] Load company\n");
        printf("  [0] Exit\n");

        if (!read_int("  > ", &choice)) continue;

        switch (choice) {
            case 1: c = company_new_interactive();  break;
            case 2: c = company_load_interactive(); break;
            case 0: return 0;
            default: printf("  Invalid option.\n"); break;
        }
    }

    menu_company(c);

    company_destroy(c);
    return 0;
}
