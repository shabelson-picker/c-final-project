#include <stdio.h>
#include <stdarg.h>
#include "ui.h"

void screen_clear(void) {
    printf("\033[2J\033[H");  /* clear screen + cursor home */
}

void screen_pause(void) {
    int c;
    printf("\n  Press Enter to continue...");
    while ((c = getchar()) != '\n' && c != EOF) {}
}

/* On-theme ASCII banner (ASCII-only - no backslashes/Unicode, avoids MSVC C4819).
 * The bottom strip is a mini Gantt motif: solid / expected / overrun zones. */
void print_banner(void) {
    cprintf(C_BOLD C_CYAN,
        "\n"
        "   ____  ____  ____  _ ____ ____ ___    __  __  ____ \n"
        "  |  _ \\|  _ \\|  _ \\| | ___|  __|_ _|  |  \\/  |/  __|\n"
        "  | |_) | |_) | |_) | | _| | |   | |   | |\\/| |\\__ \\ \n"
        "  |  __/|  _ <|  _ <| | |__| |__ | |   | |  | |___) |\n"
        "  |_|   |_| \\_\\_| \\_\\_|____|____|___|  |_|  |_|____/ \n");
    cprintf(C_BOLD, "         P R O J E C T   M A Y H E M   M G M T\n");
    cprintf(C_DIM,  "      [");
    cprintf(C_BOLD C_GREEN, "########");
    cprintf(C_DIM,  "");
    cprintf(C_YELLOW, "~~~~~");
    cprintf(C_DIM,  "----]  plan . schedule . ship\n\n");
}

void goodbye(void) {
    cprintf(C_DIM,
        "\n"
        "      .-----------------------------.\n"
        "      |  it's only after we've lost  |\n"
        "      |  everything that we're free  |\n"
        "      '-----------------------------'\n");
    cprintf(C_DIM, "      \"You met me at a very strange time in my life.\"\n\n");
}

void cprintf(const char *color, const char *fmt, ...) {
    va_list ap;
    fputs(color, stdout);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fputs(C_RESET, stdout);
}

const char *ui_zebra(int row) {
    return (row & 1) ? C_DIM : C_CYAN;
}

void ui_progress_bar(int done, int prog, int total, int width) {
    int d = total ? done * width / total : 0;
    int p = total ? prog * width / total : 0;
    int rest, k;
    if (d > width) d = width;
    if (d + p > width) p = width - d;
    rest = width - d - p;

    putchar('[');
    fputs(C_GREEN,  stdout); for (k = 0; k < d;    k++) putchar('#');
    fputs(C_YELLOW, stdout); for (k = 0; k < p;    k++) putchar('~');
    fputs(C_DIM,    stdout); for (k = 0; k < rest; k++) putchar('.');
    fputs(C_RESET,  stdout);
    putchar(']');
}
