#ifndef UI_H
#define UI_H

/* Terminal screen helpers (ANSI; the app already relies on ANSI for colour). */

/* Clear the screen and move the cursor home. */
void screen_clear(void);

/* Print a prompt and block until the user presses Enter. */
void screen_pause(void);

/* Print the application's ASCII title banner (logo + tagline). */
void print_banner(void);

/* Print the parting message shown on program exit. */
void goodbye(void);

/* ---- ANSI colours ------------------------------------------------------- */
/* Concatenate with string LITERALS: printf(C_CYAN "label" C_RESET " %s\n", v).
 * For runtime strings or whole lines, use cprintf(). */
#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_DIM     "\033[2m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN    "\033[36m"

/* printf a line wrapped in `color`, auto-resetting after (color = a C_* macro). */
void cprintf(const char *color, const char *fmt, ...);

/* Alternating row colour for long lists/reports (pass the 0-based row index).
 * Emit it before a row and follow the row with C_RESET. */
const char *ui_zebra(int row);

/* Print a segmented progress bar of the given width:
 * green '#' = done, yellow '~' = in-progress, dim '.' = remaining. */
void ui_progress_bar(int done, int prog, int total, int width);

#endif /* UI_H */
