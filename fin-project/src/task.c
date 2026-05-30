#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "task.h"
#include "dynamic_int_array.h"

static const char *STATUS_LABELS[] = {
    "TODO", "IN_PROGRESS", "DONE", "CANCELLED", "BLOCKED"
};

Task *task_create(int id, const char *title, const char *description) {
    Task *t = (Task *)malloc(sizeof(Task));
    if (!t) return NULL;

    memset(t, 0, sizeof(Task));
    dia_init(&t->pre_ids);
    dia_init(&t->post_ids);
    dia_init(&t->alt_ids);
    dia_init(&t->work_pre_ids);
    t->id          = id;
    t->status      = STATUS_TODO;
    t->assignee_id = -1;
    t->milestone_id = -1;
    t->sched_start = -1;
    t->sched_end   = -1;
    t->latest_start = -1;

    strncpy(t->title,       title,       MAX_TITLE_LEN - 1);
    strncpy(t->description, description, MAX_DESC_LEN  - 1);

    return t;
}

void task_destroy(Task *t) {
    if (!t) return;
    dia_free(&t->pre_ids);
    dia_free(&t->post_ids);
    dia_free(&t->alt_ids);
    dia_free(&t->work_pre_ids);
    free(t);
}

void task_set_pert(Task *t, float min, float likely, float max) {
    float sigma;
    t->pert_min    = min;
    t->pert_likely = likely;
    t->pert_max    = max;
    t->pert_expected = (min + 4.0f * likely + max) / 6.0f;
    sigma = (max - min) / 6.0f;
    t->pert_variance = sigma * sigma;
}

void task_set_risk(Task *t, float risk) {
    if      (risk < 0.0f) t->risk = 0.0f;
    else if (risk > 1.0f) t->risk = 1.0f;
    else                  t->risk = risk;
}

void task_set_skills(Task *t, uint32_t skills) {
    t->required_skills = skills;
}

int task_add_pre(Task *t, int pre_id) {
    return dia_append(&t->pre_ids, pre_id);
}

int task_add_post(Task *t, int post_id) {
    return dia_append(&t->post_ids, post_id);
}

int task_add_alt(Task *t, int alt_id) {
    return dia_append(&t->alt_ids, alt_id);
}

int task_remove_pre(Task *t, int pre_id) {
    return dia_remove(&t->pre_ids, pre_id);
}

int task_remove_post(Task *t, int post_id) {
    return dia_remove(&t->post_ids, post_id);
}

int task_can_be_done_by(const Task *t, uint32_t member_skills) {
    return (member_skills & t->required_skills) == t->required_skills;
}

void task_print(const Task *t) {
    int i, first = 1;

    printf("[%d] %-30s  %-12s  PERT:%.1f/%.1f/%.1f(exp=%.1f)  risk=%2.0f/10",
           t->id, t->title, STATUS_LABELS[t->status],
           t->pert_min, t->pert_likely, t->pert_max, t->pert_expected,
           t->risk * 10.0f);

    if (t->required_skills) {
        printf("  skills:[");
        for (i = 0; i < 8; i++) {
            if (t->required_skills & (1u << i)) {
                if (!first) printf(",");
                printf("%s", SKILL_NAMES[i]);
                first = 0;
            }
        }
        printf("]");
    }

    if (t->is_critical) printf("  [CRITICAL]");
    if (t->sched_start >= 0)
        printf("  sched: day %d-%d", t->sched_start, t->sched_end);
    printf("\n");
}
