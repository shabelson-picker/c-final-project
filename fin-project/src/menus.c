#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <io.h>
#include <windows.h>
#include "util.h"
#include "file_browser.h"
#include "constants.h"
#include "tui_framework.h"
#include "menus.h"
#include "company.h"
#include "algorithms.h"
#include "renderer.h"
#include "dot_export.h"
#include "report_exporter.h"
#include "persistence.h"
#include "roles.h"
#include "reports.h"
#include "sim.h"
#include "ui.h"

/* GANTT_WIDTH defined in constants.h */

/* screen_clear() / screen_pause() / C_* colours / cprintf() live in ui.h */

/* Input helpers, breadcrumb, and the menu runners live in tui_framework.{c,h}. */

/* Forward declarations - defined in member menu section */
static uint32_t select_skills(void);
static uint32_t select_skills_from(uint32_t initial);

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

/* ---- small shared helpers ----------------------------------------------- */

/* save_dir if set, else "." - keeps export-path building uniform. */
static const char *dir_or_dot(const char *save_dir) {
    return save_dir[0] ? save_dir : ".";
}

/* One-line roster entry: "    [id] name (role)". */
static void print_member_line(const TeamMember *m) {
    printf("    [%d] %s (%s)\n", m->id, m->name, m->role);
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

    task_set_status(t, (TaskStatus)choice);
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
            str_copy(t->title, title, MAX_TITLE_LEN);
            break;
        }
        case 2: {
            char desc[MAX_DESC_LEN];
            CANCELLED(read_str("  New description: ", desc, MAX_DESC_LEN));
            str_copy(t->description, desc, MAX_DESC_LEN);
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
            task_set_skills(t, select_skills_from(t->required_skills));
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

static void act_deps_link(void *ctx) { link_tasks(((DepsCtx *)ctx)->p); }
static void act_deps_view(void *ctx) {
    DepsCtx *d = (DepsCtx *)ctx;
    int new_id;
    Task *next;
    list_tasks(d->p);
    if (read_int("  Task ID: ", &new_id) == -1) return;
    next = project_find_task(d->p, new_id);
    if (!next) { cprintf(C_RED, "  Task [%d] not found.\n", new_id); return; }
    d->t = next;
}

static const MenuItem DEPS_MENU[] = {
    { "Link two tasks",                   0, act_deps_link },
    { "View another task's dependencies", 0, act_deps_view },
    { NULL, 0, NULL }
};

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
    run_table_menu(&d, deps_render, DEPS_MENU, "Back", NULL);
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
        task_clear_assignment(t);
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

    task_pin_assignee(t, member_id);
    cprintf(C_GREEN, "  Task [%d] pinned to %s; the scheduler will not reassign it.\n",
            task_id, m->name);
    autosave(p);
}

static void split_task_ui(Project *p) {
    int id, pct;
    Task *b, *t;
    list_tasks(p);
    if (read_int("  Task id to split: ", &id) == -1) return;
    t = project_find_task(p, id);
    if (!t || id < 1)   { cprintf(C_RED, "  No such task.\n"); return; }
    if (t->fixed_time)  { cprintf(C_YELLOW, "  Cannot split a fixed-time (vacation) block.\n"); return; }
    for (;;) {
        int r = read_int("  Part 1 share, percent (10-90): ", &pct);
        if (r == -1) return;
        if (r == 0)  continue;
        if (pct < 10 || pct > 90) { printf("  Must be 10-90.\n"); continue; }
        break;
    }
    b = project_split_task(p, id, pct / 100.0f);
    if (b) { cprintf(C_GREEN, "  Split done: part 1 = [%d], part 2 = [%d]. Reschedule to update the plan.\n", id, b->id); autosave(p); }
    else     cprintf(C_RED, "  Split failed.\n");
}

static void act_task_list(void *ctx)   { list_tasks((Project *)ctx); }
static void act_task_add(void *ctx)    { add_task((Project *)ctx); }
static void act_task_remove(void *ctx) { remove_task((Project *)ctx); }
static void act_task_edit(void *ctx)   { edit_task((Project *)ctx); }
static void act_task_status(void *ctx) { change_status((Project *)ctx); }
static void act_task_link(void *ctx)   { link_tasks((Project *)ctx); }
static void act_task_deps(void *ctx)   { menu_deps((Project *)ctx); }
static void act_task_assign(void *ctx) { manual_assign((Project *)ctx); }
static void act_task_split(void *ctx)  { split_task_ui((Project *)ctx); }

static const MenuItem TASKS_MENU[] = {
    { "List",   0,              act_task_list },
    { "Add",    PRIV_EDIT_TASK, act_task_add },
    { "Remove", PRIV_EDIT_TASK, act_task_remove },
    { "Edit",   PRIV_EDIT_TASK, act_task_edit },
    { "Status", PRIV_EDIT_TASK, act_task_status },
    { "Link",   PRIV_EDIT_DEPS, act_task_link },
    { "Deps",   0,              act_task_deps },
    { "Assign", PRIV_ASSIGN,    act_task_assign },
    { "Split",  PRIV_EDIT_TASK, act_task_split },
    { NULL, 0, NULL }
};

void menu_tasks(Project *p) {
    crumb_push("Tasks");
    run_table_menu(p, NULL, TASKS_MENU, "Back", NULL);
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

/* Interactive skills checklist seeded with `initial` (so an edit starts from the
 * current selection rather than blank). Returns the toggled mask. */
static uint32_t select_skills_from(uint32_t initial) {
    uint32_t mask = initial;
    run_checklist(&mask, skills_render, skills_toggle);
    return mask;
}

static uint32_t select_skills(void) {
    return select_skills_from(0);
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

/* Attach/detach tasks to a milestone - this is what makes the forecast/status
 * tracking live (a milestone with no tasks reports "(none)"). */
typedef struct { Project *p; Milestone *m; } MsAttachCtx;

static void ms_attach_render(void *ctx) {
    MsAttachCtx *a = (MsAttachCtx *)ctx;
    int i;
    cprintf(C_BOLD, "\n  Tasks on milestone [M%d] '%s' (deadline day %d):\n",
            a->m->id, a->m->name, a->m->deadline_day);
    for (i = 0; i < a->p->task_count; i++) {
        Task *t = a->p->tasks[i];
        int on = milestone_has_task(a->m, t->id);
        cprintf(ui_zebra(i), "  %2d. [%c] [%d] %-30.30s\n",
                i + 1, on ? 'x' : ' ', t->id, t->title);
    }
    cprintf(C_DIM, "  Enter a number to toggle, 0 to finish.\n");
}

static void ms_attach_toggle(void *ctx, int n) {
    MsAttachCtx *a = (MsAttachCtx *)ctx;
    Task *t;
    if (n < 1 || n > a->p->task_count) return;
    t = a->p->tasks[n - 1];
    if (milestone_has_task(a->m, t->id)) project_unlink_task_milestone(a->p, t->id, a->m->id);
    else                                 project_link_task_milestone(a->p, t->id, a->m->id);
}

static void act_ms_list(void *ctx) {
    Project *p = (Project *)ctx;
    int i;
    if (p->milestone_count == 0) { cprintf(C_DIM, "  No milestones.\n"); return; }
    for (i = 0; i < p->milestone_count; i++) {
        Milestone      *m  = p->milestones[i];
        MilestoneStatus st = milestone_status(p, m);
        int             fc = milestone_forecast_day(p, m);
        const char     *col = (st == MS_LATE) ? C_RED
                            : (st == MS_AT_RISK) ? C_YELLOW
                            : (st == MS_ON_TRACK) ? C_GREEN : C_DIM;
        cprintf(ui_zebra(i), "  [M%d] %-26.26s  deadline day %-4d  forecast %s",
                m->id, m->name, m->deadline_day,
                fc >= 0 ? "" : "(none)");
        if (fc >= 0) printf("day %-4d", fc);
        printf("  ");
        cprintf(col, "%s\n", milestone_status_label(st));
    }
}
static void act_ms_add(void *ctx) { add_milestone((Project *)ctx); }
static void act_ms_attach(void *ctx) {
    Project *p = (Project *)ctx;
    int id, i;
    Milestone *m;
    MsAttachCtx a;
    if (p->milestone_count == 0) { cprintf(C_DIM, "  No milestones - add one first.\n"); return; }
    for (i = 0; i < p->milestone_count; i++)
        printf("    [M%d] %s\n", p->milestones[i]->id, p->milestones[i]->name);
    if (read_int("  Milestone id: ", &id) == -1) return;
    m = project_find_milestone(p, id);
    if (!m) { cprintf(C_RED, "  No such milestone.\n"); return; }
    a.p = p; a.m = m;
    run_checklist(&a, ms_attach_render, ms_attach_toggle);
    autosave(p);
}

static const MenuItem MILESTONES_MENU[] = {
    { "List",         0, act_ms_list },
    { "Add",          0, act_ms_add },
    { "Attach tasks", 0, act_ms_attach },
    { NULL, 0, NULL }
};

void menu_milestones(Project *p) {
    crumb_push("Milestones");
    run_table_menu(p, NULL, MILESTONES_MENU, "Back", NULL);
    crumb_pop();
}

/* ---- schedule menu ------------------------------------------------------ */

/* The strategy chosen by the last "Generate" run, reused by reschedules that
 * don't prompt for one (e.g. Request vacation). */
static ScheduleStrategy g_last_strategy = SCHED_EARLIEST_DEADLINE;

/* render hook: keep the Gantt visible above the schedule menu.
 * Runs inside an open-project context, so g_company is the active company. */
static void sched_render(void *ctx) { render_gantt((Project *)ctx, g_company, GANTT_WIDTH); }

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

static void act_sched_generate(void *ctx) {
    Project *p = (Project *)ctx;
    int strat;
    ScheduleStrategy s;
    printf("  1. Earliest Deadline    2. Risk-Weighted Critical Path    3. Pessimistic (worst-case)\n");
    { int _r = read_nav("  Strategy: ", &strat); if (_r != 1 || strat < 1 || strat > 3) return; }
    s = (strat == 1) ? SCHED_EARLIEST_DEADLINE
      : (strat == 2) ? SCHED_RISK_WEIGHTED_CRITICAL
      : SCHED_PESSIMISTIC;
    g_last_strategy = s;   /* reused by reschedules that don't prompt */
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
    assign_members_greedy(g_company, g_company_project_idx, s);
    schedule_project(p, s);
    render_gantt(p, g_company, GANTT_WIDTH);
    resolve_unassigned(g_company, g_company_project_idx, s);
    company_save(g_company);  /* persist assignments + any roster adds */
}
static void act_sched_report(void *ctx) { schedule_print_report((Project *)ctx); }
static void act_sched_gantt(void *ctx)  { render_gantt((Project *)ctx, g_company, GANTT_WIDTH); }
static void act_sched_dag(void *ctx)    { render_dag((Project *)ctx); }
static void act_sched_dot(void *ctx) {
    Project *p = (Project *)ctx;
    char dot_path[MAX_PATH_LEN];
    snprintf(dot_path, MAX_PATH_LEN, "%s\\%s.dot", dir_or_dot(p->save_dir), p->name);
    export_dot(p, dot_path);
}
static void act_sched_html(void *ctx) {
    Project *p = (Project *)ctx;
    char rep_path[MAX_PATH_LEN];
    snprintf(rep_path, MAX_PATH_LEN, "%s\\%s_report.html", dir_or_dot(p->save_dir), p->name);
    export_report_html(p, g_company, rep_path);
}
static void act_sched_vacation(void *ctx) {   /* pin an immovable block; reschedule around it */
    Project *p = (Project *)ctx;
    int mid, start, len, k;
    TeamMember *m;
    char label[MAX_NAME_LEN + 16];
    if (p->member_ids.count == 0) { cprintf(C_YELLOW, "  No members on the project roster.\n"); return; }
    printf("  Roster:\n");
    for (k = 0; k < p->member_ids.count; k++) {
        TeamMember *mm = company_find_member(g_company, p->member_ids.data[k]);
        if (mm) print_member_line(mm);
    }
    if (read_nav("  Member id: ", &mid) != 1) return;
    m = company_find_member(g_company, mid);
    if (!m || dia_find_index(&p->member_ids, mid) == -1) {
        cprintf(C_YELLOW, "  That member is not on this project.\n"); return;
    }
    if (read_nav("  Start day (days from this project's start): ", &start) != 1) return;
    if (read_nav("  Length in days: ", &len) != 1 || len < 1) return;
    snprintf(label, sizeof label, "[Vacation] %s", m->name);
    if (!project_add_fixed_block(p, label, mid, start, len)) {
        cprintf(C_YELLOW, "  Could not create the block.\n"); return;
    }
    assign_members_greedy(g_company, g_company_project_idx, g_last_strategy);
    schedule_project(p, g_last_strategy);
    render_gantt(p, g_company, GANTT_WIDTH);
    company_save(g_company);
    cprintf(C_GREEN, "  Vacation for %s pinned at day %d for %d day(s); others rerouted.\n",
            m->name, start, len);
}

static const MenuItem SCHEDULE_MENU[] = {
    { "Generate scheduale", PRIV_SCHEDULE, act_sched_generate },
    { "Report",             PRIV_REPORT,   act_sched_report },
    { "Gantt",              PRIV_REPORT,   act_sched_gantt },
    { "DAG",                PRIV_REPORT,   act_sched_dag },
    { "Export .dot",        PRIV_REPORT,   act_sched_dot },
    { "Export report",      PRIV_REPORT,   act_sched_html },
    { "Request vacation",   PRIV_ASSIGN,   act_sched_vacation },
    { NULL, 0, NULL }
};

void menu_schedule(Company *c, int project_idx) {
    Project *p;
    if (project_idx < 0 || project_idx >= c->project_count) return;
    p = c->projects[project_idx];
    /* Reaffirm the open-project context (open_project set it; this keeps
     * menu_schedule self-contained). The caller owns clearing it, so we don't. */
    g_company             = c;
    g_company_project_idx = project_idx;
    crumb_push("Schedule");
    /* sched_render keeps the Gantt (from persisted data) above the menu every
     * iteration, so it's visible on entry without a regenerate. */
    run_table_menu(p, sched_render, SCHEDULE_MENU, "Back", NULL);
    crumb_pop();
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
    team_member_set_skills(m, select_skills());
    cprintf(C_GREEN, "  Member [%d] %s added to company.\n", m->id, m->name);
    company_save(c);
}

static void team_render(void *ctx) { list_members((Company *)ctx); }

static void act_team_add(void *ctx) { add_company_member((Company *)ctx); }
static void act_team_remove(void *ctx) {
    Company *c = (Company *)ctx;
    int id;
    if (read_int("  Member ID to remove: ", &id) == -1) return;
    if (company_remove_member(c, id)) {
        cprintf(C_GREEN, "  Removed.\n");
        /* easter egg: in death, every member of Project Mayhem has a name */
        cprintf(C_BOLD C_YELLOW,
                "  In death, a member of Project Mayhem has a name. His name is Robert Paulson.\n");
        company_save(c);
    } else {
        cprintf(C_RED, "  Member [%d] not found.\n", id);
    }
}
static void act_team_skills(void *ctx) {
    Company *c = (Company *)ctx;
    int id;
    TeamMember *m;
    if (read_int("  Member ID to edit skills: ", &id) == -1) return;
    m = company_find_member(c, id);
    if (!m) { cprintf(C_RED, "  Member [%d] not found.\n", id); return; }
    cprintf(C_DIM, "  Editing skills for %s:\n", m->name);
    team_member_set_skills(m, select_skills_from(m->skills));
    cprintf(C_GREEN, "  Skills updated for %s.\n", m->name);
    company_save(c);
}

static const MenuItem TEAM_MENU[] = {
    { "Add member",    0, act_team_add },
    { "Remove member", 0, act_team_remove },
    { "Edit skills",   0, act_team_skills },
    { NULL, 0, NULL }
};

static void menu_company_team(Company *c) {
    crumb_push("Team");
    run_table_menu(c, team_render, TEAM_MENU, "Back", NULL);
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

static void act_proj_tasks(void *ctx)      { menu_tasks(((ProjCtx *)ctx)->p); }
static void act_proj_deps(void *ctx)       { menu_deps(((ProjCtx *)ctx)->p); }
static void act_proj_milestones(void *ctx) { menu_milestones(((ProjCtx *)ctx)->p); }
static void act_proj_schedule(void *ctx)   { ProjCtx *pc = (ProjCtx *)ctx; menu_schedule(pc->c, pc->idx); }
static void act_proj_members(void *ctx) {
    ProjCtx *pc = (ProjCtx *)ctx;
    run_checklist(pc, roster_render, roster_toggle);
    company_save(pc->c);
}
static void act_proj_simulate(void *ctx)   { ProjCtx *pc = (ProjCtx *)ctx; menu_simulate(pc->c, pc->idx); }

static const MenuItem PROJECT_MENU[] = {
    { "Tasks",      0,             act_proj_tasks },
    { "Deps",       0,             act_proj_deps },
    { "Milestones", 0,             act_proj_milestones },
    { "Schedule",   0,             act_proj_schedule },
    { "Members",    PRIV_ASSIGN,   act_proj_members },
    { "Simulate",   PRIV_SCHEDULE, act_proj_simulate },
    { NULL, 0, NULL }
};

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
    run_table_menu(&pc, NULL, PROJECT_MENU, "Back", NULL);

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
    str_copy(p->save_dir, proj_dir, sizeof(p->save_dir));

    company_save(c);
    cprintf(C_GREEN, "  Project '%s' created.\n", name);
}

static void projects_render(void *ctx) { list_projects((Company *)ctx); }

static void act_projects_new(void *ctx)  { create_project((Company *)ctx); }
static void act_projects_open(void *ctx) { open_project((Company *)ctx); }

static const MenuItem PROJECTS_MENU[] = {
    { "New project",  PRIV_ADMIN,        act_projects_new },
    { "Open project", PRIV_VIEW_PROJECT, act_projects_open },
    { NULL, 0, NULL }
};

static void menu_company_projects(Company *c) {
    crumb_push("Projects");
    run_table_menu(c, projects_render, PROJECTS_MENU, "Back", NULL);
    crumb_pop();
}

/* ---- company top-level menu --------------------------------------------- */

/* VIEW_OWN realization: every assignment for one member, across all projects. */
static void show_my_tasks(Company *c) {
    int mid, i, j, found = 0;
    TeamMember *m;
    if (c->member_count == 0) { cprintf(C_DIM, "  No members.\n"); return; }
    printf("  Members:\n");
    for (i = 0; i < c->member_count; i++)
        print_member_line(c->members[i]);
    if (read_int("  Your member id: ", &mid) == -1) return;
    m = company_find_member(c, mid);
    if (!m) { cprintf(C_RED, "  No such member.\n"); return; }

    cprintf(C_BOLD, "\n  Tasks assigned to %s, across all projects:\n", m->name);
    for (i = 0; i < c->project_count; i++) {
        Project *p = c->projects[i];
        for (j = 0; j < p->task_count; j++) {
            Task *t = p->tasks[j];
            if (t->assignee_id != mid) continue;
            cprintf(t->is_critical ? C_RED : C_RESET,
                    "    %-18.18s  %-24.24s  day %d..%d%s\n",
                    p->name, t->title, t->sched_start, t->sched_end,
                    t->is_critical ? "  [CRITICAL]" : "");
            found = 1;
        }
    }
    if (!found) cprintf(C_DIM, "    (no assignments)\n");
}

static void company_render(void *ctx) { company_print_summary((Company *)ctx); }

/* The rules. */
static void project_mayhem_rules(void) {
    cprintf(C_BOLD C_RED, "\n  Welcome to Project Mayhem.\n");
    cprintf(C_RED, "  1st rule: You do not talk about Project Mayhem.\n");
    cprintf(C_RED, "  2nd rule: You DO NOT talk about Project Mayhem.\n");
    cprintf(C_DIM, "  ... 8th rule: If this is your first night, you have to assign a task.\n");
}

/* ---- role sign-in ------------------------------------------------------- */

/* Print the privileges a mask grants, comma-separated (dim), or "all". */
static void print_privs(uint32_t mask) {
    static const Privilege BITS[] = {
        PRIV_VIEW_OWN, PRIV_VIEW_PROJECT, PRIV_VIEW_PORTFOLIO, PRIV_EDIT_TASK,
        PRIV_EDIT_DEPS, PRIV_ASSIGN, PRIV_SCHEDULE, PRIV_REPORT, PRIV_ADMIN
    };
    int i, first = 1;
    if (mask == PRIV_ALL) { cprintf(C_DIM, "all privileges"); return; }
    for (i = 0; i < (int)(sizeof BITS / sizeof BITS[0]); i++) {
        if (mask & BITS[i]) {
            cprintf(C_DIM, "%s%s", first ? "" : ", ", priv_name(BITS[i]));
            first = 0;
        }
    }
    if (first) cprintf(C_DIM, "none");
}

int choose_role(void) {
    Role roles[MAX_ROLES];
    int  n, i, sel = 0;

    n = roles_load("roles.cfg", roles, MAX_ROLES);
    if (n <= 0) {
        char dir[MAX_PATH_LEN], path[MAX_PATH_LEN];
        get_exe_dir(dir, MAX_PATH_LEN);
        if (dir[0]) {
            snprintf(path, sizeof path, "%s\\roles.cfg", dir);
            n = roles_load(path, roles, MAX_ROLES);
        }
    }
    if (n < 0) { cprintf(C_DIM, "  (roles.cfg not found - System Admin only)\n"); n = 0; }

    cprintf(C_BOLD C_CYAN, "\n  Choose your role:\n");
    printf("    [0] %-16s - ", "System Admin"); print_privs(PRIV_ALL);          printf("\n");
    for (i = 0; i < n; i++) {
        printf("    [%d] %-16.16s - ", i + 1, roles[i].name); print_privs(roles[i].privs); printf("\n");
    }
    cprintf(C_DIM, "   [-1] Exit program\n");

    for (;;) {
        int r = read_int("  Select role: ", &sel);
        if (r == -1) {
            if (feof(stdin)) return 1;                    /* EOF -> exit program */
            sel = 0; break;                               /* empty Enter -> default admin */
        }
        if (r == 1 && sel == -1) return 1;                /* exit program */
        if (r == 1 && sel >= 0 && sel <= n) break;        /* valid selection */
        cprintf(C_YELLOW, "  Enter a number between -1 and %d.\n", n);
    }

    if (sel == 0) roles_set_current("System Admin", PRIV_ALL);
    else          roles_set_current(roles[sel - 1].name, roles[sel - 1].privs);

    cprintf(C_GREEN, "  Signed in as %s.\n", roles_current_name());
    return 0;
}

/* ---- company top-level menu --------------------------------------------- */

/* Each company-menu action takes the opaque context (the Company *). */
static void act_projects(void *ctx)  { menu_company_projects((Company *)ctx); }
static void act_team(void *ctx)      { menu_company_team((Company *)ctx); }
static void act_save(void *ctx) {
    Company *c = (Company *)ctx;
    company_save(c);
    cprintf(C_GREEN, "  Saved to %s\n", c->save_dir);
}
static void act_portfolio(void *ctx) { render_portfolio_gantt((Company *)ctx, GANTT_WIDTH); }
static void act_my_tasks(void *ctx)  { show_my_tasks((Company *)ctx); }
static void act_all_gantt(void *ctx) { render_company_gantt((Company *)ctx, GANTT_WIDTH); }
static void act_workload(void *ctx)  { render_workload_report((Company *)ctx); }
static void act_exec_report(void *ctx) {
    Company *c = (Company *)ctx;
    char path[MAX_PATH_LEN];
    snprintf(path, MAX_PATH_LEN, "%s\\executive_report.html", dir_or_dot(c->save_dir));
    export_executive_report_html(c, path);
}

/* Label + required privilege + action, all in one place - no parallel switch. */
static const MenuItem COMPANY_MENU[] = {
    { "Projects",       PRIV_VIEW_PROJECT,   act_projects },
    { "Team",           PRIV_ADMIN,          act_team },
    { "Save",           0,                   act_save },
    { "Portfolio",      PRIV_VIEW_PORTFOLIO, act_portfolio },
    { "My tasks",       PRIV_VIEW_OWN,       act_my_tasks },
    { "All-task Gantt", PRIV_VIEW_PORTFOLIO, act_all_gantt },
    { "Workload",       PRIV_VIEW_PORTFOLIO, act_workload },
    { "Exec report",    PRIV_REPORT,         act_exec_report },
    { NULL, 0, NULL }
};

/* Handle choices not in the table (the hidden easter-egg code). */
static void company_fallback(void *ctx, int choice) {
    (void)ctx;
    if (choice == 1999) project_mayhem_rules();   /* undocumented */
    else                cprintf(C_RED, "  Invalid option.\n");
}

void menu_company(Company *c) {
    crumb_push(c->name);
    run_table_menu(c, company_render, COMPANY_MENU, "Sign out", company_fallback);
    crumb_pop();
}
