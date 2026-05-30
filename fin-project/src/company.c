#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "company.h"
#include "constants.h"
#include "persistence.h"
#include "file_browser.h"
#include "menus.h"

#define INIT_CAP 8

static int ensure_member_cap(Company *c) {
    if (c->member_count < c->member_capacity) return 1;
    int new_cap = c->member_capacity * 2;
    TeamMember **tmp = (TeamMember **)realloc(c->members, new_cap * sizeof(TeamMember *));
    if (!tmp) return 0;
    c->members          = tmp;
    c->member_capacity  = new_cap;
    return 1;
}

static int ensure_project_cap(Company *c) {
    if (c->project_count < c->project_capacity) return 1;
    int new_cap = c->project_capacity * 2;
    Project **tmp = (Project **)realloc(c->projects, new_cap * sizeof(Project *));
    if (!tmp) return 0;
    c->projects          = tmp;
    c->project_capacity  = new_cap;
    return 1;
}

Company *company_create(const char *name) {
    Company *c = (Company *)malloc(sizeof(Company));
    if (!c) return NULL;

    memset(c, 0, sizeof(Company));
    strncpy(c->name, name, MAX_NAME_LEN - 1);

    c->member_capacity  = INIT_CAP;
    c->project_capacity = INIT_CAP;
    c->next_member_id   = 1;

    c->members  = (TeamMember **)malloc(INIT_CAP * sizeof(TeamMember *));
    c->projects = (Project **)   malloc(INIT_CAP * sizeof(Project *));

    if (!c->members || !c->projects) { company_destroy(c); return NULL; }
    return c;
}

void company_destroy(Company *c) {
    int i;
    if (!c) return;
    for (i = 0; i < c->member_count;  i++) team_member_destroy(c->members[i]);
    for (i = 0; i < c->project_count; i++) project_destroy(c->projects[i]);
    free(c->members);
    free(c->projects);
    free(c);
}

TeamMember *company_add_member(Company *c, const char *name, const char *role) {
    if (!ensure_member_cap(c)) return NULL;
    TeamMember *m = team_member_create(c->next_member_id++, name, role);
    if (!m) return NULL;
    c->members[c->member_count++] = m;
    return m;
}

int company_remove_member(Company *c, int member_id) {
    int i;
    for (i = 0; i < c->member_count; i++) {
        if (c->members[i]->id == member_id) {
            team_member_destroy(c->members[i]);
            c->members[i] = c->members[--c->member_count];
            return 1;
        }
    }
    return 0;
}

TeamMember *company_find_member(Company *c, int member_id) {
    int i;
    for (i = 0; i < c->member_count; i++)
        if (c->members[i]->id == member_id) return c->members[i];
    return NULL;
}

Project *company_add_project(Company *c, const char *name, Date start_date) {
    if (!ensure_project_cap(c)) return NULL;
    Project *p = project_create(name, start_date);
    if (!p) return NULL;
    c->projects[c->project_count++] = p;
    return p;
}

int company_remove_project(Company *c, int project_idx) {
    if (project_idx < 0 || project_idx >= c->project_count) return 0;
    project_destroy(c->projects[project_idx]);
    c->projects[project_idx] = c->projects[--c->project_count];
    return 1;
}

int company_assign_member(Company *c, int project_idx, int member_id, int task_id) {
    Project    *p;
    TeamMember *m;

    if (project_idx < 0 || project_idx >= c->project_count) return 0;
    p = c->projects[project_idx];
    m = company_find_member(c, member_id);
    if (!m) return 0;

    /* Add to project roster if not already on it */
    if (dia_find_index(&m->project_ids, project_idx) == -1)
        dia_sort_insert(&m->project_ids, project_idx);

    /* Keep p->member_ids sorted so dia_find_index (bsearch) is valid */
    if (dia_find_index(&p->member_ids, member_id) == -1)
        dia_sort_insert(&p->member_ids, member_id);

    /* Assign to task if requested */
    if (task_id != -1) {
        Task *t = project_find_task(p, task_id);
        if (!t) return 0;
        if (!team_member_has_skills(m, t->required_skills)) {
            printf("  Member '%s' lacks required skills for task '%s'.\n",
                   m->name, t->title);
            return 0;
        }
        t->assignee_id = member_id;
        tra_append(&m->tasks, project_idx, task_id);
    }
    return 1;
}

Company *company_new_interactive(void) {
    char name[MAX_NAME_LEN], parent[MAX_PATH_LEN], dir[MAX_PATH_LEN];
    Company *c;

    read_str("  Company name: ", name, MAX_NAME_LEN);
    if (name[0] == '\0') strncpy(name, "Untitled", MAX_NAME_LEN - 1);

    printf("  Select parent directory:\n");
    if (!dir_browser(parent, MAX_PATH_LEN)) return NULL;

    snprintf(dir, MAX_PATH_LEN, "%s\\%s", parent, name);

    c = company_create(name);
    if (!c) { fprintf(stderr, "Fatal: could not allocate company.\n"); return NULL; }

    strncpy(c->save_dir, dir, sizeof(c->save_dir) - 1);

    if (!company_save(c)) {
        printf("  Could not create company bundle at '%s'.\n", c->save_dir);
        company_destroy(c);
        return NULL;
    }
    printf("  Company '%s' created at %s\n", c->name, c->save_dir);
    return c;
}

Company *company_load_interactive(void) {
    char dir[MAX_PATH_LEN];
    Company *c;

    printf("  Select company directory:\n");
    if (!dir_browser(dir, MAX_PATH_LEN)) return NULL;

    c = company_load(dir);
    if (!c) { printf("  Could not load company from '%s'.\n", dir); return NULL; }

    printf("  Loaded company '%s' (%d projects, %d members).\n",
           c->name, c->project_count, c->member_count);
    return c;
}

void company_print_summary(const Company *c) {
    int i;
    printf("\n  Company: %s\n", c->name);
    printf("  Members: %d   Projects: %d\n", c->member_count, c->project_count);
    printf("\n  Projects:\n");
    for (i = 0; i < c->project_count; i++)
        printf("    [%d] %s  (%d tasks)\n",
               i, c->projects[i]->name, c->projects[i]->task_count);
}
