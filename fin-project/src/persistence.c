#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <direct.h>
#include <io.h>
#include "persistence.h"

/* =========================================================================
 * YAML subset writers
 * ========================================================================= */

static void write_str(FILE* f, const char* key, const char* val, int indent) {
	fprintf(f, "%*s%s: \"%s\"\n", indent, "", key, val);
}

static void write_int(FILE* f, const char* key, int val, int indent) {
	fprintf(f, "%*s%s: %d\n", indent, "", key, val);
}

static void write_float(FILE* f, const char* key, float val, int indent) {
	fprintf(f, "%*s%s: %.4f\n", indent, "", key, val);
}

static void write_int_array(FILE* f, const char* key, const int* arr, int count,
	int sentinel, int indent) {
	int i, first = 1;
	fprintf(f, "%*s%s: [", indent, "", key);
	for (i = 0; i < count; i++) {
		if (arr[i] == sentinel) continue;
		if (!first) fprintf(f, ", ");
		fprintf(f, "%d", arr[i]);
		first = 0;
	}
	fprintf(f, "]\n");
}

/* =========================================================================
 * YAML subset parser helpers
 * ========================================================================= */

static int get_indent(const char* line) {
	int n = 0; while (line[n] == ' ') n++; return n;
}

static int split_kv(const char* line, char* key, int kmax, char* val, int vmax) {
	const char* colon = strchr(line, ':');
	int klen; const char* vstart;
	if (!colon) return 0;
	while (*line == ' ') line++;
	klen = (int)(colon - line);
	if (klen <= 0 || klen >= kmax) return 0;
	strncpy(key, line, klen); key[klen] = '\0';
	vstart = colon + 1;
	while (*vstart == ' ') vstart++;
	strncpy(val, vstart, vmax - 1); val[vmax - 1] = '\0';
	{
		int vlen = (int)strlen(val);
		while (vlen > 0 && (val[vlen - 1] == '\n' || val[vlen - 1] == '\r')) val[--vlen] = '\0';
	}
	return 1;
}

static void unquote(const char* src, char* dst, int max) {
	int len = (int)strlen(src);
	if (len >= 2 && src[0] == '"' && src[len - 1] == '"') {
		len -= 2; if (len >= max) len = max - 1;
		strncpy(dst, src + 1, len); dst[len] = '\0';
	}
	else { strncpy(dst, src, max - 1); dst[max - 1] = '\0'; }
}

static int parse_int_array(const char* val, int* arr, int max) {
	const char* p; int count = 0;
	p = strchr(val, '['); if (!p) return 0; p++;
	while (*p && *p != ']' && count < max) {
		while (*p == ' ' || *p == ',') p++;
		if (*p == ']' || *p == '\0') break;
		arr[count++] = (int)strtol(p, (char**)&p, 10);
	}
	return count;
}

/* =========================================================================
 * Project save / load
 * ========================================================================= */

static int save_project_meta(const Project* p, const char* dir) {
	char path[MAX_PATH_LEN]; FILE* f;
	snprintf(path, MAX_PATH_LEN, "%s\\project.yaml", dir);
	f = fopen(path, "w");
	if (!f) { printf("  Save error: %s\n", strerror(errno)); return 0; }

	fprintf(f, "meta:\n");
	write_str(f, "name", p->name, 2);
	write_int(f, "year", p->start_date.year, 2);
	write_int(f, "month", p->start_date.month, 2);
	write_int(f, "day", p->start_date.day, 2);
	write_int_array(f, "member_ids", p->member_ids.data, p->member_ids.count, -999, 2);

	fprintf(f, "\ntasks:\n");
	{
		int i;
		for (i = 0; i < p->task_count; i++) {
			Task* t = p->tasks[i];
			fprintf(f, "  -\n");
			write_int(f, "id", t->id, 4);
			write_str(f, "title", t->title, 4);
			write_str(f, "description", t->description, 4);
			write_int(f, "status", (int)t->status, 4);
			write_float(f, "pert_min", t->pert_min, 4);
			write_float(f, "pert_likely", t->pert_likely, 4);
			write_float(f, "pert_max", t->pert_max, 4);
			write_float(f, "risk", t->risk, 4);
			write_int(f, "required_skills", (int)t->required_skills, 4);
			write_int(f, "assignee_id", t->assignee_id, 4);
			write_int(f, "milestone_id", t->milestone_id, 4);
			write_int_array(f, "pre",      t->pre_ids.data,      t->pre_ids.count,      START_NODE_ID, 4);
			write_int_array(f, "post",     t->post_ids.data,     t->post_ids.count,     END_NODE_ID,   4);
			write_int_array(f, "alt",      t->alt_ids.data,      t->alt_ids.count,      -999,          4);
			write_int_array(f, "work_pre", t->work_pre_ids.data, t->work_pre_ids.count, -999,          4);
		}
	}

	fprintf(f, "\nmilestones:\n");
	{
		int i;
		for (i = 0; i < p->milestone_count; i++) {
			Milestone* m = p->milestones[i];
			fprintf(f, "  -\n");
			write_int(f, "id", m->id, 4);
			write_str(f, "name", m->name, 4);
			write_int(f, "deadline_day", m->deadline_day, 4);
			write_int(f, "priority", m->priority, 4);
			write_int_array(f, "tasks", m->task_ids, m->task_count, -999, 4);
		}
	}

	fclose(f);
	return 1;
}

int project_save(const Project* p, const char* dir) {
	_mkdir(dir);
	return save_project_meta(p, dir);
}

static void parse_meta(const char* key, const char* val,
	char* name, Date* start) {
	if (strcmp(key, "name") == 0) unquote(val, name, MAX_NAME_LEN);
	else if (strcmp(key, "year") == 0) start->year = (int)strtol(val, NULL, 10);
	else if (strcmp(key, "month") == 0) start->month = (int)strtol(val, NULL, 10);
	else if (strcmp(key, "day") == 0) start->day = (int)strtol(val, NULL, 10);
}

static void parse_task(Project* p, Task* t, const char* key, const char* val) {
	char str[MAX_DESC_LEN]; int ids[64]; int n;
	if (strcmp(key, "id") == 0) t->id = (int)strtol(val, NULL, 10);
	else if (strcmp(key, "title") == 0) { unquote(val, str, MAX_TITLE_LEN); strncpy(t->title, str, MAX_TITLE_LEN - 1); }
	else if (strcmp(key, "description") == 0) { unquote(val, str, MAX_DESC_LEN);  strncpy(t->description, str, MAX_DESC_LEN - 1); }
	else if (strcmp(key, "status") == 0) t->status = (TaskStatus)strtol(val, NULL, 10);
	else if (strcmp(key, "pert_min") == 0) t->pert_min = strtof(val, NULL);
	else if (strcmp(key, "pert_likely") == 0) t->pert_likely = strtof(val, NULL);
	else if (strcmp(key, "pert_max") == 0) { t->pert_max = strtof(val, NULL); task_set_pert(t, t->pert_min, t->pert_likely, t->pert_max); }
	else if (strcmp(key, "risk") == 0) task_set_risk(t, strtof(val, NULL));
	else if (strcmp(key, "required_skills") == 0) t->required_skills = (uint32_t)strtol(val, NULL, 10);
	else if (strcmp(key, "assignee_id") == 0) t->assignee_id = (int)strtol(val, NULL, 10);
	else if (strcmp(key, "milestone_id") == 0) t->milestone_id = (int)strtol(val, NULL, 10);
	else if (strcmp(key, "pre")      == 0) { int j; n = parse_int_array(val, ids, 64); for (j=0;j<n;j++) project_link_tasks(p, ids[j], t->id); }
	else if (strcmp(key, "alt")      == 0) { int j; n = parse_int_array(val, ids, 64); for (j=0;j<n;j++) task_add_alt(t, ids[j]); }
	else if (strcmp(key, "work_pre") == 0) { int j; n = parse_int_array(val, ids, 64); for (j=0;j<n;j++) dia_append(&t->work_pre_ids, ids[j]); }
}

static void parse_milestone(Project* p, Milestone* m, const char* key, const char* val) {
	char str[MAX_NAME_LEN]; int ids[64]; int n, j;
	if (strcmp(key, "id") == 0) m->id = (int)strtol(val, NULL, 10);
	else if (strcmp(key, "name") == 0) { unquote(val, str, MAX_NAME_LEN); strncpy(m->name, str, MAX_NAME_LEN - 1); }
	else if (strcmp(key, "deadline_day") == 0) m->deadline_day = (int)strtol(val, NULL, 10);
	else if (strcmp(key, "priority") == 0) m->priority = (int)strtol(val, NULL, 10);
	else if (strcmp(key, "tasks") == 0) { n = parse_int_array(val, ids, 64); for (j = 0;j < n;j++) project_link_task_milestone(p, ids[j], m->id); }
}

Project* project_load(const char* dir) {
	char path[MAX_PATH_LEN], line[512], key[64], val[450];
	FILE* f;
	Project* p = NULL;
	typedef enum { S_NONE, S_META, S_TASKS, S_MILESTONES } Sec;
	Sec sec = S_NONE;
	Task* cur_task = NULL;
	Milestone* cur_ms = NULL;
	Date start = { 2026, 1, 1 };
	char name[MAX_NAME_LEN] = "Untitled";

	snprintf(path, MAX_PATH_LEN, "%s\\project.yaml", dir);
	f = fopen(path, "r");
	if (!f) { printf("  Load error: %s (%s)\n", path, strerror(errno)); return NULL; }

	while (fgets(line, sizeof(line), f)) {
		int indent = get_indent(line);
		char* trimmed = line + indent;
		if (trimmed[0] == '\n' || trimmed[0] == '#') continue;

		if (indent == 0) {
			if (strncmp(trimmed, "meta:", 5) == 0) { sec = S_META;       continue; }
			if (strncmp(trimmed, "tasks:", 6) == 0) { sec = S_TASKS;      if (!p) p = project_create(name, start); continue; }
			if (strncmp(trimmed, "milestones:", 11) == 0) { sec = S_MILESTONES; continue; }
		}

		if (indent == 2 && trimmed[0] == '-') {
			if (sec == S_TASKS && p)      cur_task = project_add_task(p, "", "");
			if (sec == S_MILESTONES && p) cur_ms = project_add_milestone(p, "", 0, 0);
			continue;
		}

		if (!split_kv(trimmed, key, sizeof(key), val, sizeof(val))) continue;

		switch (sec) {
		case S_META:       parse_meta(key, val, name, &start);       break;
		case S_TASKS:      if (cur_task && p) parse_task(p, cur_task, key, val); break;
		case S_MILESTONES: if (cur_ms && p) parse_milestone(p, cur_ms, key, val); break;
		default: break;
		}
	}

	fclose(f);
	if (!p) p = project_create(name, start);
	strncpy(p->save_dir, dir, sizeof(p->save_dir) - 1);
	return p;
}

/* =========================================================================
 * Company save / load
 * ========================================================================= */

int company_save(const Company* c) {
	char path[MAX_PATH_LEN]; FILE* f; int i;

	_mkdir(c->save_dir);

	/* company.yaml — metadata */
	snprintf(path, MAX_PATH_LEN, "%s\\company.yaml", c->save_dir);
	f = fopen(path, "w");
	if (!f) { printf("  Save error: %s\n", strerror(errno)); return 0; }
	fprintf(f, "company:\n");
	write_str(f, "name", c->name, 2);
	fclose(f);

	/* team.yaml — all company members */
	snprintf(path, MAX_PATH_LEN, "%s\\team.yaml", c->save_dir);
	f = fopen(path, "w");
	if (!f) { printf("  Save error: %s\n", strerror(errno)); return 0; }
	fprintf(f, "members:\n");
	for (i = 0; i < c->member_count; i++) {
		TeamMember* m = c->members[i];
		fprintf(f, "  -\n");
		write_int(f, "id", m->id, 4);
		write_str(f, "name", m->name, 4);
		write_str(f, "role", m->role, 4);
		write_int(f, "skills", (int)m->skills, 4);
		write_float(f, "availability", m->availability, 4);
		write_int_array(f, "project_ids", m->project_ids.data, m->project_ids.count, -999, 4);
	}
	fclose(f);

	/* projects/ — one bundle per project */
	snprintf(path, MAX_PATH_LEN, "%s\\projects", c->save_dir);
	_mkdir(path);
	for (i = 0; i < c->project_count; i++) {
		Project* p = c->projects[i];
		char proj_dir[MAX_PATH_LEN];
		snprintf(proj_dir, MAX_PATH_LEN, "%s\\projects\\%s", c->save_dir, p->name);
		project_save(p, proj_dir);
	}

	return 1;
}

Company* company_load(const char* dir) {
	char path[MAX_PATH_LEN], line[512], key[64], val[450];
	FILE* f; Company* c = NULL;
	char name[MAX_NAME_LEN] = "Untitled";
	TeamMember* cur_mb = NULL;

	/* company.yaml */
	snprintf(path, MAX_PATH_LEN, "%s\\company.yaml", dir);
	f = fopen(path, "r");
	if (!f) { printf("  Load error: %s (%s)\n", path, strerror(errno)); return NULL; }
	while (fgets(line, sizeof(line), f)) {
		char* trimmed = line + get_indent(line);
		if (trimmed[0] == '\n' || trimmed[0] == '#') continue;
		if (!split_kv(trimmed, key, sizeof(key), val, sizeof(val))) continue;
		if (strcmp(key, "name") == 0) unquote(val, name, MAX_NAME_LEN);
	}
	fclose(f);

	c = company_create(name);
	if (!c) return NULL;
	strncpy(c->save_dir, dir, sizeof(c->save_dir) - 1);

	/* team.yaml */
	snprintf(path, MAX_PATH_LEN, "%s\\team.yaml", dir);
	f = fopen(path, "r");
	if (f) {
		while (fgets(line, sizeof(line), f)) {
			int indent = get_indent(line);
			char* trimmed = line + indent;
			if (trimmed[0] == '\n' || trimmed[0] == '#') continue;
			if (indent == 2 && trimmed[0] == '-') { cur_mb = company_add_member(c, "", ""); continue; }
			if (!cur_mb || !split_kv(trimmed, key, sizeof(key), val, sizeof(val))) continue;
			{
				char str[MAX_NAME_LEN];
				if (strcmp(key, "id") == 0) cur_mb->id = (int)strtol(val, NULL, 10);
				else if (strcmp(key, "name") == 0) { unquote(val, str, MAX_NAME_LEN); strncpy(cur_mb->name, str, MAX_NAME_LEN - 1); }
				else if (strcmp(key, "role") == 0) { unquote(val, str, MAX_NAME_LEN); strncpy(cur_mb->role, str, MAX_NAME_LEN - 1); }
				else if (strcmp(key, "skills") == 0) cur_mb->skills = (uint32_t)strtol(val, NULL, 10);
				else if (strcmp(key, "availability") == 0) cur_mb->availability = strtof(val, NULL);
				else if (strcmp(key, "project_ids") == 0) {
					int ids[64]; int n = parse_int_array(val, ids, 64); int j;
					for (j = 0; j < n; j++) dia_append(&cur_mb->project_ids, ids[j]);
				}
			}
		}
		fclose(f);
	}

	/* projects/ — load each subdirectory */
	snprintf(path, MAX_PATH_LEN, "%s\\projects", dir);
	{
		struct _finddata_t fd; intptr_t h;
		char search[MAX_PATH_LEN];
		snprintf(search, MAX_PATH_LEN, "%s\\*", path);
		h = _findfirst(search, &fd);
		if (h != -1) {
			do {
				if (fd.attrib & _A_SUBDIR) {
					char proj_dir[MAX_PATH_LEN];
					if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
					snprintf(proj_dir, MAX_PATH_LEN, "%s\\%s", path, fd.name);
					{
						Project* p = project_load(proj_dir);
						if (p && c->project_count < c->project_capacity) {
							if (c->project_count >= c->project_capacity) {
								int nc = c->project_capacity * 2;
								Project** tmp = (Project**)realloc(c->projects, nc * sizeof(Project*));
								if (tmp) { c->projects = tmp; c->project_capacity = nc; }
							}
							c->projects[c->project_count++] = p;
						}
					}
				}
			} while (_findnext(h, &fd) == 0);
			_findclose(h);
		}
	}

	return c;
}
