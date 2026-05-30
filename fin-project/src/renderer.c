#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "renderer.h"
#include "constants.h"

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
 * Fill buf[0..width-1] with the Gantt bar for task t.
 * Legend:  '.' min-max uncertainty range
 *          '#' expected (likely) duration block
 * We print the buffer with colour after.
 */
static void build_bar(const Task *t, int project_end, int width,
                      char *bar, char *kind) {
    int col_min    = day_to_col((int)(t->sched_start + t->pert_min    + 0.5f), project_end, width);
    int col_likely = day_to_col((int)(t->sched_start                         ), project_end, width);
    int col_exp    = day_to_col(t->sched_end,                                  project_end, width);
    int col_max    = day_to_col((int)(t->sched_start + t->pert_max    + 0.5f), project_end, width);
    int i;

    memset(bar,  ' ', (size_t)width);
    memset(kind, 0,   (size_t)width);

    for (i = col_min; i <= col_max && i < width; i++) {
        bar[i]  = '.';
        kind[i] = 1;   /* uncertain range */
    }
    for (i = col_likely; i <= col_exp && i < width; i++) {
        bar[i]  = '#';
        kind[i] = 2;   /* expected block  */
    }
}

void render_gantt(const Project *p, int width) {
    int i, j, project_end = 0;
    char *bar  = (char *)malloc((size_t)(width + 1));
    char *kind = (char *)malloc((size_t)(width + 1));

    if (!bar || !kind) { free(bar); free(kind); return; }

    /* Find project end */
    for (i = 0; i < p->task_count; i++)
        if (p->tasks[i]->sched_end > project_end)
            project_end = p->tasks[i]->sched_end;

    /* Header */
    printf("\n%s=== Gantt Chart  (project duration: %d days) ===%s\n",
           ANSI_BOLD, project_end, ANSI_RESET);
    printf("%-28s |", "Task");
    for (j = 0; j < width; j++) printf(j % 10 == 0 ? "|" : "-");
    printf("\n");

    /* One row per task */
    for (i = 0; i < p->task_count; i++) {
        Task *t = p->tasks[i];
        const char *color = t->is_critical ? ANSI_RED : ANSI_GREEN;

        build_bar(t, project_end, width, bar, kind);

        printf("%-28s |", t->title);
        for (j = 0; j < width; j++) {
            if (kind[j] == 2) {
                printf("%s%s%c%s", ANSI_BOLD, color, bar[j], ANSI_RESET);
            } else if (kind[j] == 1) {
                printf("%s%c%s", ANSI_DIM, bar[j], ANSI_RESET);
            } else {
                printf(" ");
            }
        }
        printf("%s%s\n", t->is_critical ? "  [CRITICAL]" : "", ANSI_RESET);
    }

    /* Milestone markers */
    for (i = 0; i < p->milestone_count; i++) {
        Milestone *m = p->milestones[i];
        int col = day_to_col(m->deadline_day, project_end, width);
        printf("%s%-28s |%s", ANSI_YELLOW, m->name, ANSI_RESET);
        for (j = 0; j < width; j++)
            printf(j == col ? "|" : " ");
        printf("%s  M%d (day %d)%s\n", ANSI_YELLOW, m->id, m->deadline_day, ANSI_RESET);
    }

    free(bar);
    free(kind);
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
