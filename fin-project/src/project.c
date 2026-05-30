#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "project.h"

#define INIT_CAP 8

static int ensure_task_cap(Project *p) {
    if (p->task_count < p->task_capacity) return 1;
    int new_cap = p->task_capacity * 2;
    Task **tmp = (Task **)realloc(p->tasks, new_cap * sizeof(Task *));
    if (!tmp) return 0;
    p->tasks = tmp;
    p->task_capacity = new_cap;
    return 1;
}

static int ensure_milestone_cap(Project *p) {
    if (p->milestone_count < p->milestone_capacity) return 1;
    int new_cap = p->milestone_capacity * 2;
    Milestone **tmp = (Milestone **)realloc(p->milestones, new_cap * sizeof(Milestone *));
    if (!tmp) return 0;
    p->milestones = tmp;
    p->milestone_capacity = new_cap;
    return 1;
}

Project *project_create(const char *name, Date start_date) {
    Project *p = (Project *)malloc(sizeof(Project));
    if (!p) return NULL;

    memset(p, 0, sizeof(Project));
    strncpy(p->name, name, MAX_NAME_LEN - 1);
    p->start_date     = start_date;
    p->task_capacity  = INIT_CAP;
    p->milestone_capacity = INIT_CAP;

    p->tasks      = (Task **)     malloc(INIT_CAP * sizeof(Task *));
    p->milestones = (Milestone **)malloc(INIT_CAP * sizeof(Milestone *));
    p->start_node = task_create(START_NODE_ID, "[START]", "");
    p->end_node   = task_create(END_NODE_ID,   "[END]",   "");

    if (!p->tasks || !p->milestones || !p->start_node || !p->end_node) {
        project_destroy(p);
        return NULL;
    }

    dia_init(&p->member_ids);

    p->start_node->sched_start = 0;
    p->start_node->sched_end   = 0;
    p->next_task_id      = 1;
    p->next_milestone_id = 1;

    return p;
}

void project_destroy(Project *p) {
    int i;
    if (!p) return;
    task_destroy(p->start_node);
    task_destroy(p->end_node);
    for (i = 0; i < p->task_count;      i++) task_destroy(p->tasks[i]);
    for (i = 0; i < p->milestone_count; i++) milestone_destroy(p->milestones[i]);
    dia_free(&p->member_ids);
    free(p->tasks);
    free(p->milestones);
    free(p);
}

/* ---- add entities ------------------------------------------------------- */

Task* project_add_task(Project* p, const char* title, const char* desc) {
	if (!ensure_task_cap(p)) return NULL;
	Task* t = task_create(p->next_task_id++, title, desc);
	if (!t) return NULL;
	p->tasks[p->task_count++] = t;

	/* Every new task is initially an island: start -> t -> end */
	task_add_post(p->start_node, t->id);
	task_add_pre(t, START_NODE_ID);
	task_add_post(t, END_NODE_ID);
	task_add_pre(p->end_node, t->id);

	return t;
}

Milestone* project_add_milestone(Project* p, const char* name, int deadline_day, int priority) {
	if (!ensure_milestone_cap(p)) return NULL;
	Milestone* m = milestone_create(p->next_milestone_id++, name, deadline_day, priority);
	if (!m) return NULL;
	p->milestones[p->milestone_count++] = m;
	return m;
}

/* ---- remove entities ---------------------------------------------------- */

int project_remove_task(Project* p, int task_id) {
	int i, j, vidx = -1;
	Task* victim = NULL;

	for (i = 0; i < p->task_count; i++)
		if (p->tasks[i]->id == task_id) { victim = p->tasks[i]; vidx = i; break; }
	if (!victim) return 0;

	/* Detach from every predecessor's post list; reconnect orphans to END. */
	for (j = 0; j < victim->pre_ids.count; j++) {
		Task* pre = project_find_task(p, victim->pre_ids.data[j]);
		if (!pre) continue;
		task_remove_post(pre, task_id);
		if (pre->id != START_NODE_ID && pre->post_ids.count == 0) {
			task_add_post(pre, END_NODE_ID);
			task_add_pre(p->end_node, pre->id);
		}
	}

	/* Detach from every successor's pre list; reconnect orphans to START. */
	for (j = 0; j < victim->post_ids.count; j++) {
		Task* post = project_find_task(p, victim->post_ids.data[j]);
		if (!post) continue;
		task_remove_pre(post, task_id);
		if (post->id != END_NODE_ID && post->pre_ids.count == 0) {
			task_add_pre(post, START_NODE_ID);
			task_add_post(p->start_node, post->id);
		}
	}

	task_destroy(victim);
	p->tasks[vidx] = p->tasks[--p->task_count];
	return 1;
}

/* ---- lookups ------------------------------------------------------------ */

Task* project_find_task(const Project* p, int id) {
	int i;
	if (id == START_NODE_ID) return p->start_node;
	if (id == END_NODE_ID)   return p->end_node;
	for (i = 0; i < p->task_count; i++)
		if (p->tasks[i]->id == id) return p->tasks[i];
	return NULL;
}

Milestone* project_find_milestone(const Project* p, int id) {
	int i;
	for (i = 0; i < p->milestone_count; i++)
		if (p->milestones[i]->id == id) return p->milestones[i];
	return NULL;
}

/* ---- relationships ------------------------------------------------------ */

int project_link_tasks(Project* p, int pre_id, int post_id) {
	if (pre_id == post_id)
	{
		printf("ids should be different");
		return 0;
	}
	Task* pre = project_find_task(p, pre_id);
	Task* post = project_find_task(p, post_id);
	if (!pre || !post) return 0;
	{ int j; for (j = 0; j < post->pre_ids.count; j++)
		if (post->pre_ids.data[j] == pre_id) {
			printf("  Link %d -> %d already exists.\n", pre_id, post_id);
			return 0;
		}
	}
	if (project_would_create_cycle(p, pre_id, post_id))
	{
		return 0;
	}
	/* post now has a real predecessor - detach from start_node */
	task_remove_post(p->start_node, post_id);
	task_remove_pre(post, START_NODE_ID);

	/* pre now has a real successor - detach from end_node */
	task_remove_post(pre, END_NODE_ID);
	task_remove_pre(p->end_node, pre_id);

	return task_add_post(pre, post_id) && task_add_pre(post, pre_id);
}

int project_link_task_milestone(Project* p, int task_id, int milestone_id) {
	Task* t = project_find_task(p, task_id);
	Milestone* m = project_find_milestone(p, milestone_id);
	if (!t || !m) return 0;
	t->milestone_id = milestone_id;
	return milestone_add_task(m, task_id);
}

static int project_find_cyclic_paths(const Project* p, int pre_id, int post_id, DynamicIntArray* visited, DynamicIntArray* path)
{
	if (pre_id == post_id)
	{
		dia_append(path, post_id);
		return 1;

	}
	int ind = dia_find_index(visited, post_id);
	if (ind != -1)  //alredy visited
	{
		return 0;
	}
	else
	{
		dia_sort_insert(visited, post_id);
	}
	Task* t = project_find_task(p, post_id);
	for (int i = 0;i < t->post_ids.count;i++)
	{
		int res = project_find_cyclic_paths(p, pre_id, t->post_ids.data[i], visited, path);
		if (res)
		{
			dia_append(path, post_id);
			return 1;
		}
	}
	return 0;
}
int project_would_create_cycle(const Project* p, int pre_id, int post_id)
{
	DynamicIntArray visited;
	DynamicIntArray path;

	dia_init(&visited);
	dia_init(&path);
	int res = project_find_cyclic_paths(p, pre_id, post_id, &visited, &path);
	if (res)
	{
		int i;
		Task* tmp;
		printf("  Cycle detected: ");
		for (i = path.count - 1; i >= 0; i--)
		{
			tmp = project_find_task(p, path.data[i]);
			if (tmp) printf("%s", tmp->title);
			else     printf("[%d]", path.data[i]);
			if (i > 0) printf(" -> ");
		}
		/* close the loop */
		tmp = project_find_task(p, path.data[path.count - 1]);
		if (tmp) printf(" -> %s", tmp->title);
		printf("\n");
	}
	dia_free(&visited);
	dia_free(&path);
	return res;
}




/* ---- summary ------------------------------------------------------------ */

void project_print_summary(const Project *p) {
    int i;
    printf("=== Project: %s  (%d-%02d-%02d) ===\n",
           p->name, p->start_date.year, p->start_date.month, p->start_date.day);
    printf("Tasks: %d   Milestones: %d   Members on roster: %d\n\n",
           p->task_count, p->milestone_count, p->member_ids.count);

    printf("-- Tasks --\n");
    for (i = 0; i < p->task_count; i++)
        task_print(p->tasks[i]);

    printf("\n-- Milestones --\n");
    for (i = 0; i < p->milestone_count; i++)
        milestone_print(p->milestones[i]);
}

