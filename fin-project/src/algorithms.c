#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "algorithms.h"
#include "team.h"

#define MAX_ASSIGN_PASSES 8

/* =========================================================================
 * Topological sort — Kahn's algorithm
 * Considers both pre_ids (logical) and work_pre_ids (resource) edges.
 * ========================================================================= */

int *topo_sort(Project *p, int *out_count) {
    int i, j, n = p->task_count;
    int *in_degree = (int *)calloc(n, sizeof(int));
    int *order     = (int *)malloc(n * sizeof(int));
    int *queue     = (int *)malloc(n * sizeof(int));
    int  head = 0, tail = 0, placed = 0;

    if (!in_degree || !order || !queue) {
        free(in_degree); free(order); free(queue);
        *out_count = -1; return NULL;
    }

    for (i = 0; i < n; i++) {
        int cnt = 0;
        for (j = 0; j < p->tasks[i]->pre_ids.count; j++)
            if (p->tasks[i]->pre_ids.data[j] != START_NODE_ID) cnt++;
        cnt += p->tasks[i]->work_pre_ids.count;
        in_degree[i] = cnt;
    }

    for (i = 0; i < n; i++)
        if (in_degree[i] == 0) queue[tail++] = i;

    while (head < tail) {
        int idx = queue[head++];
        order[placed++] = idx;

        /* Logical successors (post_ids) */
        for (j = 0; j < p->tasks[idx]->post_ids.count; j++) {
            int post_id = p->tasks[idx]->post_ids.data[j];
            int k;
            if (post_id == END_NODE_ID) continue;
            for (k = 0; k < n; k++)
                if (p->tasks[k]->id == post_id) {
                    if (--in_degree[k] == 0) queue[tail++] = k;
                    break;
                }
        }

        /* Resource successors (tasks that list current task in work_pre_ids) */
        for (j = 0; j < n; j++) {
            int k;
            for (k = 0; k < p->tasks[j]->work_pre_ids.count; k++)
                if (p->tasks[j]->work_pre_ids.data[k] == p->tasks[idx]->id) {
                    if (--in_degree[j] == 0) queue[tail++] = j;
                    break;
                }
        }
    }

    free(in_degree); free(queue);
    if (placed != n) { free(order); *out_count = -1; return NULL; }
    *out_count = n;
    return order;
}

/* =========================================================================
 * Effective duration — strategy-dependent
 * ========================================================================= */

static float effective_duration(const Task *t, ScheduleStrategy s) {
    if (s == SCHED_RISK_WEIGHTED_CRITICAL)
        return t->pert_expected * (1.0f + t->risk * 0.5f);
    return t->pert_expected;
}

/* =========================================================================
 * Forward pass — earliest start / end per task
 * ========================================================================= */

void forward_pass(Project *p, const int *order, int n, ScheduleStrategy s) {
    int i, j;
    for (i = 0; i < n; i++) {
        Task *t = p->tasks[order[i]];
        int earliest = 0;

        for (j = 0; j < t->pre_ids.count; j++) {
            Task *pre = project_find_task(p, t->pre_ids.data[j]);
            if (pre && pre->sched_end > earliest) earliest = pre->sched_end;
        }
        for (j = 0; j < t->work_pre_ids.count; j++) {
            Task *pre = project_find_task(p, t->work_pre_ids.data[j]);
            if (pre && pre->sched_end > earliest) earliest = pre->sched_end;
        }

        t->sched_start = earliest;
        t->sched_end   = earliest + (int)(effective_duration(t, s) + 0.5f);
    }
}

/* =========================================================================
 * Backward pass — latest start, slack, critical path
 * ========================================================================= */

static int has_any_successor(Project *p, const Task *t, int n) {
    int j, k;
    for (j = 0; j < t->post_ids.count; j++)
        if (t->post_ids.data[j] != END_NODE_ID) return 1;
    for (j = 0; j < n; j++)
        for (k = 0; k < p->tasks[j]->work_pre_ids.count; k++)
            if (p->tasks[j]->work_pre_ids.data[k] == t->id) return 1;
    return 0;
}

void backward_pass(Project *p, const int *order, int n, ScheduleStrategy s) {
    int i, j, project_end = 0;

    for (i = 0; i < n; i++)
        if (p->tasks[i]->sched_end > project_end)
            project_end = p->tasks[i]->sched_end;

    for (i = 0; i < n; i++) {
        Task *t = p->tasks[i];
        t->latest_start = has_any_successor(p, t, n)
                          ? project_end
                          : project_end - (int)(effective_duration(t, s) + 0.5f);
    }

    for (i = n - 1; i >= 0; i--) {
        Task *t = p->tasks[order[i]];

        for (j = 0; j < t->post_ids.count; j++) {
            Task *post;
            if (t->post_ids.data[j] == END_NODE_ID) continue;
            post = project_find_task(p, t->post_ids.data[j]);
            if (post) {
                int ls = post->latest_start - (int)(effective_duration(t, s) + 0.5f);
                if (ls < t->latest_start) t->latest_start = ls;
            }
        }

        for (j = 0; j < n; j++) {
            int k;
            for (k = 0; k < p->tasks[j]->work_pre_ids.count; k++)
                if (p->tasks[j]->work_pre_ids.data[k] == t->id) {
                    int ls = p->tasks[j]->latest_start - (int)(effective_duration(t, s) + 0.5f);
                    if (ls < t->latest_start) t->latest_start = ls;
                    break;
                }
        }

        t->slack       = t->latest_start - t->sched_start;
        t->is_critical = (t->slack == 0) ? 1 : 0;
    }
}

/* =========================================================================
 * Public scheduling entry point
 * ========================================================================= */

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

/* =========================================================================
 * Greedy assignment — helper functions
 * ========================================================================= */

static void clear_work_assignments(Project *p) {
    int i;
    for (i = 0; i < p->task_count; i++) {
        dia_free(&p->tasks[i]->work_pre_ids);
        dia_init(&p->tasks[i]->work_pre_ids);
        p->tasks[i]->assignee_id = -1;
    }
}

static int cmp_by_sched_start(const void *a, const void *b) {
    return (*(const Task **)a)->sched_start - (*(const Task **)b)->sched_start;
}

static void sort_tasks_by_start(Task **sorted, Task **src, int n) {
    memcpy(sorted, src, n * sizeof(Task *));
    qsort(sorted, n, sizeof(Task *), cmp_by_sched_start);
}

/* Returns index into c->members[] of the best available qualified member,
 * or -1 if no qualified member is on the project roster. */
static int find_best_member(const Company *c, const Project *p,
                             const Task *t, const int *member_free_day) {
    int j, best_mi = -1, best_free = 0x7fffffff;
    for (j = 0; j < p->member_ids.count; j++) {
        TeamMember *m = company_find_member((Company *)c, p->member_ids.data[j]);
        int mi;
        if (!m || !team_member_has_skills(m, t->required_skills)) continue;
        for (mi = 0; mi < c->member_count; mi++)
            if (c->members[mi]->id == m->id) break;
        if (mi == c->member_count) continue;
        if (member_free_day[mi] < best_free) {
            best_free = member_free_day[mi];
            best_mi   = mi;
        }
    }
    return best_mi;
}

static void warn_no_member(const Task *t) {
    int k, first = 1;
    printf("  [WARNING] No qualified member for [%d] %s — missing: [", t->id, t->title);
    for (k = 0; k < 8; k++)
        if (t->required_skills & (1u << k)) {
            if (!first) printf(", ");
            printf("%s", SKILL_NAMES[k]);
            first = 0;
        }
    printf("]\n");
}

static void apply_work_dep_if_conflict(Task *t, int best_mi,
                                        const int *member_free_day,
                                        const int *member_last_task) {
    int k;
    if (member_free_day[best_mi] <= t->sched_start) return;
    if (member_last_task[best_mi] == -1) return;
    for (k = 0; k < t->work_pre_ids.count; k++)
        if (t->work_pre_ids.data[k] == member_last_task[best_mi]) return;
    dia_append(&t->work_pre_ids, member_last_task[best_mi]);
}

static void update_member_state(Task *t, int best_mi,
                                 int *member_free_day, int *member_last_task) {
    member_free_day[best_mi]  = t->sched_end;
    member_last_task[best_mi] = t->id;
}

/* One assignment pass. Returns 1 if all tasks were assigned, 0 otherwise. */
static int assignment_pass(Company *c, Project *p, Task **sorted,
                            int *member_free_day, int *member_last_task) {
    int i, j, all_assigned = 1;

    memset(member_free_day, 0, c->member_count * sizeof(int));
    for (i = 0; i < c->member_count; i++) member_last_task[i] = -1;

    for (i = 0; i < p->task_count; i++) {
        Task *t     = sorted[i];
        int   best_mi;

        /* Sync already-assigned tasks into member state */
        if (t->assignee_id != -1) {
            for (j = 0; j < c->member_count; j++)
                if (c->members[j]->id == t->assignee_id) {
                    member_free_day[j]  = t->sched_end;
                    member_last_task[j] = t->id;
                    break;
                }
            continue;
        }

        best_mi = find_best_member(c, p, t, member_free_day);

        if (best_mi == -1) { warn_no_member(t); all_assigned = 0; continue; }

        apply_work_dep_if_conflict(t, best_mi, member_free_day, member_last_task);

        t->assignee_id = c->members[best_mi]->id;
        update_member_state(t, best_mi, member_free_day, member_last_task);
    }
    return all_assigned;
}

/* =========================================================================
 * Public greedy assignment entry point
 * ========================================================================= */

void assign_members_greedy(Company *c, int project_idx) {
    Project  *p;
    int       pass;
    int      *member_free_day;
    int      *member_last_task;
    Task    **sorted;

    if (project_idx < 0 || project_idx >= c->project_count) return;
    p = c->projects[project_idx];

    clear_work_assignments(p);
    schedule_project(p, SCHED_EARLIEST_DEADLINE);

    member_free_day  = (int *)calloc(c->member_count, sizeof(int));
    member_last_task = (int *)malloc(c->member_count * sizeof(int));
    sorted           = (Task **)malloc(p->task_count * sizeof(Task *));

    if (!member_free_day || !member_last_task || !sorted) {
        free(member_free_day); free(member_last_task); free(sorted);
        return;
    }

    for (pass = 0; pass < MAX_ASSIGN_PASSES; pass++) {
        sort_tasks_by_start(sorted, p->tasks, p->task_count);
        if (assignment_pass(c, p, sorted, member_free_day, member_last_task)) break;
        schedule_project(p, SCHED_EARLIEST_DEADLINE);
    }

    printf("  Assignment complete (%d pass%s).\n", pass + 1, pass ? "es" : "");

    free(member_free_day);
    free(member_last_task);
    free(sorted);
}
