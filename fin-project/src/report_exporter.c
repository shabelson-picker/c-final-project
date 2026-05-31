#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "report_exporter.h"
#include "renderer.h"
#include "dot_export.h"
#include "reports.h"
#include "constants.h"
#include "team.h"

static const char *STATUS_LABELS[] = {
    "TODO", "IN_PROGRESS", "DONE", "CANCELLED", "BLOCKED"
};

/* Escape a string into an HTML stream. */
static void html_fputs(FILE *out, const char *s) {
    for (; *s; s++) {
        switch (*s) {
            case '&': fputs("&amp;",  out); break;
            case '<': fputs("&lt;",   out); break;
            case '>': fputs("&gt;",   out); break;
            case '"': fputs("&quot;", out); break;
            default:  fputc(*s, out);       break;
        }
    }
}

/* ---- document sections -------------------------------------------------- */

/* Shared modern stylesheet + document open. Both reports use this. */
static void write_modern_head(FILE *f, const char *title) {
    fprintf(f, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
               "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>");
    html_fputs(f, title);
    fprintf(f, "</title>\n");
    fprintf(f,
        "<style>\n"
        ":root{--bg:#0f172a;--panel:#ffffff;--ink:#1e293b;--muted:#64748b;"
        "--line:#e2e8f0;--accent:#6366f1;--accent2:#8b5cf6;--ok:#16a34a;--warn:#dc2626;--amber:#d97706}\n"
        "*{box-sizing:border-box}\n"
        "body{font-family:'Segoe UI',-apple-system,Roboto,Helvetica,Arial,sans-serif;"
        "margin:0;background:#f1f5f9;color:var(--ink);line-height:1.5}\n"
        ".wrap{max-width:1100px;margin:0 auto;padding:32px 20px 64px}\n"
        ".hero{background:linear-gradient(120deg,var(--accent),var(--accent2));color:#fff;"
        "border-radius:16px;padding:28px 32px;box-shadow:0 10px 30px rgba(99,102,241,.25)}\n"
        ".hero h1{margin:0;font-size:28px;letter-spacing:.3px}\n"
        ".hero .meta{margin:6px 0 0;color:rgba(255,255,255,.85);font-size:13px}\n"
        "h2{font-size:18px;margin:34px 0 12px;padding-bottom:6px;border-bottom:2px solid var(--line)}\n"
        ".card{background:var(--panel);border:1px solid var(--line);border-radius:14px;"
        "padding:6px 18px 18px;box-shadow:0 1px 3px rgba(15,23,42,.06);margin-bottom:18px}\n"
        ".kpis{display:flex;flex-wrap:wrap;gap:14px;margin:18px 0}\n"
        ".kpi{flex:1;min-width:150px;background:var(--panel);border:1px solid var(--line);"
        "border-radius:14px;padding:16px 18px;box-shadow:0 1px 3px rgba(15,23,42,.06)}\n"
        ".kpi .n{font-size:26px;font-weight:700;color:var(--accent)}\n"
        ".kpi .l{font-size:12px;color:var(--muted);text-transform:uppercase;letter-spacing:.6px}\n"
        "table{border-collapse:collapse;width:100%%;font-size:13px}\n"
        "th,td{padding:9px 12px;text-align:left;border-bottom:1px solid var(--line)}\n"
        "th{color:var(--muted);font-size:11px;text-transform:uppercase;letter-spacing:.5px}\n"
        "tbody tr:hover{background:#f8fafc} td.crit{color:var(--warn);font-weight:600}\n"
        ".pill{display:inline-block;padding:2px 10px;border-radius:999px;font-size:11px;font-weight:600}\n"
        ".pill.ok{background:#dcfce7;color:var(--ok)} .pill.over{background:#fee2e2;color:var(--warn)}\n"
        ".pill.idle{background:#f1f5f9;color:var(--muted)}\n"
        ".bar{height:8px;border-radius:999px;background:var(--line);overflow:hidden;min-width:90px}\n"
        ".bar>span{display:block;height:100%%;background:linear-gradient(90deg,var(--ok),#22c55e)}\n"
        "pre.gantt{font-family:Consolas,'Courier New',monospace;font-size:12px;line-height:1.4;"
        "background:#0f172a;color:#e2e8f0;border-radius:12px;padding:14px;overflow-x:auto}\n"
        "pre.gantt .opt{font-weight:bold;color:#fff} pre.gantt .exp{color:#94a3b8} pre.gantt .pess{color:var(--amber)}\n"
        "pre.gantt .crit{color:#f87171} pre.gantt .noncrit{color:#4ade80}\n"
        ".legend{color:var(--muted);font-size:12px} .warn{color:var(--warn);font-weight:600}\n"
        "svg{max-width:100%%;height:auto}\n"
        "</style>\n</head>\n<body>\n<div class=\"wrap\">\n");
}

static void write_head(FILE *f, const Project *p) {
    char title[160];
    snprintf(title, sizeof title, "%.120s - Project Report", p->name);
    write_modern_head(f, title);
}

static void write_header_section(FILE *f, const Project *p) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    int dur = 0, i;
    for (i = 0; i < p->task_count; i++)
        if (p->tasks[i]->sched_end > dur) dur = p->tasks[i]->sched_end;

    fprintf(f, "<div class=\"hero\"><h1>");
    html_fputs(f, p->name);
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
        html_fputs(f, t->title);
        fprintf(f, "</td><td>%s</td><td>", STATUS_LABELS[st]);
        if (a) html_fputs(f, a->name); else fprintf(f, "-");
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
        html_fputs(f, m->name);
        fprintf(f, "</td><td>day %d</td>", m->deadline_day);
        if (fc >= 0) fprintf(f, "<td>day %d</td>", fc); else fprintf(f, "<td>-</td>");
        fprintf(f, "<td>%d</td><td>%d</td><td><span class=\"pill %s\">%s</span></td></tr>\n",
                m->priority, m->task_count, pill, milestone_status_label(st));
    }
    fprintf(f, "</table></div>\n");
}

/* Inline an SVG file into the report (skipping the <?xml?>/DOCTYPE prolog).
 * Returns 1 if anything was written, 0 if the file was missing/empty. */
static int inline_svg(FILE *out, const char *svgpath) {
    FILE *s = fopen(svgpath, "rb");
    long sz;
    size_t rd;
    char *buf, *start;

    if (!s) return 0;
    fseek(s, 0, SEEK_END);
    sz = ftell(s);
    fseek(s, 0, SEEK_SET);
    if (sz <= 0) { fclose(s); return 0; }

    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(s); return 0; }
    rd = fread(buf, 1, (size_t)sz, s);
    buf[rd] = '\0';
    fclose(s);

    start = strstr(buf, "<svg");
    if (start) fwrite(start, 1, strlen(start), out);
    free(buf);
    return start != NULL;
}

/* Dependency graph: write .dot, render to SVG via Graphviz, inline it.
 * Falls back to a warning if Graphviz (`dot`) is not available. */
static void write_dag_section(FILE *f, const Project *p, const char *report_path) {
    char base[MAX_PATH_LEN], dotpath[MAX_PATH_LEN], svgpath[MAX_PATH_LEN], cmd[600];
    char *ext;

    fprintf(f, "<h2>Dependency Graph</h2>\n");

    /* sibling paths <base>.dot / <base>.svg derived from <base>.html */
    strncpy(base, report_path, sizeof(base) - 1);
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

    if (!inline_svg(f, svgpath))
        fprintf(f, "<p class=\"warn\">Install Graphviz for full report.</p>\n");
}

/* Open a file in the platform default browser. */
static void open_in_browser(const char *filename) {
    char cmd[512];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", filename);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" &", filename);
#endif
    system(cmd);
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
    write_modern_head(f, title);

    /* Hero */
    fprintf(f, "<div class=\"hero\"><h1>");
    html_fputs(f, c->name);
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
        html_fputs(f, p->name);
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
        html_fputs(f, m ? m->name : "?");
        fprintf(f, "</td><td>");
        html_fputs(f, m ? m->role : "-");
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
                html_fputs(f, p->name);
                fprintf(f, "</td><td>");
                html_fputs(f, m->name);
                fprintf(f, "</td><td>day %d</td>", m->deadline_day);
                if (fc >= 0) fprintf(f, "<td>day %d</td>", fc); else fprintf(f, "<td>-</td>");
                fprintf(f, "<td><span class=\"pill %s\">%s</span></td></tr>\n",
                        pill, milestone_status_label(st));
            }
        }
        fprintf(f, "</table></div>\n");
    }

    fprintf(f, "</div>\n</body>\n</html>\n");
    fclose(f);
    printf("  Saved: %s\n", filename);
    open_in_browser(filename);
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
    fprintf(f, "</div>\n</body>\n</html>\n");
    fclose(f);

    printf("  Saved: %s\n", filename);

#ifdef _WIN32
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", filename);
        system(cmd);
    }
#else
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" &", filename);
        system(cmd);
    }
#endif

    return 1;
}
