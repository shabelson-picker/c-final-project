#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <windows.h>
#include "file_browser.h"
#include "constants.h"
#include "menus.h"
#include "company.h"
#include "algorithms.h"
#include "renderer.h"
#include "dot_export.h"
#include "persistence.h"

/* GANTT_WIDTH defined in constants.h */

/* ---- input helpers ------------------------------------------------------ */

static void flush_input(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

static int read_int_impl(const char *prompt, int *out, int show_hint) {
    char buf[64], *end;

    if (show_hint)
        printf("%s(Enter to cancel) ", prompt);
    else
        printf("%s", prompt);

    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    if (buf[0] == '\n' || buf[0] == '\0') return -1;

    *out = (int)strtol(buf, &end, 10);
    while (*end == ' ' || *end == '\t') end++;
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
    if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';
    if (len == 0) return -1;
    return 1;
}

int read_float(const char *prompt, float *out) {
    char buf[64], *end;

    printf("%s(Enter to cancel) ", prompt);
    if (!fgets(buf, sizeof(buf), stdin)) return -1;
    if (buf[0] == '\n' || buf[0] == '\0') return -1;

    *out = strtof(buf, &end);
    while (*end == ' ' || *end == '\t') end++;
    if (end == buf || (*end != '\n' && *end != '\0')) {
        printf("  Please enter a number.\n");
        return 0;
    }
    return 1;
}

/* Forward declaration — defined in member menu section */
static uint32_t select_skills(void);

/* ---- breadcrumb --------------------------------------------------------- */

#define MAX_CRUMB_DEPTH 8
#define MAX_CRUMB_LEN   32

static char g_crumbs[MAX_CRUMB_DEPTH][MAX_CRUMB_LEN];
static int  g_crumb_depth = 0;

void crumb_push(const char *name) {
    if (g_crumb_depth < MAX_CRUMB_DEPTH)
        strncpy(g_crumbs[g_crumb_depth++], name, MAX_CRUMB_LEN - 1);
}

void crumb_pop(void) {
    if (g_crumb_depth > 0) g_crumb_depth--;
}

void crumb_print(void) {
    int i;
    printf("\n  ");
    for (i = 0; i < g_crumb_depth; i++) {
        if (i > 0) printf(" > ");
        printf("%s", g_crumbs[i]);
    }
    printf("\n  ");
    for (i = 0; i < 44; i++) printf("-");
    printf("\n");
}

/* ---- auto-save ---------------------------------------------------------- */

static Company *g_company = NULL;  /* set when entering a project context */

static void autosave(Project *p) {
    if (p->save_dir[0] == '\0') {
        printf("  [warning] auto-save skipped — no directory set.\n");
        return;
    }
    if (g_company) company_save(g_company);
    else if (!project_save(p, p->save_dir))
        printf("  [warning] auto-save failed for '%s'\n", p->save_dir);
}

/* ---- generic menu runner ------------------------------------------------ */

void run_menu(Project *p,
              void (*render)(Project *p),
              const char *options,
              int  (*handler)(Project *p, int choice)) {
    int choice;
    for (;;) {
        crumb_print();
        if (render) render(p);
        printf("%s\n", options);
        { int _r = read_nav("  > ", &choice); if (_r != 1) continue; }
        if (handler(p, choice)) break;
    }
}

/* ---- task menu ---------------------------------------------------------- */

#define CANCELLED(r) do { if ((r) == -1) { printf("  Cancelled.\n"); return; } } while(0)

static void add_task(Project *p) {
    char title[MAX_TITLE_LEN], desc[MAX_DESC_LEN];
    float mn = 0, lt = 0, mx = 0, risk = 0;
    Task *t;
    int r;

    CANCELLED(read_str("  Title: ", title, MAX_TITLE_LEN));
    CANCELLED(read_str("  Description: ", desc, MAX_DESC_LEN));

    for (;;) {
        r = read_float("  Optimistic duration, days (best case):   ", &mn);
        CANCELLED(r);
        if (r == 0) continue;
        if (mn < 0) { printf("  Cannot be negative.\n"); continue; }
        break;
    }
    for (;;) {
        r = read_float("  Most likely duration, days:               ", &lt);
        CANCELLED(r);
        if (r == 0) continue;
        if (lt < mn) { printf("  Must be >= optimistic (%.1f).\n", mn); continue; }
        break;
    }
    for (;;) {
        r = read_float("  Pessimistic duration, days (worst case):  ", &mx);
        CANCELLED(r);
        if (r == 0) continue;
        if (mx < lt) { printf("  Must be >= most likely (%.1f).\n", lt); continue; }
        break;
    }
    {
        int risk_int = 0;
        for (;;) {
            r = read_int("  Risk level (0 = none, 10 = critical): ", &risk_int);
            CANCELLED(r);
            if (r == 0) continue;
            if (risk_int < 0 || risk_int > 10) { printf("  Must be between 0 and 10.\n"); continue; }
            break;
        }
        risk = risk_int / 10.0f;
    }

    t = project_add_task(p, title, desc);
    if (!t) { printf("  Error: could not create task.\n"); return; }

    task_set_pert(t, mn, lt, mx);
    task_set_risk(t, risk);
    printf("  Task [%d] created. Expected duration: %.1f days.\n", t->id, t->pert_expected);
    autosave(p);
}

static void list_tasks(Project *p) {
    int i;
    if (p->task_count == 0) { printf("  No tasks.\n"); return; }
    printf("\n");
    for (i = 0; i < p->task_count; i++)
        task_print(p->tasks[i]);
}

static void link_tasks(Project *p) {
    int pre_id, post_id;
    Task *post;
    CANCELLED(read_int("  Pre-task ID: ",  &pre_id));
    CANCELLED(read_int("  Post-task ID: ", &post_id));
    if (!project_link_tasks(p, pre_id, post_id)) {
        printf("  Error: task not found.\n");
        return;
    }
    printf("  Linked %d --> %d\n", pre_id, post_id);
    autosave(p);
    post = project_find_task(p, post_id);
    if (post) render_task_deps(p, post);
}

static void remove_task(Project *p) {
    int id;
    list_tasks(p);
    CANCELLED(read_int("  Task ID to remove: ", &id));
    if (project_remove_task(p, id)) {
        printf("  Task [%d] removed.\n", id);
        autosave(p);
    } else {
        printf("  Task [%d] not found.\n", id);
    }
}

static void change_status(Project *p) {
    static const char *STATUS_LABELS[] = {
        "Todo", "In Progress", "Done", "Cancelled", "Blocked"
    };
    int id, choice, i;
    Task *t;

    list_tasks(p);
    CANCELLED(read_int("  Task ID: ", &id));
    t = project_find_task(p, id);
    if (!t) { printf("  Task [%d] not found.\n", id); return; }

    printf("\n  Current status: %s\n", STATUS_LABELS[t->status]);
    for (i = 0; i < 5; i++)
        printf("  [%d] %s\n", i, STATUS_LABELS[i]);

    CANCELLED(read_int("  New status: ", &choice));
    if (choice < 0 || choice > 4) { printf("  Invalid status.\n"); return; }

    t->status = (TaskStatus)choice;
    printf("  Task [%d] status -> %s\n", id, STATUS_LABELS[t->status]);
    autosave(p);
}

static void edit_task(Project *p) {
    int id, field, r;
    Task *t;

    list_tasks(p);
    CANCELLED(read_int("  Task ID to edit: ", &id));
    t = project_find_task(p, id);
    if (!t) { printf("  Task [%d] not found.\n", id); return; }

    printf("\n  Editing: [%d] %s\n", t->id, t->title);
    printf("  [1] Title\n");
    printf("  [2] Description\n");
    printf("  [3] Durations (PERT)\n");
    printf("  [4] Risk level\n");
    printf("  [5] Required skills\n");
    printf("  [0] Cancel\n");

    CANCELLED(read_int("  Field: ", &field));

    switch (field) {
        case 1: {
            char title[MAX_TITLE_LEN];
            CANCELLED(read_str("  New title: ", title, MAX_TITLE_LEN));
            strncpy(t->title, title, MAX_TITLE_LEN - 1);
            break;
        }
        case 2: {
            char desc[MAX_DESC_LEN];
            CANCELLED(read_str("  New description: ", desc, MAX_DESC_LEN));
            strncpy(t->description, desc, MAX_DESC_LEN - 1);
            break;
        }
        case 3: {
            float mn = 0, lt = 0, mx = 0;
            printf("  Current: %.1f / %.1f / %.1f\n", t->pert_min, t->pert_likely, t->pert_max);
            for (;;) { r = read_float("  Optimistic: ", &mn); CANCELLED(r); if (r==0) continue; if (mn>=0) break; printf("  Cannot be negative.\n"); }
            for (;;) { r = read_float("  Most likely: ", &lt); CANCELLED(r); if (r==0) continue; if (lt>=mn) break; printf("  Must be >= %.1f.\n", mn); }
            for (;;) { r = read_float("  Pessimistic: ", &mx); CANCELLED(r); if (r==0) continue; if (mx>=lt) break; printf("  Must be >= %.1f.\n", lt); }
            task_set_pert(t, mn, lt, mx);
            break;
        }
        case 4: {
            int risk_int = 0;
            printf("  Current: %.0f/10\n", t->risk * 10.0f);
            for (;;) { r = read_int("  New risk (0-10): ", &risk_int); CANCELLED(r); if (r==0) continue; if (risk_int>=0 && risk_int<=10) break; printf("  Must be 0-10.\n"); }
            task_set_risk(t, risk_int / 10.0f);
            break;
        }
        case 5:
            t->required_skills = select_skills();
            break;
        case 0:
            return;
        default:
            printf("  Invalid field.\n");
            return;
    }

    printf("  Task [%d] updated.\n", t->id);
    autosave(p);
}

static void menu_deps(Project *p) {
    int choice;
    Task *t = NULL;

    crumb_push("Deps");

    list_tasks(p);
    { int _r = read_int("  Task ID (0 to back): ", &choice); if (_r == -1 || choice == 0) { crumb_pop(); return; } }
    t = project_find_task(p, choice);
    if (!t) { printf("  Task [%d] not found.\n", choice); crumb_pop(); return; }

    for (;;) {
        crumb_print();
        render_task_deps(p, t);
        printf("  1. Link two tasks    2. View another task's dependencies    0. Back\n");
        { int _r = read_nav("  > ", &choice); if (_r != 1) continue; }
        if (choice == 0) break;
        switch (choice) {
            case 1: link_tasks(p); break;
            case 2: {
                int new_id;
                list_tasks(p);
                if (read_int("  Task ID: ", &new_id) == -1) break;
                { Task *next = project_find_task(p, new_id);
                  if (!next) { printf("  Task [%d] not found.\n", new_id); break; }
                  t = next; }
                break;
            }
        }
    }

    crumb_pop();
}

static int tasks_handler(Project *p, int choice) {
    switch (choice) {
        case 0: return 1;
        case 1: list_tasks(p);    break;
        case 2: add_task(p);      break;
        case 3: remove_task(p);   break;
        case 4: edit_task(p);     break;
        case 5: change_status(p); break;
        case 6: link_tasks(p);    break;
        case 7: menu_deps(p);     break;
    }
    return 0;
}

void menu_tasks(Project *p) {
    crumb_push("Tasks");
    run_menu(p, NULL,
             "  1. List    2. Add    3. Remove    4. Edit    5. Status    6. Link    7. Deps    0. Back",
             tasks_handler);
    crumb_pop();
}

/* ---- member menu -------------------------------------------------------- */

static uint32_t select_skills(void) {
    uint32_t mask = 0;
    int choice, i;

    for (;;) {
        printf("\n  Skills checklist (enter number to toggle, 0 to confirm):\n");
        for (i = 0; i < SKILL_COUNT; i++)
            printf("  [%s] %d. %s\n",
                   (mask & (1u << i)) ? "x" : " ",
                   i + 1, SKILL_NAMES[i]);

        { int _r = read_nav("  > ", &choice); if (_r != 1) continue; }
        if (choice == 0) break;
        if (choice >= 1 && choice <= SKILL_COUNT)
            mask ^= (1u << (choice - 1));
    }

    return mask;
}

/* ---- milestone menu ----------------------------------------------------- */

static void add_milestone(Project *p) {
    char name[MAX_NAME_LEN];
    int deadline, priority;

    CANCELLED(read_str("  Name: ", name, MAX_NAME_LEN));
    CANCELLED(read_int("  Deadline (days from project start): ", &deadline));
    CANCELLED(read_int("  Priority (1=low, 10=critical): ",      &priority));

    Milestone *m = project_add_milestone(p, name, deadline, priority);
    if (!m) { printf("  Error.\n"); return; }
    printf("  Milestone [M%d] '%s' created at day %d.\n", m->id, m->name, m->deadline_day);
    autosave(p);
}

static int milestones_handler(Project *p, int choice) {
    int i;
    switch (choice) {
        case 0: return 1;
        case 1:
            for (i = 0; i < p->milestone_count; i++)
                milestone_print(p->milestones[i]);
            break;
        case 2: add_milestone(p); break;
    }
    return 0;
}

void menu_milestones(Project *p) {
    crumb_push("Milestones");
    run_menu(p, NULL, "  1. List    2. Add    0. Back", milestones_handler);
    crumb_pop();
}

/* ---- schedule menu ------------------------------------------------------ */

static Company *g_sched_company    = NULL;
static int      g_sched_project_idx = -1;

static int schedule_handler(Project *p, int choice) {
    switch (choice) {
        case 0: return 1;
        case 1:
            if (schedule_project(p, SCHED_EARLIEST_DEADLINE))
                printf("  Schedule computed.\n");
            break;
        case 2:
            if (schedule_project(p, SCHED_RISK_WEIGHTED_CRITICAL))
                printf("  Risk-weighted schedule computed.\n");
            break;
        case 3:
            assign_members_greedy(g_sched_company, g_sched_project_idx);
            printf("  Members assigned.\n");
            break;
        case 4: schedule_print_report(p); break;
        case 5: {
            int strat;
            printf("  1. Earliest Deadline    2. Risk-Weighted Critical Path\n");
            { int _r = read_nav("  Strategy: ", &strat); if (_r != 1 || (strat != 1 && strat != 2)) break; }
            schedule_project(p, strat == 1 ? SCHED_EARLIEST_DEADLINE : SCHED_RISK_WEIGHTED_CRITICAL);
            render_gantt(p, g_sched_company, GANTT_WIDTH);
            break;
        }
        case 6: render_dag(p);                break;
        case 7: {
            char dot_path[MAX_PATH_LEN], exe_dir[MAX_PATH_LEN];
            get_exe_dir(exe_dir, MAX_PATH_LEN);
            snprintf(dot_path, MAX_PATH_LEN, "%s%s.dot", exe_dir, p->name);
            export_dot(p, dot_path);
            break;
        }
    }
    return 0;
}

void menu_schedule(Company *c, int project_idx) {
    Project *p;
    if (project_idx < 0 || project_idx >= c->project_count) return;
    p = c->projects[project_idx];
    g_sched_company     = c;
    g_sched_project_idx = project_idx;
    crumb_push("Schedule");
    run_menu(p, NULL,
             "  1. Earliest Deadline    2. Risk-Weighted Critical Path\n"
             "  3. Assign members (greedy)    4. Report    5. Gantt    6. DAG    7. Export .dot    0. Back",
             schedule_handler);
    crumb_pop();
    g_sched_company     = NULL;
    g_sched_project_idx = -1;
}

/* ---- company team menu -------------------------------------------------- */

static void list_members(Company *c) {
    int i;
    if (c->member_count == 0) { printf("  No members.\n"); return; }
    printf("\n");
    for (i = 0; i < c->member_count; i++)
        team_member_print(c->members[i]);
}

static void add_company_member(Company *c) {
    char name[MAX_NAME_LEN], role[MAX_NAME_LEN];
    TeamMember *m;
    CANCELLED(read_str("  Name: ", name, MAX_NAME_LEN));
    CANCELLED(read_str("  Role: ", role, MAX_NAME_LEN));
    m = company_add_member(c, name, role);
    if (!m) { printf("  Error: could not create member.\n"); return; }
    m->skills = select_skills();
    printf("  Member [%d] %s added to company.\n", m->id, m->name);
    if (g_company) company_save(g_company);
}

static void menu_company_team(Company *c) {
    int choice;
    crumb_push("Team");
    for (;;) {
        crumb_print();
        list_members(c);
        printf("  1. Add member    2. Remove member    0. Back\n");
        { int _r = read_nav("  > ", &choice); if (_r != 1) continue; }
        if (choice == 0) break;
        if (choice == 1) add_company_member(c);
        if (choice == 2) {
            int id;
            if (read_int("  Member ID to remove: ", &id) == -1) continue;
            if (company_remove_member(c, id)) { printf("  Removed.\n"); if (g_company) company_save(g_company); }
            else printf("  Member [%d] not found.\n", id);
        }
    }
    crumb_pop();
}

/* ---- company projects menu ---------------------------------------------- */

static void list_projects(Company *c) {
    int i;
    if (c->project_count == 0) { printf("  No projects.\n"); return; }
    printf("\n");
    for (i = 0; i < c->project_count; i++)
        printf("  [%d] %s  (%d tasks, %d members)\n",
               i, c->projects[i]->name,
               c->projects[i]->task_count,
               c->projects[i]->member_ids.count);
}

static void open_project(Company *c) {
    int idx, choice;
    Project *p;
    char crumb_label[MAX_CRUMB_LEN];

    list_projects(c);
    if (read_int("  Project index: ", &idx) == -1) return;
    if (idx < 0 || idx >= c->project_count) { printf("  Invalid index.\n"); return; }

    p = c->projects[idx];
    g_company = c;
    snprintf(crumb_label, MAX_CRUMB_LEN, "%.28s", p->name);
    crumb_push(crumb_label);

    for (;;) {
        crumb_print();
        printf("  1. Tasks    2. Deps    3. Milestones    4. Schedule    5. Members    0. Back\n");
        { int _r = read_nav("  > ", &choice); if (_r != 1) continue; }
        if (choice == 0) break;
        switch (choice) {
            case 1: menu_tasks(p);           break;
            case 2: menu_deps(p);            break;
            case 3: menu_milestones(p);      break;
            case 4: menu_schedule(c, idx);   break;
            case 5: {
                /* assign company member to this project */
                int mid;
                list_members(c);
                if (read_int("  Add member ID to project: ", &mid) == -1) break;
                if (company_assign_member(c, idx, mid, -1))
                    printf("  Member [%d] added to project.\n", mid);
                else
                    printf("  Failed — check member ID and skills.\n");
                break;
            }
        }
    }

    crumb_pop();
    g_company = NULL;
}

static void create_project(Company *c) {
    char name[MAX_NAME_LEN];
    Date today = {2026, 5, 30};
    Project *p;
    char proj_dir[MAX_PATH_LEN];

    CANCELLED(read_str("  Project name: ", name, MAX_NAME_LEN));
    p = company_add_project(c, name, today);
    if (!p) { printf("  Error: could not create project.\n"); return; }

    snprintf(proj_dir, MAX_PATH_LEN, "%s\\projects\\%s", c->save_dir, name);
    strncpy(p->save_dir, proj_dir, sizeof(p->save_dir) - 1);

    company_save(c);
    printf("  Project '%s' created.\n", name);
}

static void menu_company_projects(Company *c) {
    int choice;
    crumb_push("Projects");
    for (;;) {
        crumb_print();
        list_projects(c);
        printf("  1. New project    2. Open project    0. Back\n");
        { int _r = read_nav("  > ", &choice); if (_r != 1) continue; }
        if (choice == 0) break;
        if (choice == 1) create_project(c);
        if (choice == 2) open_project(c);
    }
    crumb_pop();
}

/* ---- company top-level menu --------------------------------------------- */

void menu_company(Company *c) {
    int choice;
    crumb_push(c->name);
    for (;;) {
        crumb_print();
        company_print_summary(c);
        printf("  1. Projects    2. Team    3. Save    0. Exit\n");
        { int _r = read_nav("  > ", &choice); if (_r != 1) continue; }
        if (choice == 0) break;
        switch (choice) {
            case 1: menu_company_projects(c); break;
            case 2: menu_company_team(c);     break;
            case 3: company_save(c); printf("  Saved to %s\n", c->save_dir); break;
        }
    }
    crumb_pop();
}
