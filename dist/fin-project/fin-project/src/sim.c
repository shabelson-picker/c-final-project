#include <stdio.h>
#include <string.h>
#include <io.h>
#include <direct.h>
#include "util.h"
#include "sim.h"
#include "algorithms.h"
#include "persistence.h"
#include "tui_framework.h"
#include "menus.h"
#include "ui.h"
#include "constants.h"

/* ---- pure core ---------------------------------------------------------- */

int sim_advance_day(Project *p, int day) {
    int i, done = 0;
    for (i = 0; i < p->task_count; i++) {
        Task *t = p->tasks[i];
        if (t->status == STATUS_DONE || t->status == STATUS_CANCELLED) continue;
        if (t->fixed_time) continue;                 /* vacations are not "work" */
        if (t->assignee_id != -1 && t->sched_end <= day) {
            task_set_status(t, STATUS_DONE);
            done++;
        } else if (t->assignee_id != -1 && t->sched_start <= day && day < t->sched_end) {
            task_set_status(t, STATUS_IN_PROGRESS);
        }
    }
    return done;
}

/* ---- sandbox clone ------------------------------------------------------ */

/* Recursively delete a directory bundle (files + subdirectories). Used to clean
 * up the sandbox temp bundle when the simulation ends. Best-effort - any error
 * (e.g. a file held open) is ignored. */
static void remove_dir_recursive(const char *dir) {
    struct _finddata_t fd;
    intptr_t h;
    char search[MAX_PATH_LEN], child[MAX_PATH_LEN];

    snprintf(search, sizeof search, "%s\\*", dir);
    h = _findfirst(search, &fd);
    if (h != -1) {
        do {
            if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
            snprintf(child, sizeof child, "%s\\%s", dir, fd.name);
            if (fd.attrib & _A_SUBDIR) remove_dir_recursive(child);
            else                       remove(child);
        } while (_findnext(h, &fd) == 0);
        _findclose(h);
    }
    _rmdir(dir);
}

/* Clone the company by round-tripping through a temp bundle. The returned copy
 * is fully independent; its save_dir points at the temp dir. Returns NULL on
 * failure. On success, *out_tmp receives the temp path. */
static Company *clone_company(Company *c, char *out_tmp, int tmp_sz) {
    char orig[MAX_PATH_LEN];
    Company *sim;
    str_copy(orig, c->save_dir, sizeof orig);
    snprintf(out_tmp, tmp_sz, "%s_sandbox", orig);

    str_copy(c->save_dir, out_tmp, MAX_PATH_LEN);   /* temporarily retarget */
    company_save(c);
    str_copy(c->save_dir, orig, MAX_PATH_LEN);       /* restore real path */

    sim = company_load(out_tmp);
    return sim;
}

/* ---- rendering ---------------------------------------------------------- */

static const char *st_glyph(TaskStatus s) {
    switch (s) {
        case STATUS_DONE:        return "[x]";
        case STATUS_IN_PROGRESS: return "[~]";
        case STATUS_CANCELLED:   return "[/]";
        case STATUS_BLOCKED:     return "[!]";
        default:                 return "[ ]";
    }
}

static void render_board(Company *sim, Project *p, int day) {
    int i, done = 0, total = 0;
    cprintf(C_BOLD C_CYAN, "\n  SIMULATION - %s   ", p->name);
    cprintf(C_BOLD, "Day %d\n", day);
    cprintf(C_DIM, "  [x]done [~]active [ ]todo [/]cancelled\n");
    for (i = 0; i < p->task_count; i++) {
        Task *t = p->tasks[i];
        TeamMember *m = (t->assignee_id != -1) ? company_find_member(sim, t->assignee_id) : NULL;
        const char *col = (t->status == STATUS_DONE) ? C_GREEN
                        : (t->status == STATUS_CANCELLED) ? C_DIM
                        : (t->sched_start <= day && day < t->sched_end) ? C_YELLOW
                        : C_RESET;
        if (t->fixed_time) continue;
        total++;
        if (t->status == STATUS_DONE) done++;
        cprintf(col, "  %s [%2d] %-26.26s %-14.14s %3d..%-3d\n",
                st_glyph(t->status), t->id, t->title,
                m ? m->name : "(unassigned)", t->sched_start, t->sched_end);
    }
    cprintf(C_DIM, "  progress: %d/%d tasks done\n", done, total);
}

/* ---- event helpers ------------------------------------------------------ */

static void replan(Company *sim, int idx) {
    assign_members_greedy(sim, idx, SCHED_EARLIEST_DEADLINE);
    schedule_project(sim->projects[idx], SCHED_EARLIEST_DEADLINE);
}

static void set_status_by_id(Project *p, int id, TaskStatus s) {
    Task *t = project_find_task(p, id);
    if (!t) { cprintf(C_RED, "  No task [%d].\n", id); return; }
    task_set_status(t, s);
    cprintf(C_GREEN, "  Task [%d] '%s' -> %s\n", id, t->title,
            s == STATUS_DONE ? "DONE" : s == STATUS_CANCELLED ? "CANCELLED" : "updated");
}

static void event_add_task(Company *sim, int idx) {
    Project *p = sim->projects[idx];
    char title[MAX_TITLE_LEN];
    int days;
    Task *t;
    if (read_str("  New task title: ", title, MAX_TITLE_LEN) == -1) return;
    if (read_int("  Likely duration (days): ", &days) != 1 || days < 1) return;
    t = project_add_task(p, title, "added mid-simulation");
    if (!t) { cprintf(C_RED, "  Could not add.\n"); return; }
    task_set_pert(t, (float)days, (float)days, (float)days);
    replan(sim, idx);
    cprintf(C_GREEN, "  Added [%d] '%s' (%d days) and re-planned.\n", t->id, title, days);
}

static void event_quit(Company *sim, int idx) {
    Project *p = sim->projects[idx];
    int mid, i, freed = 0;
    TeamMember *m;
    if (read_int("  Member id who quits: ", &mid) != 1) return;
    m = company_find_member(sim, mid);
    if (!m) { cprintf(C_RED, "  No such member.\n"); return; }
    for (i = 0; i < p->task_count; i++)
        if (p->tasks[i]->assignee_id == mid) {
            task_clear_assignment(p->tasks[i]);
            freed++;
        }
    dia_sort_remove(&p->member_ids, mid);     /* off this project's roster */
    company_remove_member(sim, mid);          /* out of the company */
    cprintf(C_YELLOW, "  %s quit. %d task(s) unassigned; re-planning...\n", m->name, freed);
    replan(sim, idx);
}

static void event_hire(Company *sim, int idx) {
    char name[MAX_NAME_LEN];
    int skills;
    TeamMember *m;
    if (read_str("  New hire name: ", name, MAX_NAME_LEN) == -1) return;
    cprintf(C_DIM, "  skills sum: FE1 BE2 HW4 EMB8 QA16 DEVOPS32 DESIGN64 PM128\n");
    if (read_int("  Skills bitmask: ", &skills) != 1 || skills < 0) return;
    m = company_add_member(sim, name, "New Hire");
    if (!m) { cprintf(C_RED, "  Could not hire.\n"); return; }
    team_member_set_skills(m, (uint32_t)skills);
    company_assign_member(sim, idx, m->id, -1);   /* onto the roster */
    cprintf(C_GREEN, "  Hired %s [id %d]; re-planning...\n", name, m->id);
    replan(sim, idx);
}

/* ---- main loop ---------------------------------------------------------- */

void menu_simulate(Company *c, int project_idx) {
    char tmp[MAX_PATH_LEN], orig[MAX_PATH_LEN];
    Company *sim;
    Project *p;
    int day = 0, choice, running = 1;

    if (project_idx < 0 || project_idx >= c->project_count) return;
    str_copy(orig, c->save_dir, sizeof orig);

    sim = clone_company(c, tmp, sizeof tmp);
    if (!sim) {
        cprintf(C_RED, "  Could not start sandbox.\n");
        remove_dir_recursive(tmp);
        screen_pause();
        return;
    }
    if (project_idx >= sim->project_count) {
        company_destroy(sim);
        remove_dir_recursive(tmp);
        return;
    }
    p = sim->projects[project_idx];
    replan(sim, project_idx);

    crumb_push("Simulate (sandbox)");
    while (running) {
        crumb_refresh();
        render_board(sim, p, day);
        cprintf(C_BOLD C_CYAN,
            "\n  1.Next day  2.Complete  3.Cancel  4.Add task  5.Quit  6.Hire  7.Re-plan  8.Apply to real  0.Exit\n");
        if (read_nav("  > ", &choice) != 1) { screen_pause(); continue; }
        switch (choice) {
            case 0: running = 0; break;
            case 1: {
                int n = sim_advance_day(p, ++day);
                cprintf(C_GREEN, "  -> Day %d: %d task(s) completed.\n", day, n);
                screen_pause();
                break;
            }
            case 2: { int id; if (read_int("  Task id done: ", &id) == 1) set_status_by_id(p, id, STATUS_DONE); screen_pause(); break; }
            case 3: { int id; if (read_int("  Task id cancelled: ", &id) == 1) set_status_by_id(p, id, STATUS_CANCELLED); screen_pause(); break; }
            case 4: event_add_task(sim, project_idx); screen_pause(); break;
            case 5: event_quit(sim, project_idx);     screen_pause(); break;
            case 6: event_hire(sim, project_idx);     screen_pause(); break;
            case 7: replan(sim, project_idx); cprintf(C_GREEN, "  Re-planned.\n"); screen_pause(); break;
            case 8:
                str_copy(sim->save_dir, orig, MAX_PATH_LEN);
                if (company_save(sim)) cprintf(C_GREEN, "  Sandbox applied to the real project.\n");
                else                   cprintf(C_RED,   "  Apply failed.\n");
                screen_pause();
                running = 0;
                break;
        }
    }
    crumb_pop();
    company_destroy(sim);
    remove_dir_recursive(tmp);   /* clean up the sandbox temp bundle */
    cprintf(C_DIM, "  Left the sandbox.\n");
}
