#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tui_framework.h"
#include "ui.h"      /* screen_clear / screen_pause / cprintf / C_* colours */
#include "util.h"    /* str_copy */

/* ---- line input --------------------------------------------------------- */

static int read_int_impl(const char *prompt, int *out, int show_hint) {
    char buf[64], *end;

    if (show_hint)
        printf("%s(Enter to cancel) ", prompt);
    else
        printf("%s", prompt);

    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    if (buf[0] == '\n' || buf[0] == '\0') return -1;

    *out = (int)strtol(buf, &end, 10);
    while (*end == ' ' || *end == '\t' || *end == '\r') end++;   /* tolerate CRLF */
    if (end == buf || (*end != '\n' && *end != '\0')) {
        printf("  Please enter a whole number.\n");
        return 0;
    }
    return 1;
}

int read_int(const char *prompt, int *out) { return read_int_impl(prompt, out, 1); }
int read_nav(const char *prompt, int *out) { return read_int_impl(prompt, out, 0); }

int read_str(const char *prompt, char *buf, int max) {
    size_t len;
    printf("%s(Enter to cancel) ", prompt);
    if (!fgets(buf, max, stdin)) return -1;
    len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) buf[--len] = '\0';  /* strip CRLF */
    if (len == 0) return -1;
    return 1;
}

int read_float(const char *prompt, float *out) {
    char buf[64], *end;

    printf("%s(Enter to cancel) ", prompt);
    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    if (buf[0] == '\n' || buf[0] == '\0') return -1;

    *out = strtof(buf, &end);
    while (*end == ' ' || *end == '\t' || *end == '\r') end++;   /* tolerate CRLF */
    if (end == buf || (*end != '\n' && *end != '\0')) {
        printf("  Please enter a number.\n");
        return 0;
    }
    return 1;
}

/* ---- breadcrumb title bar ----------------------------------------------- */

#define MAX_CRUMB_DEPTH   8
#define BANNER_RULE_WIDTH 44   /* width of the "===" rules around the title bar */
/* MAX_CRUMB_LEN is public (tui_framework.h) - callers size crumb labels with it */

static char g_title[64] = "";
static char g_crumbs[MAX_CRUMB_DEPTH][MAX_CRUMB_LEN];
static int  g_crumb_depth = 0;

void crumb_set_title(const char *title) {
    str_copy(g_title, title, sizeof g_title);
}

void crumb_push(const char *name) {
    if (g_crumb_depth < MAX_CRUMB_DEPTH)
        str_copy(g_crumbs[g_crumb_depth++], name, MAX_CRUMB_LEN);
}

void crumb_pop(void) {
    if (g_crumb_depth > 0) g_crumb_depth--;
}

/* Clear the screen and draw the title + breadcrumb trail. */
static void crumb_draw(void) {
    int i;
    screen_clear();
    for (i = 0; i < BANNER_RULE_WIDTH; i++) putchar('=');
    printf("\n  " C_BOLD C_CYAN "%s" C_RESET, g_title);
    if (roles_current_name()[0]) printf(C_DIM "   [role: %s]" C_RESET, roles_current_name());
    printf("\n  ");
    for (i = 0; i < g_crumb_depth; i++) {
        if (i > 0) printf(" > ");
        if (i == g_crumb_depth - 1)
            printf(C_BOLD C_CYAN "%s" C_RESET, g_crumbs[i]);   /* current */
        else
            printf(C_DIM "%s" C_RESET, g_crumbs[i]);           /* ancestors */
    }
    printf("\n");
    for (i = 0; i < BANNER_RULE_WIDTH; i++) putchar('=');
    printf("\n");
}

void crumb_refresh(void) { crumb_draw(); }  /* clear + header */

/* ---- table-driven menus ------------------------------------------------- */

void run_table_menu(void *ctx, void (*render)(void *ctx),
                    const MenuItem *items, const char *back_label,
                    void (*fallback)(void *ctx, int choice)) {
    int choice, i, n;
    for (n = 0; items[n].label; n++) { }
    for (;;) {
        crumb_refresh();
        if (render) render(ctx);
        /* all options on one line: colour once, printf each (no newline), end the line */
        fputs(C_BOLD C_CYAN, stdout);
        for (i = 0; i < n; i++) printf("  %d. %s", i + 1, items[i].label);
        printf("  0. %s", back_label);
        fputs(C_RESET "\n", stdout);
        { int _r = read_nav("  > ", &choice);
          if (_r == -1 && feof(stdin)) break;                 /* EOF -> leave menu */
          if (_r != 1) { screen_pause(); continue; } }
        if (choice == 0) break;
        if (choice >= 1 && choice <= n) {
            const MenuItem *it = &items[choice - 1];
            if (it->priv == 0 || priv_require(it->priv)) it->action(ctx);
        } else if (fallback) {
            fallback(ctx, choice);
        } else {
            cprintf(C_RED, "  Invalid option.\n");
        }
        screen_pause();
    }
}

void run_checklist(void *ctx, void (*render)(void *ctx),
                   void (*toggle)(void *ctx, int n)) {
    int n;
    for (;;) {
        crumb_refresh();
        render(ctx);
        { int _r = read_nav("  > ", &n);
          if (_r == -1 && feof(stdin)) break;                 /* EOF -> leave checklist */
          if (_r != 1) continue; }
        if (n == 0) break;
        toggle(ctx, n);
    }
}
