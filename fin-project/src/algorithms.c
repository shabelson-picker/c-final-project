#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "algorithms.h"
#include "team.h"
#include "ui.h"

#define MAX_ASSIGN_PASSES 8

/* Assignment policy is module-level so the menu can set it without threading a
 * parameter through every scheduling call site. Default keeps the old behavior. */
static AssignPolicy g_assign_policy = ASSIGN_EARLIEST_FREE;

void         assign_set_policy(AssignPolicy policy) { g_assign_policy = policy; }
AssignPolicy assign_get_policy(void)                { return g_assign_policy; }

static int popcount32(uint32_t x) {
    int n = 0;
    while (x) { x &= x - 1; n++; }
    return n;
}

/* Build a 4-element ranking key (lower is better) from a candidate's metrics,
 * ordered by policy. The trailing member id makes ties deterministic. */
static void score_key(AssignPolicy pol, int free_day, int load, int surplus,
                      int id, int key[4]) {
    switch (pol) {
        case ASSIGN_BALANCED:  key[0] = load;    key[1] = free_day; key[2] = surplus; break;
        case ASSIGN_BEST_FIT:  key[0] = surplus; key[1] = free_day; key[2] = load;    break;
        case ASSIGN_EARLIEST_FREE:
        default:               key[0] = free_day; key[1] = load;    key[2] = surplus; break;
    }
    key[3] = id;
}

static int key_less(const int a[4], const int b[4]) {
    int i;
    for (i = 0; i < 4; i++)
        if (a[i] != b[i]) return a[i] < b[i];
    return 0;
}

/* =========================================================================
 * Topological sort - Kahn's algorithm
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
 * Effective duration - strategy-dependent
 * ========================================================================= */

static float effective_duration(const Task *t, ScheduleStrategy s) {
    if (s == SCHED_RISK_WEIGHTED_CRITICAL)
        return t->pert_expected * (1.0f + t->risk * 0.5f);
    if (s == SCHED_PESSIMISTIC)
        return t->pert_max;
    return t->pert_expected;
}

/* =========================================================================
 * Forward pass - earliest start / end per task
 * ========================================================================= */

void forward_pass(Project *p, const int *order, int n, ScheduleStrategy s) {
    int i, j;
    for (i = 0; i < n; i++) {
        Task *t = p->tasks[order[i]];
        int earliest = 0;

        if (t->fixed_time) continue;   /* immovable window (vacation): keep sched_start/end */

        for (j = 0; j < t->pre_ids.count; j++) {
            Task *pre = project_find_task(p, t->pre_ids.data[j]);
            if (pre && pre->sched_end > earliest) earliest = pre->sched_end;
        }
        for (j = 0; j < t->work_pre_ids.count; j++) {
            Task *pre = project_find_task(p, t->work_pre_ids.data[j]);
            if (pre && pre->sched_end > earliest) earliest = pre->sched_end;
        }

        if (t->min_start > earliest) earliest = t->min_start;  /* cross-project / pinned floor */
        t->sched_start = earliest;
        t->sched_end   = earliest + (int)(effective_duration(t, s) + 0.5f);
    }
}

/* =========================================================================
 * Backward pass - latest start, slack, critical path
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
    int n, i;
    int *order = topo_sort(p, &n);
    if (!order) {
        fprintf(stderr, "[scheduler] cycle detected in task graph\n");
        return 0;
    }
    forward_pass(p, order, n, strategy);
    backward_pass(p, order, n, strategy);
    for (i = 0; i < n; i++) p->tasks[order[i]]->topo_order = i + 1;
    free(order);
    return 1;
}

void schedule_print_report(const Project *p) {
    int i;
    cprintf(C_BOLD, "\n=== Schedule Report ===\n");
    cprintf(C_BOLD, "%-4s  %-30s  %5s  %5s  %5s  %8s\n",
            "ID", "Title", "Start", "End", "Slack", "Status");
    printf("---------------------------------------------------------------\n");
    for (i = 0; i < p->task_count; i++) {
        Task *t = p->tasks[i];
        cprintf(ui_zebra(i), "%-4d  %-30s  %5d  %5d  %5d  %s\n",
                t->id, t->title,
                t->sched_start, t->sched_end, t->slack,
                t->is_critical ? "[CRITICAL]" : "");
    }
}

/* =========================================================================
 * Greedy assignment - helper functions
 * ========================================================================= */

static void clear_work_assignments(Project *p) {
    int i;
    for (i = 0; i < p->task_count; i++) {
        dia_free(&p->tasks[i]->work_pre_ids);
        dia_init(&p->tasks[i]->work_pre_ids);
        p->tasks[i]->min_start = 0;            /* recomputed from cross-project floor */
        if (!p->tasks[i]->manually_assigned)   /* keep pinned assignments */
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

/* Returns index into c->members[] of the best qualified member under the active
 * policy, or -1 if no qualified member is on the project roster. Ranks on free
 * day, current workload (member_load), and skill surplus (extra skills beyond
 * what the task needs - lower keeps generalists available). */
static int find_best_member(const Company *c, const Project *p, const Task *t,
                            const int *member_free_day, const int *member_load,
                            AssignPolicy policy) {
    int j, best_mi = -1, best_key[4];
    for (j = 0; j < p->member_ids.count; j++) {
        TeamMember *m = company_find_member((Company *)c, p->member_ids.data[j]);
        int mi, surplus, key[4];
        if (!m || !team_member_has_skills(m, t->required_skills)) continue;
        for (mi = 0; mi < c->member_count; mi++)
            if (c->members[mi]->id == m->id) break;
        if (mi == c->member_count) continue;
        surplus = popcount32(m->skills & ~t->required_skills);
        score_key(policy, member_free_day[mi], member_load[mi], surplus, m->id, key);
        if (best_mi == -1 || key_less(key, best_key)) {
            memcpy(best_key, key, sizeof best_key);
            best_mi = mi;
        }
    }
    return best_mi;
}

/* Suggest a company member NOT on the project roster who has all of the task's
 * required skills. Returns the member id, or -1 if none qualifies. Pure (no I/O)
 * so the UI can drive the "add to project?" decision. Relies on p->member_ids
 * being sorted (dia_find_index = bsearch). */
int suggest_company_member(const Company *c, const Project *p, const Task *t) {
    int mi;
    for (mi = 0; mi < c->member_count; mi++) {
        TeamMember *m = c->members[mi];
        if (!team_member_has_skills(m, t->required_skills)) continue;
        if (dia_find_index(&p->member_ids, m->id) != -1) continue;  /* already on roster */
        return m->id;
    }
    return -1;
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

/* Cross-project member calendar.
 * For every company member, fill floor[mi] with the project-relative day before
 * which they are unavailable because of work already committed in OTHER projects:
 *   floor = max over other-project tasks assigned to the member of
 *           (other.start_date + task.sched_end) - this.start_date
 * clamped at 0. This serializes a shared member across projects (the bug where
 * member_free_day reset to 0 per project, so one person started day 0 everywhere).
 * Backward compatible: if this or another project has no valid start_date, that
 * project contributes nothing, so single-project / undated setups behave as before.
 * floor[] must have room for c->member_count ints. */
static void compute_external_floor(const Company *c, int project_idx, int *floor) {
    const Project *self = c->projects[project_idx];
    long self_start;
    int pi, ti, mi;

    for (mi = 0; mi < c->member_count; mi++) floor[mi] = 0;
    if (!date_is_valid(self->start_date)) return;
    self_start = date_to_days(self->start_date);

    for (pi = 0; pi < c->project_count; pi++) {
        const Project *q;
        long q_start;
        if (pi == project_idx) continue;
        q = c->projects[pi];
        if (!date_is_valid(q->start_date)) continue;
        q_start = date_to_days(q->start_date);

        for (ti = 0; ti < q->task_count; ti++) {
            const Task *t = q->tasks[ti];
            long abs_end;
            int  rel;
            if (t->assignee_id == -1) continue;
            for (mi = 0; mi < c->member_count; mi++)
                if (c->members[mi]->id == t->assignee_id) break;
            if (mi == c->member_count) continue;       /* assignee left the company */
            abs_end = q_start + t->sched_end;           /* day the commitment frees up */
            rel     = (int)(abs_end - self_start);      /* in this project's day frame  */
            if (rel > floor[mi]) floor[mi] = rel;
        }
    }
}

/* One assignment pass. Returns 1 if all tasks were assigned, 0 otherwise.
 * Unassignable tasks are left with assignee_id == -1 for the caller to resolve.
 * member_free_day is seeded from the cross-project floor so a member committed
 * elsewhere is not booked from day 0 here. */
static int assignment_pass(Company *c, Project *p, Task **sorted,
                            const int *floor, AssignPolicy policy,
                            int *member_free_day, int *member_last_task,
                            int *member_load) {
    int i, j, all_assigned = 1;

    for (i = 0; i < c->member_count; i++) {
        member_free_day[i]  = floor[i];
        member_last_task[i] = -1;
        member_load[i]      = 0;
    }

    for (i = 0; i < p->task_count; i++) {
        Task *t     = sorted[i];
        int   best_mi;

        /* Sync already-assigned tasks into member state */
        if (t->assignee_id != -1) {
            for (j = 0; j < c->member_count; j++)
                if (c->members[j]->id == t->assignee_id) {
                    t->min_start        = floor[j];   /* pinned task still waits on other projects */
                    member_free_day[j]  = t->sched_end;
                    member_last_task[j] = t->id;
                    member_load[j]++;
                    break;
                }
            continue;
        }

        best_mi = find_best_member(c, p, t, member_free_day, member_load, policy);

        if (best_mi == -1) { all_assigned = 0; continue; }

        apply_work_dep_if_conflict(t, best_mi, member_free_day, member_last_task);

        t->assignee_id = c->members[best_mi]->id;
        t->min_start   = floor[best_mi];   /* cross-project earliest-start floor */
        update_member_state(t, best_mi, member_free_day, member_last_task);
        member_load[best_mi]++;
    }
    return all_assigned;
}

/* =========================================================================
 * Resource-overlap resolution (fixpoint)
 * Serialize any two same-assignee tasks whose scheduled windows overlap by
 * adding a work_pre edge (the later-starting task waits for the earlier one),
 * rescheduling, and repeating until a full scan adds nothing. Pushing one task
 * can create a fresh overlap with a third, so this MUST iterate to a fixpoint.
 * Edges are oriented by current sched_start, so the graph stays acyclic;
 * bounded by the number of same-member task pairs.
 * ========================================================================= */

/* Sort key: group by assignee, then by start day, then by id (stable order). */
static int cmp_by_assignee_then_start(const void *a, const void *b) {
    const Task *ta = *(const Task * const *)a;
    const Task *tb = *(const Task * const *)b;
    if (ta->assignee_id != tb->assignee_id) return ta->assignee_id - tb->assignee_id;
    if (ta->sched_start != tb->sched_start) return ta->sched_start - tb->sched_start;
    return ta->id - tb->id;
}

static void resolve_resource_overlaps(Project *p, ScheduleStrategy s) {
    int iter, max_iter = p->task_count * p->task_count + 1;
    Task **bymember = (Task **)malloc(p->task_count * sizeof(Task *));
    if (!bymember) return;

    for (iter = 0; iter < max_iter; iter++) {
        int added = 0, i, k;

        if (!schedule_project(p, s)) break;   /* cycle detected - bail */

        /* Group tasks by assignee, sorted by start within each member. Two of a
         * member's tasks can only overlap if they are neighbours in this order
         * (sorted-interval property), so a single linear scan suffices. */
        memcpy(bymember, p->tasks, p->task_count * sizeof(Task *));
        qsort(bymember, p->task_count, sizeof(Task *), cmp_by_assignee_then_start);

        for (i = 1; i < p->task_count; i++) {
            Task *prev = bymember[i - 1];
            Task *cur  = bymember[i];
            Task *waiter, *dep;
            int dup = 0;

            if (prev->assignee_id == -1 || cur->assignee_id != prev->assignee_id)
                continue;
            if (cur->sched_start >= prev->sched_end)   /* sorted: prev starts first */
                continue;                              /* no overlap */

            /* Decide which task moves. A fixed_time block (vacation) never moves:
             * the flexible task is pushed after it. If both are fixed we cannot
             * resolve the clash, so skip (avoids spinning to max_iter). */
            if (cur->fixed_time && prev->fixed_time)
                continue;
            if (cur->fixed_time) { waiter = prev; dep = cur; }  /* push earlier task after the block */
            else                 { waiter = cur;  dep = prev; } /* default: later waits for earlier */

            /* work_pre_ids is unsorted - linear contains check */
            for (k = 0; k < waiter->work_pre_ids.count; k++)
                if (waiter->work_pre_ids.data[k] == dep->id) { dup = 1; break; }
            if (dup) continue;

            dia_append(&waiter->work_pre_ids, dep->id);
            added = 1;
        }

        if (!added) break;   /* fixpoint: no overlaps remain */
    }

    free(bymember);
}

/* =========================================================================
 * Public greedy assignment entry point
 * ========================================================================= */

void assign_members_greedy(Company *c, int project_idx, ScheduleStrategy strategy) {
    Project  *p;
    int       pass, prev_assigned;
    int      *member_free_day;
    int      *member_last_task;
    int      *member_load;
    int      *external_floor;
    AssignPolicy policy = g_assign_policy;
    Task    **sorted;

    if (project_idx < 0 || project_idx >= c->project_count) return;
    p = c->projects[project_idx];

    if (p->member_ids.count == 0) {
        printf("  No members on project roster. Add members first (option 5 in project menu).\n");
        return;
    }

    clear_work_assignments(p);
    schedule_project(p, strategy);

    member_free_day  = (int *)calloc(c->member_count, sizeof(int));
    member_last_task = (int *)malloc(c->member_count * sizeof(int));
    member_load      = (int *)calloc(c->member_count, sizeof(int));
    external_floor   = (int *)calloc(c->member_count, sizeof(int));
    sorted           = (Task **)malloc(p->task_count * sizeof(Task *));

    if (!member_free_day || !member_last_task || !member_load || !external_floor || !sorted) {
        free(member_free_day); free(member_last_task); free(member_load);
        free(external_floor); free(sorted);
        return;
    }

    /* Snapshot each member's commitments in OTHER projects once up front, so this
     * project schedules around them instead of double-booking from day 0. */
    compute_external_floor(c, project_idx, external_floor);

    prev_assigned = -1;
    for (pass = 0; pass < MAX_ASSIGN_PASSES; pass++) {
        int assigned_count = 0, i;
        sort_tasks_by_start(sorted, p->tasks, p->task_count);
        if (assignment_pass(c, p, sorted, external_floor, policy,
                            member_free_day, member_last_task, member_load)) break;
        for (i = 0; i < p->task_count; i++)
            if (p->tasks[i]->assignee_id != -1) assigned_count++;
        if (assigned_count == prev_assigned) break;  /* no progress - stop */
        prev_assigned = assigned_count;
        schedule_project(p, strategy);
    }

    /* Greedy only catches conflicts visible in the pre-push schedule; this
     * loop resolves any overlaps that emerge after tasks are pushed. */
    resolve_resource_overlaps(p, strategy);

    printf("  Assignment complete (%d pass%s).\n", pass + 1, pass ? "es" : "");

    free(member_free_day);
    free(member_last_task);
    free(member_load);
    free(external_floor);
    free(sorted);
}
