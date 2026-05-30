#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"
#include "project.h"
#include "company.h"

/* ---- topological sort (Kahn's algorithm) -------------------------------- */

/*
 * Returns a heap-allocated array of task indices (into p->tasks[]) in
 * topological order. *out_count is set to the number of elements.
 * Returns NULL and sets *out_count = -1 if a cycle is detected.
 * Caller must free() the returned array.
 */
static int *topo_sort(Project *p, int *out_count) {
    int i, j, n = p->task_count;
    int *in_degree = (int *)calloc(n, sizeof(int));
    int *order     = (int *)malloc(n * sizeof(int));
    int *queue     = (int *)malloc(n * sizeof(int));
    int  head = 0, tail = 0, placed = 0;

    if (!in_degree || !order || !queue) {
        free(in_degree); free(order); free(queue);
        *out_count = -1;
        return NULL;
    }

    /* Count in-degrees — sentinel START_NODE_ID is not a real predecessor */
    for (i = 0; i < n; i++) {
        int cnt = 0;
        for (j = 0; j < p->tasks[i]->pre_ids.count; j++)
            if (p->tasks[i]->pre_ids.data[j] != START_NODE_ID) cnt++;
        in_degree[i] = cnt;
    }

    /* Seed queue with all zero-in-degree nodes */
    for (i = 0; i < n; i++)
        if (in_degree[i] == 0) queue[tail++] = i;

    while (head < tail) {
        int idx = queue[head++];
        order[placed++] = idx;

        /* Decrement in-degree of user-task successors only */
        for (j = 0; j < p->tasks[idx]->post_ids.count; j++) {
            int post_id = p->tasks[idx]->post_ids.data[j];
            int k;
            if (post_id == END_NODE_ID) continue;   /* skip sentinel */
            for (k = 0; k < n; k++) {
                if (p->tasks[k]->id == post_id) {
                    if (--in_degree[k] == 0)
                        queue[tail++] = k;
                    break;
                }
            }
        }
    }

    free(in_degree);
    free(queue);

    if (placed != n) {   /* cycle detected */
        free(order);
        *out_count = -1;
        return NULL;
    }

    *out_count = n;
    return order;
}

/* Effective duration: risk-weighted PERT expected value */
static float effective_duration(const Task *t, ScheduleStrategy s) {
    if (s == SCHED_RISK_WEIGHTED_CRITICAL)
        return t->pert_expected * (1.0f + t->risk * 0.5f);
    return t->pert_expected;
}

/* ---- forward pass ------------------------------------------------------- */

static void forward_pass(Project *p, const int *order, int n, ScheduleStrategy s) {
    int i, j;
    for (i = 0; i < n; i++) {
        Task *t = p->tasks[order[i]];
        int earliest = 0;

        /* earliest start = max(end of all predecessors) */
        for (j = 0; j < t->pre_ids.count; j++) {
            Task *pre = project_find_task(p, t->pre_ids.data[j]);
            if (pre && pre->sched_end > earliest)
                earliest = pre->sched_end;
        }

        t->sched_start = earliest;
        t->sched_end   = earliest + (int)(effective_duration(t, s) + 0.5f);
    }
}

/* ---- backward pass ------------------------------------------------------ */

static void backward_pass(Project *p, const int *order, int n, ScheduleStrategy s) {
    int i, j;

    /* Find project end = max sched_end across all tasks */
    int project_end = 0;
    for (i = 0; i < n; i++)
        if (p->tasks[i]->sched_end > project_end)
            project_end = p->tasks[i]->sched_end;

    /* Leaf = no user-task successors (only END_NODE_ID or nothing) */
    for (i = 0; i < n; i++) {
        Task *t = p->tasks[i];
        int has_user_succ = 0;
        for (j = 0; j < t->post_ids.count; j++)
            if (t->post_ids.data[j] != END_NODE_ID) { has_user_succ = 1; break; }

        if (!has_user_succ)
            t->latest_start = project_end - (int)(effective_duration(t, s) + 0.5f);
        else
            t->latest_start = project_end; /* tightened in reverse pass below */
    }

    /* Reverse topological order — skip END_NODE_ID successors */
    for (i = n - 1; i >= 0; i--) {
        Task *t = p->tasks[order[i]];
        for (j = 0; j < t->post_ids.count; j++) {
            Task *post;
            if (t->post_ids.data[j] == END_NODE_ID) continue;
            post = project_find_task(p, t->post_ids.data[j]);
            if (post) {
                int ls = post->latest_start - (int)(effective_duration(t, s) + 0.5f);
                if (ls < t->latest_start)
                    t->latest_start = ls;
            }
        }
        t->slack       = t->latest_start - t->sched_start;
        t->is_critical = (t->slack == 0) ? 1 : 0;
    }
}

/* ---- public API --------------------------------------------------------- */

int schedule_project(Project *p, ScheduleStrategy strategy) {
    int n;
    int *order = topo_sort(p, &n);
    if (!order) {
        fprintf(stderr, "[scheduler] cycle detected in task graph\n");
        return 0;
    }

    forward_pass(p, order, n, strategy);
    backward_pass(p, order, n, strategy);

    free(order);
    return 1;
}

void assign_members_greedy(Company *c, int project_idx) {
    int i, j;
    Project *p;
    if (project_idx < 0 || project_idx >= c->project_count) return;
    p = c->projects[project_idx];

    for (i = 0; i < p->task_count; i++) {
        Task *t = p->tasks[i];
        if (t->assignee_id != -1) continue;

        for (j = 0; j < p->member_ids.count; j++) {
            TeamMember *m = company_find_member(c, p->member_ids.data[j]);
            if (m && team_member_has_skills(m, t->required_skills)) {
                t->assignee_id = m->id;
                break;
            }
        }
    }
}

void schedule_print_report(const Project *p) {
    int i;
    printf("\n=== Schedule Report ===\n");
    printf("%-4s  %-30s  %5s  %5s  %5s  %8s\n",
           "ID", "Title", "Start", "End", "Slack", "Status");
    printf("---------------------------------------------------------------\n");

    for (i = 0; i < p->task_count; i++) {
        Task *t = p->tasks[i];
        printf("%-4d  %-30s  %5d  %5d  %5d  %s\n",
               t->id, t->title,
               t->sched_start, t->sched_end, t->slack,
               t->is_critical ? "[CRITICAL]" : "");
    }
}
