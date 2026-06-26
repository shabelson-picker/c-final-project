#ifndef TUI_FRAMEWORK_H
#define TUI_FRAMEWORK_H

#include "roles.h"   /* Privilege - for MenuItem.priv and run_table_menu gating */

/* ===========================================================================
 * Reusable text-UI layer: line input, a breadcrumb title bar, and table-driven
 * menus. Knows nothing about this app's domain - it builds only on ui.h (colour
 * + screen) and roles.h (privilege gating). The application supplies the menu
 * content (the MenuItem tables and their actions); this drives them.
 * ========================================================================= */

/* ---- Line input --------------------------------------------------------- */

/// <summary>Prompt for an integer (with an "(Enter to cancel)" hint).</summary>
/// <returns>1 on a valid number, 0 on invalid input, -1 if cancelled (empty).</returns>
int read_int(const char *prompt, int *out);

/// <summary>Prompt for an integer without the cancel hint (menu navigation).</summary>
/// <returns>1 on a valid number, 0 on invalid input, -1 if cancelled (empty).</returns>
int read_nav(const char *prompt, int *out);

/// <summary>Prompt for a string into a fixed buffer (trailing newline/CR trimmed).</summary>
/// <returns>1 on input, -1 if cancelled (empty).</returns>
int read_str(const char *prompt, char *buf, int max);

/// <summary>Prompt for a float.</summary>
/// <returns>1 on a valid number, 0 on invalid input, -1 if cancelled (empty).</returns>
int read_float(const char *prompt, float *out);

/* ---- Breadcrumb title bar ----------------------------------------------- */

#define MAX_CRUMB_LEN 32   /* capacity of a single breadcrumb label (incl. NUL) */

/// <summary>Set the application title shown in the breadcrumb header (call once).</summary>
void crumb_set_title(const char *title);

/// <summary>Push a label onto the breadcrumb trail (when entering a submenu).</summary>
void crumb_push(const char *name);

/// <summary>Pop the last breadcrumb label (when leaving a submenu).</summary>
void crumb_pop(void);

/// <summary>Clear the screen, then draw the title + breadcrumb trail.</summary>
void crumb_refresh(void);

/* ---- Table-driven menus ------------------------------------------------- */

/// <summary>
/// One menu entry: its label, the privilege required to use it (0 = none), and
/// the action it runs. Pairing the label with its action means the two can never
/// drift apart.
/// </summary>
typedef struct {
    const char *label;
    Privilege   priv;                /* 0 = no privilege gate */
    void      (*action)(void *ctx);
} MenuItem;

/// <summary>
/// Table-driven menu loop. Owns the rendering: auto-numbers the items 1..n, draws
/// "0. <back_label>", reads a choice, enforces each item's privilege, then
/// dispatches to its action. `items` ends at a NULL label. `render` (optional)
/// draws above the options each pass; `fallback` (optional) handles a choice not
/// in the table (e.g. a hidden code).
/// </summary>
void run_table_menu(void *ctx, void (*render)(void *ctx),
                    const MenuItem *items, const char *back_label,
                    void (*fallback)(void *ctx, int choice));

/// <summary>
/// Checklist loop: refresh + render, read a number; 0 confirms, anything else is
/// passed to toggle(). No pause - the redrawn checkmarks are the feedback.
/// </summary>
void run_checklist(void *ctx, void (*render)(void *ctx),
                   void (*toggle)(void *ctx, int n));

#endif /* TUI_FRAMEWORK_H */
