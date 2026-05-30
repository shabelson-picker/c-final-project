#ifndef MENUS_H
#define MENUS_H

#include "company.h"

/* ---- Company-level menu ------------------------------------------------- */

/// <summary>Top-level company menu loop (projects, team, save, exit).</summary>
/// <param name="c">The open company.</param>
void menu_company(Company *c);

/* ---- Project-level menus (require active project + company context) ------ */

/// <summary>Task menu: list/add/remove/edit/status/link/deps/manual-assign.</summary>
/// <param name="p">Active project.</param>
void menu_tasks(Project *p);

/// <summary>Milestone menu: list / add.</summary>
/// <param name="p">Active project.</param>
void menu_milestones(Project *p);

/// <summary>Schedule menu: generate schedule, report, Gantt, DAG, exports. Shows the Gantt on entry.</summary>
/// <param name="c">Company (needed for member names / assignment).</param>
/// <param name="project_idx">Index of the project to schedule.</param>
void menu_schedule(Company *c, int project_idx);

/* ---- Breadcrumb navigation bar ------------------------------------------ */

/// <summary>Push a label onto the breadcrumb trail (when entering a submenu).</summary>
/// <param name="name">Crumb label.</param>
void crumb_push(const char *name);

/// <summary>Pop the last breadcrumb label (when leaving a submenu).</summary>
void crumb_pop(void);

/// <summary>Draw the title banner + breadcrumb trail WITHOUT clearing the screen.</summary>
void crumb_print(void);

/// <summary>Clear the screen, then draw the title banner + breadcrumb trail.</summary>
void crumb_refresh(void);

/* ---- Generic menu runner (opaque context) ------------------------------- */

/// <summary>
/// Generic menu loop over an opaque context: refresh, optional render, print
/// options, read a choice, dispatch to handler. Pauses after each action.
/// </summary>
/// <param name="ctx">Context passed to render/handler (cast inside them).</param>
/// <param name="render">Optional pre-options renderer, or NULL.</param>
/// <param name="options">Options line to display.</param>
/// <param name="handler">Choice handler; returns non-zero to exit the menu.</param>
void run_menu(void *ctx,
              void (*render)(void *ctx),
              const char *options,
              int  (*handler)(void *ctx, int choice));

/* ---- Shared input helpers ------------------------------------------------ */

/// <summary>Prompt for an integer (with an "(Enter to cancel)" hint).</summary>
/// <param name="prompt">Prompt text.</param>
/// <param name="out">Receives the parsed value on success.</param>
/// <returns>1 on a valid number, 0 on invalid input, -1 if cancelled (empty).</returns>
int read_int(const char *prompt, int *out);

/// <summary>Prompt for an integer without the cancel hint (menu navigation).</summary>
/// <param name="prompt">Prompt text.</param>
/// <param name="out">Receives the parsed value on success.</param>
/// <returns>1 on a valid number, 0 on invalid input, -1 if cancelled (empty).</returns>
int read_nav(const char *prompt, int *out);

/// <summary>Prompt for a string into a fixed buffer (newline trimmed).</summary>
/// <param name="prompt">Prompt text.</param>
/// <param name="buf">Destination buffer.</param>
/// <param name="max">Buffer size.</param>
/// <returns>1 on input, -1 if cancelled (empty).</returns>
int read_str(const char *prompt, char *buf, int max);

/// <summary>Prompt for a float.</summary>
/// <param name="prompt">Prompt text.</param>
/// <param name="out">Receives the parsed value on success.</param>
/// <returns>1 on a valid number, 0 on invalid input, -1 if cancelled (empty).</returns>
int read_float(const char *prompt, float *out);

#endif /* MENUS_H */
