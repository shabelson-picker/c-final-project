#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "report_exporter.h"
#include "renderer.h"
#include "dot_export.h"
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

static void write_head(FILE *f, const Project *p) {
    fprintf(f, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n<title>");
    html_fputs(f, p->name);
    fprintf(f, " - Project Report</title>\n");
    fprintf(f,
        "<style>\n"
        "body{font-family:'Segoe UI',Arial,sans-serif;margin:24px;color:#222}\n"
        "h1{margin-bottom:2px} .meta{color:#666;margin-top:0}\n"
        "table{border-collapse:collapse;margin:8px 0 20px} \n"
        "th,td{border:1px solid #ccc;padding:4px 8px;font-size:13px}\n"
        "th{background:#f0f0f0;text-align:left} td.crit{color:#c62828;font-weight:bold}\n"
        "pre.gantt{font-family:Consolas,'Courier New',monospace;font-size:12px;line-height:1.35;"
        "background:#fafafa;border:1px solid #ddd;padding:10px;overflow-x:auto}\n"
        "pre.gantt .opt{font-weight:bold} pre.gantt .exp{color:#999} pre.gantt .pess{color:#e69500}\n"
        "pre.gantt .crit{color:#e53935} pre.gantt .noncrit{color:#2e7d32}\n"
        ".legend{color:#666;font-size:12px} .warn{color:#c62828;font-weight:bold}\n"
        "</style>\n</head>\n<body>\n");
}

static void write_header_section(FILE *f, const Project *p) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    int dur = 0, i;
    for (i = 0; i < p->task_count; i++)
        if (p->tasks[i]->sched_end > dur) dur = p->tasks[i]->sched_end;

    fprintf(f, "<h1>");
    html_fputs(f, p->name);
    fprintf(f, "</h1>\n");
    fprintf(f, "<p class=\"meta\">Start %04d-%02d-%02d &middot; Generated %04d-%02d-%02d "
               "&middot; Duration %d days &middot; %d tasks &middot; %d on roster</p>\n",
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
    fprintf(f, "<h2>Milestones</h2>\n<table>\n"
               "<tr><th>ID</th><th>Name</th><th>Deadline (day)</th>"
               "<th>Priority</th><th>Tasks</th></tr>\n");
    for (i = 0; i < p->milestone_count; i++) {
        Milestone *m = p->milestones[i];
        fprintf(f, "<tr><td>M%d</td><td>", m->id);
        html_fputs(f, m->name);
        fprintf(f, "</td><td>%d</td><td>%d</td><td>%d</td></tr>\n",
                m->deadline_day, m->priority, m->task_count);
    }
    fprintf(f, "</table>\n");
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
    fprintf(f, "</body>\n</html>\n");
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
