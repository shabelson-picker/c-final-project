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
