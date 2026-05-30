#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <io.h>
#include <windows.h>
#include "file_browser.h"
#include "constants.h"
#include "menus.h"
#include "company.h"
#include "algorithms.h"
#include "renderer.h"
#include "dot_export.h"
#include "report_exporter.h"
#include "persistence.h"
#include "ui.h"

/* GANTT_WIDTH defined in constants.h */

/* screen_clear() / screen_pause() / C_* colours / cprintf() live in ui.h */

/* ---- input helpers ------------------------------------------------------ */

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

/* Forward declaration - defined in member menu section */
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

/* Draw the title banner + breadcrumb trail. refresh != 0 clears the screen
 * first (fresh menu); refresh == 0 draws in place (keeps prior output). */
static void crumb_draw(int refresh) {
    int i;
    if (refresh) screen_clear();
    for (i = 0; i < 44; i++) putchar('=');
    printf("\n  " C_BOLD C_CYAN "PROJECT MAYHEM MANAGEMENT" C_RESET "\n  ");
    for (i = 0; i < g_crumb_depth; i++) {
        if (i > 0) printf(" > ");
        if (i == g_crumb_depth - 1)
            printf(C_BOLD C_CYAN "%s" C_RESET, g_crumbs[i]);   /* current */
        else
            printf(C_DIM "%s" C_RESET, g_crumbs[i]);           /* ancestors */
    }
    printf("\n");
    for (i = 0; i < 44; i++) putchar('=');
    printf("\n");
}

void crumb_print(void)   { crumb_draw(0); }  /* header only, no clear */
void crumb_refresh(void) { crumb_draw(1); }  /* clear + header        */

/* ---- auto-save ---------------------------------------------------------- */

static Company *g_company = NULL;          /* set when entering a project context */
static int      g_company_project_idx = -1; /* index of the open project in g_company */

static void autosave(Project *p) {
    if (p->save_dir[0] == '\0') {
        cprintf(C_YELLOW, "  [warning] auto-save skipped - no directory set.\n");
        return;
    }
    if (g_company) company_save(g_company);
    else if (!project_save(p, p->save_dir))
        cprintf(C_YELLOW, "  [warning] auto-save failed for '%s'\n", p->save_dir);
}

/* ---- generic menu / checklist runners ----------------------------------- */

/* Generic menu loop over an opaque context. Each iteration: refresh, render,
 * show options, read a choice. handler returns non-zero to exit the menu.
 * A pause follows every action (and invalid input) so output is readable. */
void run_menu(void *ctx,
              void (*render)(void *ctx),
              const char *options,
              int  (*handler)(void *ctx, int choice)) {
    int choice;
    for (;;) {
        crumb_refresh();
        if (render) render(ctx);
        printf(C_BOLD C_CYAN "%s" C_RESET "\n", options);
        { int _r = read_nav("  > ", &choice); if (_r != 1) { screen_pause(); continue; } }
        if (handler(ctx, choice)) break;
        screen_pause();
    }
}

/* Generic checklist loop: refresh + render, read a number, 0 confirms, anything
 * else is passed to toggle(). No pause - the redrawn checkmarks are the feedback. */
static void run_checklist(void *ctx,
                          void (*render)(void *ctx),
                          void (*toggle)(void *ctx, int n)) {
    int n;
    for (;;) {
        crumb_refresh();
        render(ctx);
        { int _r = read_nav("  > ", &n); if (_r != 1) continue; }
        if (n == 0) break;
        toggle(ctx, n);
    }
}

/* ---- task menu ---------------------------------------------------------- */

#define CANCELLED(r) do { if ((r) == -1) { cprintf(C_DIM, "  Cancelled.\n"); return; } } while(0)

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
    if (!t) { cprintf(C_RED, "  Error: could not create task.\n"); return; }

    task_set_pert(t, mn, lt, mx);
    task_set_risk(t, risk);
    cprintf(C_GREEN, "  Task [%d] created. Expected duration: %.1f days.\n", t->id, t->pert_expected);
    autosave(p);
}

static void list_tasks(Project *p) {
    int i;
    if (p->task_count == 0) { cprintf(C_DIM, "  No tasks.\n"); return; }
    printf("\n");
    for (i = 0; i < p->task_count; i++) {
        fputs(ui_zebra(i), stdout);
        task_print(p->tasks[i]);
        fputs(C_RESET, stdout);
    }
}

static void link_tasks(Project *p) {
    int pre_id, post_id;
    Task *post;
    CANCELLED(read_int("  Pre-task ID: ",  &pre_id));
    CANCELLED(read_int("  Post-task ID: ", &post_id));
    if (!project_link_tasks(p, pre_id, post_id)) {
        cprintf(C_RED, "  Error: task not found.\n");
        return;
    }
    cprintf(C_GREEN, "  Linked %d --> %d\n", pre_id, post_id);
    autosave(p);
    post = project_find_task(p, post_id);
    if (post) render_task_deps(p, post);
}

static void remove_task(Project *p) {
    int id;
    list_tasks(p);
    CANCELLED(read_int("  Task ID to remove: ", &id));
    if (project_remove_task(p, id)) {
        cprintf(C_GREEN, "  Task [%d] removed.\n", id);
        autosave(p);
    } else {
        cprintf(C_RED, "  Task [%d] not found.\n", id);
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
    if (!t) { cprintf(C_RED, "  Task [%d] not found.\n", id); return; }

    printf("\n  Current status: %s\n", STATUS_LABELS[t->status]);
    for (i = 0; i < 5; i++)
        printf("  [%d] %s\n", i, STATUS_LABELS[i]);

    CANCELLED(read_int("  New status: ", &choice));
    if (choice < 0 || choice > 4) { cprintf(C_RED, "  Invalid status.\n"); return; }

    t->status = (TaskStatus)choice;
    cprintf(C_GREEN, "  Task [%d] status -> %s\n", id, STATUS_LABELS[t->status]);
    autosave(p);
}

static void edit_task(Project *p) {
    int id, field, r;
    Task *t;

    list_tasks(p);
    CANCELLED(read_int("  Task ID to edit: ", &id));
    t = project_find_task(p, id);
    if (!t) { cprintf(C_RED, "  Task [%d] not found.\n", id); return; }

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
            cprintf(C_RED, "  Invalid field.\n");
            return;
    }

    cprintf(C_GREEN, "  Task [%d] updated.\n", t->id);
    autosave(p);
}

typedef struct { Project *p; Task *t; } DepsCtx;

static void deps_render(void *ctx) {
    DepsCtx *d = (DepsCtx *)ctx;
    render_task_deps(d->p, d->t);
}

static int deps_handler(void *ctx, int choice) {
    DepsCtx *d = (DepsCtx *)ctx;
    switch (choice) {
        case 0: return 1;
        case 1: link_tasks(d->p); break;
        case 2: {
            int new_id;
            list_tasks(d->p);
            if (read_int("  Task ID: ", &new_id) == -1) break;
            { Task *next = project_find_task(d->p, new_id);
              if (!next) { cprintf(C_RED, "  Task [%d] not found.\n", new_id); break; }
              d->t = next; }
            break;
        }
    }
    return 0;
}

static void menu_deps(Project *p) {
    int choice;
    Task *t;
    DepsCtx d;

    crumb_push("Deps");
    list_tasks(p);
    { int _r = read_int("  Task ID (0 to back): ", &choice); if (_r == -1 || choice == 0) { crumb_pop(); return; } }
    t = project_find_task(p, choice);
    if (!t) { cprintf(C_RED, "  Task [%d] not found.\n", choice); crumb_pop(); return; }

    d.p = p; d.t = t;
    run_menu(&d, deps_render,
             "  1. Link two tasks    2. View another task's dependencies    0. Back",
             deps_handler);
    crumb_pop();
}

/* Print one member's block: the member line + each assigned task on its own
 * line (derived by scanning every project for assignee_id == member->id), the
 * whole block in one zebra colour (row = member index), then a separator. */
static void print_member_block(Company *c, TeamMember *m, int row) {
    int pi, ti, i;

    fputs(ui_zebra(row), stdout);
    team_member_print(m);
    for (pi = 0; pi < c->project_count; pi++) {
        Project *pr = c->projects[pi];
        for (ti = 0; ti < pr->task_count; ti++) {
            Task *t = pr->tasks[ti];
            if (t->assignee_id == m->id)
                printf("       - [%d] %s  (%s)\n", t->id, t->title, pr->name);
        }
    }
    fputs(C_RESET, stdout);

    fputs(C_DIM, stdout);
    printf("  ");
    for (i = 0; i < 60; i++) putchar('-');
    printf("\n");
    fputs(C_RESET, stdout);
}

/* Manually pin a task to a member. Pinned tasks (manually_assigned) are not
 * reassigned by the scheduler. Member ID -1 clears the pin. */
static void manual_assign(Project *p) {
    int task_id, member_id, mi;
    Task *t;
    TeamMember *m;

    if (!g_company) { cprintf(C_RED, "  No company context.\n"); return; }

    list_tasks(p);
    CANCELLED(read_int("  Task ID: ", &task_id));
    t = project_find_task(p, task_id);
    if (!t) { cprintf(C_RED, "  Task [%d] not found.\n", task_id); return; }

    crumb_refresh();
    cprintf(C_BOLD, "  Assign this task:\n  ");
    task_print(t);

    printf("\n  Company members:\n");
    for (mi = 0; mi < g_company->member_count; mi++)
        print_member_block(g_company, g_company->members[mi], mi);
    cprintf(C_DIM, "  Enter a member ID to pin, or -1 to clear the assignment.\n");

    CANCELLED(read_int("  Member ID: ", &member_id));

    if (member_id == -1) {
        t->assignee_id        = -1;
        t->manually_assigned  = 0;
        cprintf(C_YELLOW, "  Task [%d] assignment cleared.\n", task_id);
        autosave(p);
        return;
    }

    m = company_find_member(g_company, member_id);
    if (!m) { cprintf(C_RED, "  Member [%d] not found.\n", member_id); return; }

    if (!team_member_has_skills(m, t->required_skills))
        cprintf(C_YELLOW, "  Note: %s is missing some required skills (manual override).\n", m->name);

    /* ensure the member is on the project roster so the scheduler sees them */
    if (g_company_project_idx >= 0)
        company_assign_member(g_company, g_company_project_idx, member_id, -1);

    t->assignee_id       = member_id;
    t->manually_assigned = 1;
    cprintf(C_GREEN, "  Task [%d] pinned to %s; the scheduler will not reassign it.\n",
            task_id, m->name);
    autosave(p);
}

static int tasks_handler(void *ctx, int choice) {
    Project *p = (Project *)ctx;
    switch (choice) {
        case 0: return 1;
        case 1: list_tasks(p);    break;
        case 2: add_task(p);      break;
        case 3: remove_task(p);   break;
        case 4: edit_task(p);     break;
        case 5: change_status(p); break;
        case 6: link_tasks(p);    break;
        case 7: menu_deps(p);     break;
        case 8: manual_assign(p); break;
    }
    return 0;
}

void menu_tasks(Project *p) {
    crumb_push("Tasks");
    run_menu(p, NULL,
             "  1. List    2. Add    3. Remove    4. Edit    5. Status    6. Link    7. Deps    8. Assign    0. Back",
             tasks_handler);
    crumb_pop();
}

/* ---- member menu -------------------------------------------------------- */

static void skills_render(void *ctx) {
    uint32_t mask = *(uint32_t *)ctx;
    int i;
    printf("\n  Skills checklist (enter number to toggle, 0 to confirm):\n");
    for (i = 0; i < SKILL_COUNT; i++)
        printf("  [%s] %d. %s\n", (mask & (1u << i)) ? "x" : " ", i + 1, SKILL_NAMES[i]);
}

static void skills_toggle(void *ctx, int n) {
    uint32_t *mask = (uint32_t *)ctx;
    if (n >= 1 && n <= SKILL_COUNT) *mask ^= (1u << (n - 1));
}

static uint32_t select_skills(void) {
    uint32_t mask = 0;
    run_checklist(&mask, skills_render, skills_toggle);
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
    if (!m) { cprintf(C_RED, "  Error.\n"); return; }
    cprintf(C_GREEN, "  Milestone [M%d] '%s' created at day %d.\n", m->id, m->name, m->deadline_day);
    autosave(p);
}

static int milestones_handler(void *ctx, int choice) {
    Project *p = (Project *)ctx;
    int i;
    switch (choice) {
        case 0: return 1;
        case 1:
            for (i = 0; i < p->milestone_count; i++) {
                fputs(ui_zebra(i), stdout);
                milestone_print(p->milestones[i]);
                fputs(C_RESET, stdout);
            }
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

/* run_menu render hook: keep the Gantt visible above the schedule menu. */
static void sched_render(void *ctx) { render_gantt((Project *)ctx, g_sched_company, GANTT_WIDTH); }

static int prompt_yes_no(const char *msg) {
    char buf[16];
    printf("%s [y/n]: ", msg);
    if (!fgets(buf, sizeof(buf), stdin)) return 0;
    return (buf[0] == 'y' || buf[0] == 'Y');
}

/* True if any member already on the project roster has all of t's skills. */
static int roster_has_skill(Company *c, Project *p, const Task *t) {
    int k;
    for (k = 0; k < p->member_ids.count; k++) {
        TeamMember *m = company_find_member(c, p->member_ids.data[k]);
        if (m && team_member_has_skills(m, t->required_skills)) return 1;
    }
    return 0;
}

/* Collect-then-resolve: for each task left unassigned after a greedy run, offer
 * to pull a qualified company member onto the project. Re-runs assignment once
 * if any were added. All the I/O lives here; the scheduler stays pure. */
static void resolve_unassigned(Company *c, int project_idx, ScheduleStrategy s) {
    Project *p = c->projects[project_idx];
    int i, added = 0;

    for (i = 0; i < p->task_count; i++) {
        Task *t = p->tasks[i];
        int mid;
        if (t->assignee_id != -1) continue;
        if (roster_has_skill(c, p, t)) continue;  /* coverable now - re-run will assign */

        mid = suggest_company_member(c, p, t);
        if (mid == -1) {
            cprintf(C_YELLOW, "  Task [%d] '%s': no company member has the required skills.\n", t->id, t->title);
            continue;
        }
        {
            TeamMember *m = company_find_member(c, mid);
            char msg[200];
            snprintf(msg, sizeof(msg),
                     "  Task [%d] '%s' has no one qualified on the project. Add %s (%s)?",
                     t->id, t->title, m->name, m->role);
            if (prompt_yes_no(msg)) {
                company_assign_member(c, project_idx, mid, -1);
                added = 1;
            }
        }
    }

    if (added) {
        assign_members_greedy(c, project_idx, s);
        schedule_project(p, s);
        render_gantt(p, c, GANTT_WIDTH);
    }
}

static int schedule_handler(void *ctx, int choice) {
    Project *p = (Project *)ctx;
    switch (choice) {
        case 0: return 1;
        case 1: {
            int strat;
            ScheduleStrategy s;
            printf("  1. Earliest Deadline    2. Risk-Weighted Critical Path    3. Pessimistic (worst-case)\n");
            { int _r = read_nav("  Strategy: ", &strat); if (_r != 1 || strat < 1 || strat > 3) break; }
            s = (strat == 1) ? SCHED_EARLIEST_DEADLINE
              : (strat == 2) ? SCHED_RISK_WEIGHTED_CRITICAL
              : SCHED_PESSIMISTIC;
            {
                int pol;
                printf("  1. Earliest-free (tight schedule)   2. Balanced workload   3. Best skill-fit\n");
                if (read_nav("  Assignment policy: ", &pol) == 1 && pol >= 1 && pol <= 3)
                    assign_set_policy(pol == 2 ? ASSIGN_BALANCED
                                    : pol == 3 ? ASSIGN_BEST_FIT
                                    : ASSIGN_EARLIEST_FREE);
                else
                    assign_set_policy(ASSIGN_EARLIEST_FREE);
            }
            assign_members_greedy(g_sched_company, g_sched_project_idx, s);
            schedule_project(p, s);
            render_gantt(p, g_sched_company, GANTT_WIDTH);
            resolve_unassigned(g_sched_company, g_sched_project_idx, s);
            company_save(g_sched_company);  /* persist assignments + any roster adds */
            break;
        }
        case 2: schedule_print_report(p); break;
        case 3: render_gantt(p, g_sched_company, GANTT_WIDTH); break;
        case 4: render_dag(p);            break;
        case 5: {
            char dot_path[MAX_PATH_LEN];
            const char *dir = p->save_dir[0] ? p->save_dir : ".";
            snprintf(dot_path, MAX_PATH_LEN, "%s\\%s.dot", dir, p->name);
            export_dot(p, dot_path);
            break;
        }
        case 6: {
            char rep_path[MAX_PATH_LEN];
            const char *dir = p->save_dir[0] ? p->save_dir : ".";
            snprintf(rep_path, MAX_PATH_LEN, "%s\\%s_report.html", dir, p->name);
            export_report_html(p, g_sched_company, rep_path);
            break;
        }
        case 7: {   /* Request vacation: pin an immovable block; reschedule around it */
            int mid, start, len, k;
            TeamMember *m;
            char label[MAX_NAME_LEN + 16];
            if (p->member_ids.count == 0) { cprintf(C_YELLOW, "  No members on the project roster.\n"); break; }
            printf("  Roster:\n");
            for (k = 0; k < p->member_ids.count; k++) {
                TeamMember *mm = company_find_member(g_sched_company, p->member_ids.data[k]);
                if (mm) printf("    [%d] %s (%s)\n", mm->id, mm->name, mm->role);
            }
            if (read_nav("  Member id: ", &mid) != 1) break;
            m = company_find_member(g_sched_company, mid);
            if (!m || dia_find_index(&p->member_ids, mid) == -1) {
                cprintf(C_YELLOW, "  That member is not on this project.\n"); break;
            }
            if (read_nav("  Start day (project-relative): ", &start) != 1) break;
            if (read_nav("  Length in days: ", &len) != 1 || len < 1) break;
            snprintf(label, sizeof label, "[Vacation] %s", m->name);
            if (!project_add_fixed_block(p, label, mid, start, len)) {
                cprintf(C_YELLOW, "  Could not create the block.\n"); break;
            }
            assign_members_greedy(g_sched_company, g_sched_project_idx, SCHED_EARLIEST_DEADLINE);
            schedule_project(p, SCHED_EARLIEST_DEADLINE);
            render_gantt(p, g_sched_company, GANTT_WIDTH);
            company_save(g_sched_company);
            cprintf(C_GREEN, "  Vacation for %s pinned at day %d for %d day(s); others rerouted.\n",
                    m->name, start, len);
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
    /* sched_render keeps the Gantt (from persisted data) above the menu every
     * iteration, so it's visible on entry without a regenerate. */
    run_menu(p, sched_render,
             "  1. Generate scheduale    2. Report    3. Gantt    4. DAG    5. Export .dot    6. Export report    7. Request vacation    0. Back",
             schedule_handler);
    crumb_pop();
    g_sched_company     = NULL;
    g_sched_project_idx = -1;
}

/* ---- company team menu -------------------------------------------------- */

static void list_members(Company *c) {
    int i;
    if (c->member_count == 0) { cprintf(C_DIM, "  No members.\n"); return; }
    printf("\n");
    for (i = 0; i < c->member_count; i++)
        print_member_block(c, c->members[i], i);
}


static void add_company_member(Company *c) {
    char name[MAX_NAME_LEN], role[MAX_NAME_LEN];
    TeamMember *m;
    CANCELLED(read_str("  Name: ", name, MAX_NAME_LEN));
    CANCELLED(read_str("  Role: ", role, MAX_NAME_LEN));
    m = company_add_member(c, name, role);
    if (!m) { cprintf(C_RED, "  Error: could not create member.\n"); return; }
    m->skills = select_skills();
    cprintf(C_GREEN, "  Member [%d] %s added to company.\n", m->id, m->name);
    company_save(c);
}

static void team_render(void *ctx) { list_members((Company *)ctx); }

static int team_handler(void *ctx, int choice) {
    Company *c = (Company *)ctx;
    switch (choice) {
        case 0: return 1;
        case 1: add_company_member(c); break;
        case 2: {
            int id;
            if (read_int("  Member ID to remove: ", &id) == -1) break;
            if (company_remove_member(c, id)) {
                cprintf(C_GREEN, "  Removed.\n");
                /* easter egg: in death, every member of Project Mayhem has a name */
                cprintf(C_BOLD C_YELLOW,
                        "  In death, a member of Project Mayhem has a name. His name is Robert Paulson.\n");
                company_save(c);
            } else {
                cprintf(C_RED, "  Member [%d] not found.\n", id);
            }
            break;
        }
    }
    return 0;
}

static void menu_company_team(Company *c) {
    crumb_push("Team");
    run_menu(c, team_render, "  1. Add member    2. Remove member    0. Back", team_handler);
    crumb_pop();
}

/* ---- company projects menu ---------------------------------------------- */

static void list_projects(Company *c) {
    int i;
    if (c->project_count == 0) { cprintf(C_DIM, "  No projects.\n"); return; }
    printf("\n");
    for (i = 0; i < c->project_count; i++)
        cprintf(ui_zebra(i), "  [%d] %s  (%d tasks, %d members)\n",
                i, c->projects[i]->name,
                c->projects[i]->task_count,
                c->projects[i]->member_ids.count);
}

/* Context for the project submenu and its roster checklist. */
typedef struct { Company *c; int idx; Project *p; } ProjCtx;

static void roster_render(void *ctx) {
    ProjCtx *pc = (ProjCtx *)ctx;
    int mi, _k;
    printf("\n  Project roster (enter ID to toggle, 0 to confirm):\n");
    for (mi = 0; mi < pc->c->member_count; mi++) {
        TeamMember *m = pc->c->members[mi];
        int on_roster = 0;
        for (_k = 0; _k < pc->p->member_ids.count; _k++)
            if (pc->p->member_ids.data[_k] == m->id) { on_roster = 1; break; }
        printf("  [%s] [%2d] %-20s  %s\n", on_roster ? "x" : " ", m->id, m->name, m->role);
    }
}

static void roster_toggle(void *ctx, int mid) {
    ProjCtx *pc = (ProjCtx *)ctx;
    int _k, found = 0;
    for (_k = 0; _k < pc->p->member_ids.count; _k++)
        if (pc->p->member_ids.data[_k] == mid) { found = 1; break; }
    if (found) {
        TeamMember *rm = company_find_member(pc->c, mid);
        dia_sort_remove(&pc->p->member_ids, mid);          /* master */
        if (rm) dia_sort_remove(&rm->project_ids, pc->idx); /* apprentice */
        cprintf(C_YELLOW, "  Member [%d] removed from project.\n", mid);
    } else {
        if (company_assign_member(pc->c, pc->idx, mid, -1))
            cprintf(C_GREEN, "  Member [%d] added to project.\n", mid);
        else
            cprintf(C_RED, "  Member [%d] not found.\n", mid);
    }
}

static int project_handler(void *ctx, int choice) {
    ProjCtx *pc = (ProjCtx *)ctx;
    switch (choice) {
        case 0: return 1;
        case 1: menu_tasks(pc->p);            break;
        case 2: menu_deps(pc->p);             break;
        case 3: menu_milestones(pc->p);       break;
        case 4: menu_schedule(pc->c, pc->idx); break;
        case 5: run_checklist(pc, roster_render, roster_toggle); company_save(pc->c); break;
    }
    return 0;
}

static void open_project(Company *c) {
    int idx;
    Project *p;
    char crumb_label[MAX_CRUMB_LEN];
    ProjCtx pc;

    list_projects(c);
    if (read_int("  Project index: ", &idx) == -1) return;
    if (idx < 0 || idx >= c->project_count) { cprintf(C_RED, "  Invalid index.\n"); return; }

    p = c->projects[idx];
    g_company = c;
    g_company_project_idx = idx;
    snprintf(crumb_label, MAX_CRUMB_LEN, "%.28s", p->name);
    crumb_push(crumb_label);

    pc.c = c; pc.idx = idx; pc.p = p;
    run_menu(&pc, NULL,
             "  1. Tasks    2. Deps    3. Milestones    4. Schedule    5. Members    0. Back",
             project_handler);

    crumb_pop();
    g_company = NULL;
    g_company_project_idx = -1;
}

static void create_project(Company *c) {
    char name[MAX_NAME_LEN];
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    Date today = { lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday };
    Project *p;
    char proj_dir[MAX_PATH_LEN];

    CANCELLED(read_str("  Project name: ", name, MAX_NAME_LEN));
    p = company_add_project(c, name, today);
    if (!p) { cprintf(C_RED, "  Error: could not create project.\n"); return; }

    snprintf(proj_dir, MAX_PATH_LEN, "%s\\projects\\%s", c->save_dir, name);
    strncpy(p->save_dir, proj_dir, sizeof(p->save_dir) - 1);

    company_save(c);
    cprintf(C_GREEN, "  Project '%s' created.\n", name);
}

static void projects_render(void *ctx) { list_projects((Company *)ctx); }

static int projects_handler(void *ctx, int choice) {
    Company *c = (Company *)ctx;
    switch (choice) {
        case 0: return 1;
        case 1: create_project(c); break;
        case 2: open_project(c);   break;
    }
    return 0;
}

static void menu_company_projects(Company *c) {
    crumb_push("Projects");
    run_menu(c, projects_render, "  1. New project    2. Open project    0. Back", projects_handler);
    crumb_pop();
}

/* ---- company top-level menu --------------------------------------------- */

static void company_render(void *ctx) { company_print_summary((Company *)ctx); }

/* The rules. */
static void project_mayhem_rules(void) {
    cprintf(C_BOLD C_RED, "\n  Welcome to Project Mayhem.\n");
    cprintf(C_RED, "  1st rule: You do not talk about Project Mayhem.\n");
    cprintf(C_RED, "  2nd rule: You DO NOT talk about Project Mayhem.\n");
    cprintf(C_DIM, "  ... 8th rule: If this is your first night, you have to assign a task.\n");
}

static int company_handler(void *ctx, int choice) {
    Company *c = (Company *)ctx;
    switch (choice) {
        case 0: return 1;
        case 1: menu_company_projects(c); break;
        case 2: menu_company_team(c);     break;
        case 3: company_save(c); cprintf(C_GREEN, "  Saved to %s\n", c->save_dir); break;
        case 1999: project_mayhem_rules(); break;  /* easter egg (undocumented) */
    }
    return 0;
}

void menu_company(Company *c) {
    crumb_push(c->name);
    run_menu(c, company_render, "  1. Projects    2. Team    3. Save    0. Exit", company_handler);
    crumb_pop();
}
