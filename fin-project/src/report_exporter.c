#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "util.h"
#include "html_builder.h"
#include "report_exporter.h"
#include "renderer.h"
#include "dot_export.h"
#include "reports.h"
#include "constants.h"
#include "team.h"

static const char *STATUS_LABELS[] = {
    "TODO", "IN_PROGRESS", "DONE", "CANCELLED", "BLOCKED"
};

/* ---- document sections -------------------------------------------------- */

static void write_head(FILE *f, const Project *p) {
    char title[160];
    snprintf(title, sizeof title, "%.120s - Project Report", p->name);
    html_doc_open(f, title);
}

static void write_header_section(FILE *f, const Project *p) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    int dur = 0, i;
    for (i = 0; i < p->task_count; i++)
        if (p->tasks[i]->sched_end > dur) dur = p->tasks[i]->sched_end;

    fprintf(f, "<div class=\"hero\"><h1>");
    html_escape(f, p->name);
    fprintf(f, "</h1>\n<p class=\"meta\">Start %04d-%02d-%02d &middot; Generated %04d-%02d-%02d "
               "&middot; Duration %d days &middot; %d tasks &middot; %d on roster</p></div>\n",
            p->start_date.year, p->start_date.month, p->start_date.day,
            lt ? lt->tm_year + 1900 : 0, lt ? lt->tm_mon + 1 : 0, lt ? lt->tm_mday : 0,
            dur, p->task_count, p->member_ids.count);
}

static void write_task_table(FILE *f, const Project *p, const Company *c) {
    int i;
    fprintf(f, "<h2>Tasks</h2>\n<table>\n"
               "<tr><th>ID</th><th>Title</th><th>Status</th><th>Assignee</th>"
               "<th>PERT o/l/p</th><th>Exp</th><th>Risk</th>"
               "<th>Start</th><th>End</th><th>Slack</th><th>Critical</th></tr>\n");
    for (i = 0; i < p->task_count; i++) {
        Task *t = p->tasks[i];
        TeamMember *a = (c && t->assignee_id != -1)
                        ? company_find_member((Company *)c, t->assignee_id) : NULL;
        int st = (t->status >= 0 && t->status <= 4) ? (int)t->status : 0;

        fprintf(f, "<tr><td>%d</td><td>", t->id);
        html_escape(f, t->title);
        fprintf(f, "</td><td>%s</td><td>", STATUS_LABELS[st]);
        if (a) html_escape(f, a->name); else fprintf(f, "-");
        fprintf(f, "</td><td>%.1f / %.1f / %.1f</td><td>%.1f</td><td>%.0f/10</td>"
                   "<td>%d</td><td>%d</td><td>%d</td>",
                t->pert_min, t->pert_likely, t->pert_max, t->pert_expected,
                t->risk * 10.0f, t->sched_start, t->sched_end, t->slack);
        if (t->is_critical) fprintf(f, "<td class=\"crit\">yes</td>");
        else                fprintf(f, "<td></td>");
        fprintf(f, "</tr>\n");
    }
    fprintf(f, "</table>\n");
}

static void write_milestones(FILE *f, const Project *p) {
    int i;
    if (p->milestone_count == 0) return;
    fprintf(f, "<h2>Milestones</h2>\n<div class=\"card\"><table>\n"
               "<tr><th>Name</th><th>Deadline</th><th>Forecast</th>"
               "<th>Priority</th><th>Tasks</th><th>Status</th></tr>\n");
    for (i = 0; i < p->milestone_count; i++) {
        Milestone      *m  = p->milestones[i];
        MilestoneStatus st = milestone_status(p, m);
        int             fc = milestone_forecast_day(p, m);
        const char     *pill = (st == MS_LATE) ? "over" : (st == MS_AT_RISK) ? "idle" : "ok";
        fprintf(f, "<tr><td>");
        html_escape(f, m->name);
        fprintf(f, "</td><td>day %d</td>", m->deadline_day);
        if (fc >= 0) fprintf(f, "<td>day %d</td>", fc); else fprintf(f, "<td>-</td>");
        fprintf(f, "<td>%d</td><td>%d</td><td><span class=\"pill %s\">%s</span></td></tr>\n",
                m->priority, m->task_count, pill, milestone_status_label(st));
    }
    fprintf(f, "</table></div>\n");
}

/* Dependency graph: write .dot, render to SVG via Graphviz, inline it.
 * Falls back to a warning if Graphviz (`dot`) is not available. */
static void write_dag_section(FILE *f, const Project *p, const char *report_path) {
    char base[MAX_PATH_LEN], dotpath[MAX_PATH_LEN], svgpath[MAX_PATH_LEN], cmd[600];
    char *ext;

    fprintf(f, "<h2>Dependency Graph</h2>\n");

    /* sibling paths <base>.dot / <base>.svg derived from <base>.html */
    str_copy(base, report_path, sizeof(base));
    base[sizeof(base) - 1] = '\0';
    ext = strrchr(base, '.');
    if (ext) *ext = '\0';
    snprintf(dotpath, sizeof(dotpath), "%s.dot", base);
    snprintf(svgpath, sizeof(svgpath), "%s.svg", base);

    if (!dot_write(p, dotpath)) {
        fprintf(f, "<p class=\"warn\">Could not generate graph source.</p>\n");
        return;
    }

    remove(svgpath);  /* clear any stale render so we can detect failure */
    snprintf(cmd, sizeof(cmd), "dot -Tsvg \"%s\" -o \"%s\"", dotpath, svgpath);
    system(cmd);

    if (!html_inline_svg(f, svgpath))
        fprintf(f, "<p class=\"warn\">Install Graphviz for full report.</p>\n");
}

/* ---- executive (company-wide) report ------------------------------------ */

int export_executive_report_html(const Company *c, const char *filename) {
    FILE *f = fopen(filename, "w");
    WorkloadStat wl[256];
    int   wln, i, j;
    int   total_tasks = 0, total_done = 0, over_members = 0;
    int   ms_total = 0, ms_risk = 0;
    char  title[160];
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);

    if (!f) { printf("  Could not write '%s'\n", filename); return 0; }

    for (i = 0; i < c->project_count; i++) {
        Project *p = c->projects[i];
        for (j = 0; j < p->task_count; j++) {
            total_tasks++;
            if (p->tasks[j]->status == STATUS_DONE) total_done++;
        }
    }
    wln = compute_member_workload(c, wl, 256);
    for (i = 0; i < wln; i++) if (wl[i].overallocated) over_members++;
    for (i = 0; i < c->project_count; i++) {
        Project *p = c->projects[i];
        for (j = 0; j < p->milestone_count; j++) {
            MilestoneStatus st = milestone_status(p, p->milestones[j]);
            ms_total++;
            if (st == MS_LATE || st == MS_AT_RISK) ms_risk++;
        }
    }

    snprintf(title, sizeof title, "%.120s - Executive Report", c->name);
    html_doc_open(f, title);

    /* Hero */
    fprintf(f, "<div class=\"hero\"><h1>");
    html_escape(f, c->name);
    fprintf(f, "</h1>\n<p class=\"meta\">Executive Report &middot; Generated %04d-%02d-%02d</p></div>\n",
            lt ? lt->tm_year + 1900 : 0, lt ? lt->tm_mon + 1 : 0, lt ? lt->tm_mday : 0);

    /* KPI cards */
    fprintf(f, "<div class=\"kpis\">\n");
    fprintf(f, "<div class=\"kpi\"><div class=\"n\">%d</div><div class=\"l\">Projects</div></div>\n", c->project_count);
    fprintf(f, "<div class=\"kpi\"><div class=\"n\">%d</div><div class=\"l\">Team members</div></div>\n", c->member_count);
    fprintf(f, "<div class=\"kpi\"><div class=\"n\">%d</div><div class=\"l\">Total tasks</div></div>\n", total_tasks);
    fprintf(f, "<div class=\"kpi\"><div class=\"n\">%d%%</div><div class=\"l\">Tasks done</div></div>\n",
            total_tasks ? (100 * total_done / total_tasks) : 0);
    fprintf(f, "<div class=\"kpi\"><div class=\"n\" style=\"color:%s\">%d</div><div class=\"l\">Overallocated</div></div>\n",
            over_members ? "var(--warn)" : "var(--ok)", over_members);
    fprintf(f, "<div class=\"kpi\"><div class=\"n\" style=\"color:%s\">%d/%d</div><div class=\"l\">Milestones at risk</div></div>\n",
            ms_risk ? "var(--warn)" : "var(--ok)", ms_risk, ms_total);
    fprintf(f, "</div>\n");

    /* Portfolio table */
    fprintf(f, "<h2>Portfolio</h2>\n<div class=\"card\"><table>\n"
               "<tr><th>Project</th><th>Start</th><th>Tasks</th><th>Progress</th><th>Duration</th></tr>\n");
    for (i = 0; i < c->project_count; i++) {
        Project *p = c->projects[i];
        int dur = 0, done = 0, pct;
        for (j = 0; j < p->task_count; j++) {
            if (p->tasks[j]->sched_end > dur) dur = p->tasks[j]->sched_end;
            if (p->tasks[j]->status == STATUS_DONE) done++;
        }
        pct = p->task_count ? (100 * done / p->task_count) : 0;
        fprintf(f, "<tr><td>");
        html_escape(f, p->name);
        fprintf(f, "</td><td>%04d-%02d-%02d</td><td>%d</td>"
                   "<td><div class=\"bar\"><span style=\"width:%d%%\"></span></div>%d%%</td>"
                   "<td>%d days</td></tr>\n",
                p->start_date.year, p->start_date.month, p->start_date.day,
                p->task_count, pct, pct, dur);
    }
    fprintf(f, "</table></div>\n");

    /* HR workload */
    fprintf(f, "<h2>HR &middot; Resource Workload</h2>\n<div class=\"card\"><table>\n"
               "<tr><th>Member</th><th>Role</th><th>Tasks</th><th>Committed</th>"
               "<th>Active window</th><th>Utilization</th><th>Status</th></tr>\n");
    for (i = 0; i < wln; i++) {
        TeamMember *m = company_find_member((Company *)c, wl[i].member_id);
        int win = wl[i].last_day - wl[i].first_day;
        int util = win > 0 ? (100 * wl[i].committed_days / win) : (wl[i].task_count ? 100 : 0);
        char window[40];
        if (wl[i].task_count > 0) {
            Date d = date_from_days(wl[i].first_day);
            snprintf(window, sizeof window, "%04d-%02d-%02d (+%dd)", d.year, d.month, d.day, win);
        } else {
            snprintf(window, sizeof window, "-");
        }
        if (util > 100) util = 100;
        fprintf(f, "<tr><td>");
        html_escape(f, m ? m->name : "?");
        fprintf(f, "</td><td>");
        html_escape(f, m ? m->role : "-");
        fprintf(f, "</td><td>%d</td><td>%d days</td><td>%s</td>"
                   "<td><div class=\"bar\"><span style=\"width:%d%%\"></span></div>%d%%</td><td>",
                wl[i].task_count, wl[i].committed_days, window, util, util);
        if (wl[i].overallocated)        fprintf(f, "<span class=\"pill over\">OVERALLOCATED</span>");
        else if (wl[i].task_count == 0) fprintf(f, "<span class=\"pill idle\">idle</span>");
        else                            fprintf(f, "<span class=\"pill ok\">ok</span>");
        fprintf(f, "</td></tr>\n");
    }
    fprintf(f, "</table></div>\n");

    /* Milestones across all projects */
    if (ms_total > 0) {
        fprintf(f, "<h2>Milestones</h2>\n<div class=\"card\"><table>\n"
                   "<tr><th>Project</th><th>Milestone</th><th>Deadline</th>"
                   "<th>Forecast</th><th>Status</th></tr>\n");
        for (i = 0; i < c->project_count; i++) {
            Project *p = c->projects[i];
            for (j = 0; j < p->milestone_count; j++) {
                Milestone      *m  = p->milestones[j];
                MilestoneStatus st = milestone_status(p, m);
                int             fc = milestone_forecast_day(p, m);
                const char     *pill = (st == MS_LATE) ? "over" : (st == MS_AT_RISK) ? "idle" : "ok";
                fprintf(f, "<tr><td>");
                html_escape(f, p->name);
                fprintf(f, "</td><td>");
                html_escape(f, m->name);
                fprintf(f, "</td><td>day %d</td>", m->deadline_day);
                if (fc >= 0) fprintf(f, "<td>day %d</td>", fc); else fprintf(f, "<td>-</td>");
                fprintf(f, "<td><span class=\"pill %s\">%s</span></td></tr>\n",
                        pill, milestone_status_label(st));
            }
        }
        fprintf(f, "</table></div>\n");
    }

    html_doc_close(f);
    fclose(f);
    printf("  Saved: %s\n", filename);
    html_open_in_browser(filename);
    return 1;
}

/* ---- public entry point ------------------------------------------------- */

int export_report_html(const Project *p, const Company *c, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) { printf("  Could not write '%s'\n", filename); return 0; }

    write_head(f, p);
    write_header_section(f, p);
    write_task_table(f, p, c);
    render_gantt_html(f, p, c, GANTT_WIDTH);
    write_dag_section(f, p, filename);
    write_milestones(f, p);
    html_doc_close(f);
    fclose(f);

    printf("  Saved: %s\n", filename);
    html_open_in_browser(filename);
    return 1;
}
