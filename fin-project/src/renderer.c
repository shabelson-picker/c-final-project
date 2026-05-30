#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "renderer.h"
#include "constants.h"
#include "team.h"

/* ANSI color codes */
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_DIM     "\033[2m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_CYAN    "\033[36m"

/* ---- Gantt chart -------------------------------------------------------- */

/*
 * Map a day value to a column index within [0, width).
 * project_end is the maximum sched_end across all tasks.
 */
static int day_to_col(int day, int project_end, int width) {
    if (project_end <= 0) return 0;
    int col = (int)((long)day * width / project_end);
    if (col >= width) col = width - 1;
    return col;
}

/*
 * Build a 3-zone bar for task t:
 *   kind 1 '#' - optimistic zone  [sched_start .. sched_start+pert_min]
 *   kind 2 '~' - expected zone    [sched_start+pert_min .. sched_end]
 *   kind 3 '-' - pessimistic zone [sched_end .. sched_start+pert_max]
 */
static void build_bar(const Task *t, int project_end, int width,
                      char *bar, char *kind) {
    int col_start   = day_to_col(t->sched_start,                                   project_end, width);
    int col_opt_end = day_to_col((int)(t->sched_start + t->pert_min  + 0.5f),      project_end, width);
    int col_exp_end = day_to_col(t->sched_end,                                     project_end, width);
    int col_pes_end = day_to_col((int)(t->sched_start + t->pert_max  + 0.5f),      project_end, width);
    int i;

    memset(bar,  ' ', (size_t)width);
    memset(kind, 0,   (size_t)width);

    for (i = col_start;   i <= col_opt_end && i < width; i++) { bar[i] = '#'; kind[i] = 1; }
    for (i = col_opt_end; i <= col_exp_end && i < width; i++) { bar[i] = '~'; kind[i] = 2; }
    for (i = col_exp_end; i <= col_pes_end && i < width; i++) { bar[i] = '-'; kind[i] = 3; }
}

/* One Gantt row: a task plus its 1-based DAG (topological) position. */
typedef struct { Task *t; int dag; } GanttRow;

/* Sort rows by start day for display; tie-break by id for stable output. */
static int cmp_gantt_row(const void *a, const void *b) {
    const GanttRow *ra = (const GanttRow *)a;
    const GanttRow *rb = (const GanttRow *)b;
    if (ra->t->sched_start != rb->t->sched_start)
        return ra->t->sched_start - rb->t->sched_start;
    return ra->t->id - rb->t->id;
}

void render_gantt(const Project *p, const Company *c, int width) {
    int i, j, project_end = 0;
    char *bar  = (char *)malloc((size_t)(width + 1));
    char *kind = (char *)malloc((size_t)(width + 1));
    GanttRow *rows = (GanttRow *)malloc((size_t)(p->task_count ? p->task_count : 1) * sizeof(GanttRow));

    if (!bar || !kind || !rows) { free(bar); free(kind); free(rows); return; }

    /* Build display rows from the persisted DAG (topological) order; sorted by
     * start day for display only (p->tasks untouched). */
    for (i = 0; i < p->task_count; i++) { rows[i].t = p->tasks[i]; rows[i].dag = p->tasks[i]->topo_order; }
    qsort(rows, (size_t)p->task_count, sizeof(GanttRow), cmp_gantt_row);

    /* Pass 1 - project end */
    for (i = 0; i < p->task_count; i++)
        if (p->tasks[i]->sched_end > project_end)
            project_end = p->tasks[i]->sched_end;


    printf("\n%s=== Gantt Chart  (project duration: %d days) ===%s\n",
           ANSI_BOLD, project_end, ANSI_RESET);
    printf("  Legend: %s#%s optimistic  %s~%s expected  %s-%s pessimistic overrun\n",
           ANSI_BOLD, ANSI_RESET, ANSI_DIM, ANSI_RESET, ANSI_YELLOW, ANSI_RESET);
    printf("%-22s  %-14s  %-18s  %-4s |", "Task", "Criticality", "Assignee", "DAG");
    for (j = 0; j < width; j++) printf(j % 10 == 0 ? "|" : "-");
    printf("\n");

    for (i = 0; i < p->task_count; i++) {
        Task       *t        = rows[i].t;
        const char *color    = t->is_critical ? ANSI_RED : ANSI_GREEN;
        TeamMember *assignee = (c && t->assignee_id != -1)
                               ? company_find_member((Company *)c, t->assignee_id)
                               : NULL;

        build_bar(t, project_end, width, bar, kind);

        /* Left columns */
        printf("%-22.22s  ", t->title);
        if (t->is_critical) printf("%s%-14s%s  ", ANSI_RED,    "[CRITICAL]",     ANSI_RESET);
        else                printf("%s%-14s%s  ", ANSI_DIM,    "[NOT CRITICAL]",  ANSI_RESET);
        if (assignee)       printf("%-18.18s  ", assignee->name);
        else                printf("%s%-18s%s  ", ANSI_YELLOW, "[UNASSIGNED]",    ANSI_RESET);
        if (rows[i].dag > 0) printf("%-4d |", rows[i].dag);
        else                 printf("%-4s |", "-");

        /* Bar */
        for (j = 0; j < width; j++) {
            if      (kind[j] == 1) printf("%s%s#%s", ANSI_BOLD, color, ANSI_RESET);
            else if (kind[j] == 2) printf("%s~%s",   ANSI_DIM,         ANSI_RESET);
            else if (kind[j] == 3) printf("%s-%s",   ANSI_YELLOW,      ANSI_RESET);
            else                   printf(" ");
        }
        printf("\n");
    }

    /* Milestone markers */
    for (i = 0; i < p->milestone_count; i++) {
        Milestone *m   = p->milestones[i];
        int        col = day_to_col(m->deadline_day, project_end, width);
        printf("%s%-28s |%s", ANSI_YELLOW, m->name, ANSI_RESET);
        for (j = 0; j < width; j++) printf(j == col ? "|" : " ");
        printf("%s  M%d (day %d)%s\n", ANSI_YELLOW, m->id, m->deadline_day, ANSI_RESET);
    }

    free(bar);
    free(kind);
    free(rows);
}

/* ---- Gantt chart (HTML) ------------------------------------------------- */

/* Escape a string into an HTML stream (safe inside text and <pre>). */
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

void render_gantt_html(FILE *out, const Project *p, const Company *c, int width) {
    int i, j, project_end = 0;
    char *bar  = (char *)malloc((size_t)(width + 1));
    char *kind = (char *)malloc((size_t)(width + 1));
    GanttRow *rows = (GanttRow *)malloc((size_t)(p->task_count ? p->task_count : 1) * sizeof(GanttRow));

    if (!bar || !kind || !rows) { free(bar); free(kind); free(rows); return; }

    for (i = 0; i < p->task_count; i++) { rows[i].t = p->tasks[i]; rows[i].dag = p->tasks[i]->topo_order; }
    qsort(rows, (size_t)p->task_count, sizeof(GanttRow), cmp_gantt_row);

    for (i = 0; i < p->task_count; i++)
        if (p->tasks[i]->sched_end > project_end)
            project_end = p->tasks[i]->sched_end;

    fprintf(out, "<h2>Gantt (duration: %d days)</h2>\n", project_end);
    fprintf(out, "<p class=\"legend\"># optimistic &middot; ~ expected &middot; - pessimistic overrun</p>\n");
    fprintf(out, "<pre class=\"gantt\">");

    for (i = 0; i < p->task_count; i++) {
        Task *t = rows[i].t;
        const char *cls = t->is_critical ? "crit" : "noncrit";
        TeamMember *a = (c && t->assignee_id != -1)
                        ? company_find_member((Company *)c, t->assignee_id) : NULL;
        char left[160], dag[8];

        build_bar(t, project_end, width, bar, kind);

        if (rows[i].dag > 0) snprintf(dag, sizeof(dag), "%d", rows[i].dag);
        else                 snprintf(dag, sizeof(dag), "-");

        /* left columns padded as raw text, then escaped (entities keep width in <pre>) */
        snprintf(left, sizeof(left), "%-22.22s  %-14s  %-18.18s  %-4s |",
                 t->title,
                 t->is_critical ? "[CRITICAL]" : "[NOT CRITICAL]",
                 a ? a->name : "[UNASSIGNED]",
                 dag);
        html_fputs(out, left);

        /* bar: group consecutive same-kind cells into one styled span */
        j = 0;
        while (j < width) {
            int k0 = kind[j], run = j;
            while (run < width && kind[run] == k0) run++;
            if (k0 == 0) {
                for (; j < run; j++) fputc(' ', out);
            } else {
                const char *bcls = (k0 == 1) ? "opt" : (k0 == 2) ? "exp" : "pess";
                fprintf(out, "<span class=\"%s %s\">", bcls, cls);
                for (; j < run; j++) fputc(bar[j], out);
                fputs("</span>", out);
            }
        }
        fputc('\n', out);
    }

    fprintf(out, "</pre>\n");

    free(bar);
    free(kind);
    free(rows);
}

/* ---- Portfolio Gantt chart ----------------------------------------------- */

void render_portfolio_gantt(const Company *c, int width) {
    int i, j;
    long t0 = 0, t1 = 0;
    long span;
    int datable_count = 0;
    int span_days = 0;

    if (c->project_count == 0) {
        printf("%sNo projects.%s\n", ANSI_DIM, ANSI_RESET);
        return;
    }

    /* Pass 1: compute makespan per project and absolute timeline bounds */
    for (i = 0; i < c->project_count; i++) {
        Project *p = c->projects[i];
        int makespan = 0;
        int k;
        for (k = 0; k < p->task_count; k++)
            if (p->tasks[k]->sched_end > makespan)
                makespan = p->tasks[k]->sched_end;

        if (date_is_valid(p->start_date)) {
            long sa = date_to_days(p->start_date);
            long ea = sa + makespan;
            if (datable_count == 0) {
                t0 = sa;
                t1 = ea;
            } else {
                if (sa < t0) t0 = sa;
                if (ea > t1) t1 = ea;
            }
            datable_count++;
        }
    }

    span = (datable_count > 0) ? (t1 - t0) : 0;
    span_days = (int)span;
    if (span <= 0) span = 1; /* avoid divide-by-zero */

    /* Header */
    printf("\n%s=== Portfolio Gantt (%d projects, %d days span) ===%s\n",
           ANSI_BOLD, c->project_count, span_days, ANSI_RESET);

    /* Ruler */
    printf("%-24s  %5s  |", "Project", "Days");
    for (j = 0; j < width; j++) printf(j % 10 == 0 ? "|" : "-");
    printf("\n");

    /* One row per project */
    for (i = 0; i < c->project_count; i++) {
        Project *p = c->projects[i];
        int makespan = 0;
        int k;
        for (k = 0; k < p->task_count; k++)
            if (p->tasks[k]->sched_end > makespan)
                makespan = p->tasks[k]->sched_end;

        printf("%-24.24s  %5dd  |", p->name, makespan);

        if (!date_is_valid(p->start_date)) {
            printf("%s(no start date)%s", ANSI_DIM, ANSI_RESET);
        } else {
            long sa = date_to_days(p->start_date);
            long ea = sa + makespan;
            int col_start = (int)((sa - t0) * width / span);
            int col_end   = (int)((ea - t0) * width / span);
            if (col_start < 0)       col_start = 0;
            if (col_start > width-1) col_start = width - 1;
            if (col_end   < 0)       col_end   = 0;
            if (col_end   > width-1) col_end   = width - 1;
            if (col_end < col_start) col_end   = col_start;

            for (j = 0; j < width; j++) {
                if (j >= col_start && j <= col_end)
                    printf("%s#%s", ANSI_CYAN, ANSI_RESET);
                else
                    printf(" ");
            }
        }
        printf("\n");
    }
    printf("\n");
}

/* ---- Unified company task Gantt ----------------------------------------- */

/* One color per project (cycled). */
static const char *PROJ_PALETTE[] = {
    ANSI_CYAN, ANSI_GREEN, ANSI_YELLOW, ANSI_RED, "\033[35m", "\033[34m"
};
#define PROJ_PALETTE_N ((int)(sizeof PROJ_PALETTE / sizeof PROJ_PALETTE[0]))

/* Every task of every project on one shared absolute-date timeline, each task's
 * bar drawn in its project's color. Tasks of projects without a valid start_date
 * are skipped (they have no place on a calendar). */
void render_company_gantt(const Company *c, int width) {
    long t0 = 0, t1 = 0;
    long span;
    int  i, j, k, datable = 0, rows = 0;

    if (c->project_count == 0) { printf("%sNo projects.%s\n", ANSI_DIM, ANSI_RESET); return; }

    /* Pass 1: absolute timeline bounds over all tasks of datable projects. */
    for (i = 0; i < c->project_count; i++) {
        Project *p = c->projects[i];
        long sa;
        if (!date_is_valid(p->start_date)) continue;
        sa = date_to_days(p->start_date);
        for (j = 0; j < p->task_count; j++) {
            long s = sa + p->tasks[j]->sched_start;
            long e = sa + p->tasks[j]->sched_end;
            if (datable == 0) { t0 = s; t1 = e; }
            else { if (s < t0) t0 = s; if (e > t1) t1 = e; }
            datable++;
            rows++;
        }
    }

    span = t1 - t0;
    if (span <= 0) span = 1;

    printf("\n%s=== All Tasks - Company Timeline (%d tasks, %d days) ===%s\n",
           ANSI_BOLD, rows, (int)(t1 - t0), ANSI_RESET);
    printf("%-14s  %-16s  |", "Project", "Task");
    for (j = 0; j < width; j++) printf(j % 10 == 0 ? "|" : "-");
    printf("\n");

    /* Rows: one per task, colored by project. */
    for (i = 0; i < c->project_count; i++) {
        Project    *p     = c->projects[i];
        const char *color = PROJ_PALETTE[i % PROJ_PALETTE_N];
        long sa;
        if (!date_is_valid(p->start_date)) continue;
        sa = date_to_days(p->start_date);
        for (j = 0; j < p->task_count; j++) {
            Task *t = p->tasks[j];
            int col_start = (int)((sa + t->sched_start - t0) * width / span);
            int col_end   = (int)((sa + t->sched_end   - t0) * width / span);
            if (col_start < 0)        col_start = 0;
            if (col_start > width - 1) col_start = width - 1;
            if (col_end   < 0)        col_end   = 0;
            if (col_end   > width - 1) col_end   = width - 1;
            if (col_end < col_start)  col_end   = col_start;

            printf("%s%-14.14s%s  %-16.16s  |", color, p->name, ANSI_RESET, t->title);
            for (k = 0; k < width; k++) {
                if (k >= col_start && k <= col_end) printf("%s#%s", color, ANSI_RESET);
                else                                printf(" ");
            }
            printf("\n");
        }
    }

    /* Legend: project -> color swatch. */
    printf("\n  Legend: ");
    for (i = 0; i < c->project_count; i++) {
        if (!date_is_valid(c->projects[i]->start_date)) continue;
        printf("%s##%s %.12s   ", PROJ_PALETTE[i % PROJ_PALETTE_N], ANSI_RESET, c->projects[i]->name);
    }
    printf("\n");
}

/* ---- Task dependency chart ---------------------------------------------- */

#define PRE_COL  26
#define CTR_COL  30

static void pad_to(int n) { while (n-- > 0) printf(" "); }

void render_task_deps(const Project *p, const Task *t) {
    int pre_buf[64], post_buf[64];
    int pre_n = 0, post_n = 0;
    int i, rows, mid;
    char label[CTR_COL + 1];

    for (i = 0; i < t->pre_ids.count; i++) {
        int id = t->pre_ids.data[i];
        if (id != START_NODE_ID && id != END_NODE_ID)
            pre_buf[pre_n++] = id;
    }
    for (i = 0; i < t->post_ids.count; i++) {
        int id = t->post_ids.data[i];
        if (id != START_NODE_ID && id != END_NODE_ID)
            post_buf[post_n++] = id;
    }

    rows = pre_n > post_n ? pre_n : post_n;
    if (rows == 0) rows = 1;
    mid  = rows / 2;

    snprintf(label, sizeof(label), "[%d] %.24s", t->id, t->title);

    printf("\n%s  Dependencies%s\n  ", ANSI_BOLD, ANSI_RESET);
    for (i = 0; i < PRE_COL + CTR_COL + 12; i++) printf("-");
    printf("\n\n");

    for (i = 0; i < rows; i++) {
        int llen, pad;
        printf("  ");

        /* pre column */
        if (i < pre_n) {
            char cell[PRE_COL + 1];
            Task *pre = project_find_task((Project *)p, pre_buf[i]);
            if (pre) snprintf(cell, sizeof(cell), "[%d] %.20s", pre->id, pre->title);
            else     snprintf(cell, sizeof(cell), "[%d]", pre_buf[i]);
            printf("%s", cell);
            pad_to(PRE_COL - (int)strlen(cell));
        } else {
            pad_to(PRE_COL);
        }

        /* arrow + highlighted center + arrow */
        if (i == mid) {
            llen = (int)strlen(label);
            printf(" %s->%s ", ANSI_DIM, ANSI_RESET);
            printf("%s%s%s", ANSI_BOLD, ANSI_YELLOW, label);
            pad = CTR_COL - llen;
            pad_to(pad > 0 ? pad : 0);
            printf("%s", ANSI_RESET);
            printf(" %s->%s ", ANSI_DIM, ANSI_RESET);
        } else {
            pad_to(CTR_COL + 8);
        }

        /* post column */
        if (i < post_n) {
            Task *post = project_find_task((Project *)p, post_buf[i]);
            if (post) printf("[%d] %s", post->id, post->title);
            else      printf("[%d]", post_buf[i]);
        }

        printf("\n");
    }
    printf("\n");
}

/* ---- DAG view ----------------------------------------------------------- */

void render_dag(const Project *p) {
    int i, j;
    printf("\n%s=== Dependency Graph ===%s\n", ANSI_BOLD, ANSI_RESET);

    for (i = 0; i < p->task_count; i++) {
        Task *t = p->tasks[i];
        const char *color = t->is_critical ? ANSI_RED : ANSI_CYAN;

        printf("%s[%d] %s%s", color, t->id, t->title, ANSI_RESET);

        if (t->post_ids.count > 0) {
            printf("  -->  ");
            for (j = 0; j < t->post_ids.count; j++) {
                Task *post = project_find_task((Project *)p, t->post_ids.data[j]);
                if (post) {
                    const char *pc = post->is_critical ? ANSI_RED : ANSI_CYAN;
                    printf("%s[%d]%s", pc, post->id, ANSI_RESET);
                    if (j < t->post_ids.count - 1) printf(", ");
                }
            }
        }

        if (t->alt_ids.count > 0) {
            printf("  %s(alt: ", ANSI_DIM);
            for (j = 0; j < t->alt_ids.count; j++) {
                printf("%d", t->alt_ids.data[j]);
                if (j < t->alt_ids.count - 1) printf(", ");
            }
            printf(")%s", ANSI_RESET);
        }
        printf("\n");
    }
}
