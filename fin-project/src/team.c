#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "team.h"

TeamMember *team_member_create(int id, const char *name, const char *role) {
    TeamMember *m = (TeamMember *)malloc(sizeof(TeamMember));
    if (!m) return NULL;

    memset(m, 0, sizeof(TeamMember));
    m->id           = id;
    m->availability = 1.0f;
    strncpy(m->name, name, MAX_NAME_LEN - 1);
    strncpy(m->role, role, MAX_NAME_LEN - 1);
    dia_init(&m->project_ids);
    tra_init(&m->tasks);

    return m;
}

void team_member_destroy(TeamMember *m) {
    if (!m) return;
    dia_free(&m->project_ids);
    tra_free(&m->tasks);
    free(m);
}

void team_member_add_skill(TeamMember *m, Skill skill) {
    m->skills |= (uint32_t)skill;
}

void team_member_remove_skill(TeamMember *m, Skill skill) {
    m->skills &= ~(uint32_t)skill;
}

int team_member_has_skills(const TeamMember *m, uint32_t required) {
    return (m->skills & required) == required;
}

void team_member_print(const TeamMember *m) {
    int i, first = 1;
    printf("[%d] %-20s  %-18s  avail: %2.0f%%  skills: [",
           m->id, m->name, m->role, m->availability * 100.0f);
    for (i = 0; i < 8; i++) {
        if (m->skills & (1u << i)) {
            if (!first) printf(", ");
            printf("%s", SKILL_NAMES[i]);
            first = 0;
        }
    }
    printf("]");
    if (m->project_ids.count > 0) {
        printf("  projects: [");
        for (i = 0; i < m->project_ids.count; i++) {
            if (i > 0) printf(", ");
            printf("%d", m->project_ids.data[i]);
        }
        printf("]");
    }
    printf("\n");
}
