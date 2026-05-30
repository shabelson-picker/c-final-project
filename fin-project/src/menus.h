#ifndef MENUS_H
#define MENUS_H

#include "company.h"

/* ---- Company-level menu ------------------------------------------------- */
void menu_company(Company *c);

/* ---- Project-level menus (require active project + company context) ------ */
void menu_tasks(Project *p);
void menu_milestones(Project *p);
void menu_schedule(Company *c, int project_idx);

/* ---- Breadcrumb navigation bar ------------------------------------------ */
void crumb_push(const char *name);
void crumb_pop(void);
void crumb_print(void);

/* ---- Generic menu runner ------------------------------------------------- */
void run_menu(Project *p,
              void (*render)(Project *p),
              const char *options,
              int  (*handler)(Project *p, int choice));

/* ---- Shared input helpers ------------------------------------------------ */
int read_int(const char *prompt, int *out);
int read_nav(const char *prompt, int *out);
int read_str(const char *prompt, char *buf, int max);
int read_float(const char *prompt, float *out);

#endif /* MENUS_H */
