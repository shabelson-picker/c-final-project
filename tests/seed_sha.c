/* solo-ran tool: enrich the SHA_inc demo bundle so it looks alive.
 * Rebuilds the projects as granular, multi-phase plans with parallel branches,
 * fan-in integration, multiple milestones (deadlines tuned to show on-track /
 * at-risk / late), varied status & risk, and broad rosters that share a backend
 * bottleneck (Carol id5) across projects so the cross-project calendar bites.
 * One-shot generator (NOT a regression test). Run from the fin-project root.
 */
#include <stdio.h>
#include <string.h>
#include "company.h"
#include "algorithms.h"
#include "persistence.h"   /* company_load / company_save */

#define FE 1
#define BE 2
#define HW 4
#define EMB 8
#define QA 16
#define DEVOPS 32
#define DESIGN 64
#define PM 128

/* A granular task: title, desc, PERT, risk, skills, status, and up to two
 * predecessors given as indices into the project's step array (-1 = none). */
typedef struct {
    const char *title, *desc;
    float mn, lk, mx, risk;
    unsigned skills;
    int status;     /* 0 todo, 1 in-progress, 2 done */
    int dep, dep2;
} Step;

typedef struct { const char *name; int deadline, priority, anchor; } MS;

static int find_proj(Company *c, const char *name) {
    int i;
    for (i = 0; i < c->project_count; i++)
        if (strcmp(c->projects[i]->name, name) == 0) return i;
    return -1;
}

static void clear_tasks(Project *p) {
    int i;
    while (p->task_count > 0) project_remove_task(p, p->tasks[0]->id);
    for (i = 0; i < p->milestone_count; i++) milestone_destroy(p->milestones[i]);
    p->milestone_count = 0;   /* drop stale milestones too, for a clean rebuild */
}

/* Build a project from step + milestone tables, roster members, schedule it. */
static void build(Company *c, int idx, const Step *s, int n,
                  const MS *ms, int msn, const int *roster, int rn) {
    Project *p = c->projects[idx];
    int ids[64], i;
    if (n > 64) n = 64;
    clear_tasks(p);

    for (i = 0; i < n; i++) {
        Task *t = project_add_task(p, s[i].title, s[i].desc);
        task_set_pert(t, s[i].mn, s[i].lk, s[i].mx);
        task_set_risk(t, s[i].risk);
        task_set_skills(t, s[i].skills);
        t->status = (TaskStatus)s[i].status;
        ids[i] = t->id;
    }
    for (i = 0; i < n; i++) {
        if (s[i].dep  >= 0) project_link_tasks(p, ids[s[i].dep],  ids[i]);
        if (s[i].dep2 >= 0) project_link_tasks(p, ids[s[i].dep2], ids[i]);
    }
    for (i = 0; i < msn; i++) {
        Milestone *m = project_add_milestone(p, ms[i].name, ms[i].deadline, ms[i].priority);
        if (m && ms[i].anchor >= 0 && ms[i].anchor < n)
            project_link_task_milestone(p, ids[ms[i].anchor], m->id);
    }
    for (i = 0; i < rn; i++) company_assign_member(c, idx, roster[i], -1);
    assign_members_greedy(c, idx, SCHED_EARLIEST_DEADLINE);
    schedule_project(p, SCHED_EARLIEST_DEADLINE);
}

int main(void) {
    Company *c = company_load("my_companies\\SHA_inc");
    int idx;
    if (!c) { printf("Could not load SHA_inc\n"); return 1; }

    /* ===== Apollo - Web Platform (16 tasks, diamond build phase, 3 milestones) ===== */
    {
        static const Step s[] = {
            /*0*/ {"Requirements",        "scope & stories",      2,3,4,  0.1f, DESIGN, 2, -1,-1},
            /*1*/ {"User Research",       "interviews",           1,2,4,  0.1f, DESIGN, 2,  0,-1},
            /*2*/ {"Wireframes",          "low-fi screens",       2,3,5,  0.2f, DESIGN, 2,  1,-1},
            /*3*/ {"Design System",       "tokens & components",  3,4,7,  0.2f, DESIGN, 1,  2,-1},
            /*4*/ {"DB Schema",           "tables & migrations",  2,3,5,  0.3f, BE,     1,  3,-1},
            /*5*/ {"Auth Service",        "login & sessions",     3,5,8,  0.4f, BE,     0,  4,-1},
            /*6*/ {"REST API",            "core endpoints",       5,7,11, 0.4f, BE,     0,  5,-1},
            /*7*/ {"FE Scaffolding",      "router & state",       2,3,5,  0.2f, FE,     1,  3,-1},
            /*8*/ {"Component Library",   "shared widgets",       3,4,6,  0.3f, FE,     0,  7,-1},
            /*9*/ {"Dashboard UI",        "main screens",         4,6,9,  0.3f, FE,     0,  8,-1},
            /*10*/{"API Integration",     "wire FE to BE",        2,3,5,  0.4f, BE,     0,  6, 9},
            /*11*/{"E2E Tests",           "user-flow tests",      2,4,6,  0.3f, QA,     0, 10,-1},
            /*12*/{"Perf Hardening",      "profiling & fixes",    2,3,5,  0.3f, QA,     0, 11,-1},
            /*13*/{"CI/CD Pipeline",      "build & release",      2,3,5,  0.3f, DEVOPS, 0, 10,-1},
            /*14*/{"Security Review",     "pen test & fixes",     2,3,6,  0.5f, BE,     0, 12,-1},
            /*15*/{"Prod Deploy v1",      "ship to production",   1,2,3,  0.5f, DEVOPS, 0, 14,13},
        };
        static const MS ms[] = {
            {"Design Complete", 12, 2, 3},
            {"Beta",            30, 2, 11},
            {"v1 Release",      40, 3, 15},
        };
        static const int roster[] = {6, 3, 5, 7, 2, 10, 8};  /* David Alice Carol Eve joe Hannah Frank */
        idx = find_proj(c, "Apollo");
        if (idx >= 0) build(c, idx, s, 16, ms, 3, roster, 7);
    }

    /* ===== Borealis - Embedded Firmware (14 tasks, HW/SW branches, 2 milestones) ===== */
    {
        static const Step s[] = {
            /*0*/ {"Schematic Design",    "board schematic",      3,5,8,  0.3f, HW,      2, -1,-1},
            /*1*/ {"PCB Layout",          "routing & DRC",        3,4,7,  0.3f, HW,      2,  0,-1},
            /*2*/ {"Board Bring-up",      "power & clocks",       2,4,7,  0.4f, HW|EMB,  1,  1,-1},
            /*3*/ {"Bootloader",          "secure boot",          3,4,7,  0.4f, EMB,     1,  2,-1},
            /*4*/ {"RTOS Port",           "scheduler & HAL",      5,8,13, 0.5f, EMB,     1,  2,-1},
            /*5*/ {"Sensor Drivers",      "I2C/SPI drivers",      4,6,9,  0.4f, EMB,     0,  4,-1},
            /*6*/ {"Comms Stack",         "BLE/UART",             4,6,10, 0.5f, EMB,     0,  4,-1},
            /*7*/ {"Power Management",    "sleep states",         3,4,7,  0.4f, EMB,     0,  5,-1},
            /*8*/ {"HW/SW Integration",   "bring it together",    3,5,8,  0.5f, EMB|BE,  0,  6, 7},
            /*9*/ {"Calibration",         "sensor cal",           2,3,5,  0.3f, HW,      0,  8,-1},
            /*10*/{"HIL Test",            "hardware-in-loop QA",  3,5,7,  0.3f, QA,      0,  9,-1},
            /*11*/{"Compliance",          "EMC/safety",           3,5,9,  0.6f, HW,      0, 10,-1},
            /*12*/{"Field Trial",         "beta units",           2,4,7,  0.5f, QA,      0, 11,-1},
            /*13*/{"Release Candidate",   "RC firmware",          1,2,3,  0.4f, EMB,     0, 12, 3},
        };
        static const MS ms[] = {
            {"Bring-up Done", 14, 2, 2},
            {"Borealis RC",   34, 3, 13},
        };
        static const int roster[] = {4, 9, 5, 1, 7, 11};  /* Bob Grace Carol shimi Eve jimi */
        idx = find_proj(c, "Borealis");
        if (idx >= 0) build(c, idx, s, 14, ms, 2, roster, 6);
    }

    /* ===== Cobalt - Data Pipeline (13 tasks, fan-in to GA, 2 milestones) ===== */
    {
        static const Step s[] = {
            /*0*/ {"Data Model",          "schema & contracts",   2,3,5,  0.2f, BE,     2, -1,-1},
            /*1*/ {"Source Connectors",   "ingest adapters",      3,5,8,  0.4f, BE,     1,  0,-1},
            /*2*/ {"Ingest Service",      "buffering & retries",  4,6,10, 0.4f, BE,     1,  1,-1},
            /*3*/ {"Transform Jobs",      "ETL & enrichment",     4,6,9,  0.4f, BE,     0,  2,-1},
            /*4*/ {"Data Quality",        "validation rules",     2,3,5,  0.3f, QA,     0,  3,-1},
            /*5*/ {"Warehouse Load",      "load & partitioning",  3,4,7,  0.3f, BE,     0,  3,-1},
            /*6*/ {"Orchestration",       "DAG scheduling",       3,4,7,  0.5f, DEVOPS, 0,  5,-1},
            /*7*/ {"Monitoring",          "metrics & alerts",     2,3,5,  0.3f, DEVOPS, 0,  6,-1},
            /*8*/ {"Cost Controls",       "autoscaling",          2,3,5,  0.4f, DEVOPS, 0,  6,-1},
            /*9*/ {"Reconciliation",      "QA & audits",          3,4,6,  0.3f, QA,     0,  4, 5},
            /*10*/{"Runbook & Docs",      "ops docs",             1,2,4,  0.2f, PM,     0,  7,-1},
            /*11*/{"Dashboards",          "exec dashboards",      2,3,5,  0.3f, DESIGN, 0,  7,-1},
            /*12*/{"General Availability","GA cutover",           1,2,4,  0.6f, DEVOPS, 0,  9, 8},
        };
        static const MS ms[] = {
            {"Pipeline MVP", 18, 2, 5},
            {"Cobalt GA",    30, 3, 12},
        };
        static const int roster[] = {5, 2, 3, 8, 10, 11};  /* Carol joe Alice Frank Hannah jimi */
        idx = find_proj(c, "Cobalt");
        if (idx >= 0) build(c, idx, s, 13, ms, 2, roster, 6);
    }

    /* ===== proj_1 - Mobile App (9 tasks, 1 milestone) ===== */
    {
        static const Step s[] = {
            /*0*/ {"UX Flows",        "screens & flows",   2,3,5, 0.2f, DESIGN, 2, -1,-1},
            /*1*/ {"API Contract",    "mobile BFF",        2,3,5, 0.3f, BE,     1,  0,-1},
            /*2*/ {"iOS Shell",       "nav & theming",     3,5,8, 0.3f, FE,     0,  0,-1},
            /*3*/ {"Android Shell",   "nav & theming",     3,5,8, 0.3f, FE,     0,  0,-1},
            /*4*/ {"Auth Flow",       "OAuth + biometric", 3,4,7, 0.4f, BE,     0,  1,-1},
            /*5*/ {"Feature Screens", "core features",     4,6,9, 0.4f, FE,     0,  2, 3},
            /*6*/ {"Push & Offline",  "notifications",     2,4,6, 0.4f, BE,     0,  4,-1},
            /*7*/ {"QA Pass",         "device matrix",     3,4,6, 0.3f, QA,     0,  5, 6},
            /*8*/ {"Store Release",   "app store submit",  1,2,4, 0.5f, DEVOPS, 0,  7,-1},
        };
        static const MS ms[] = { {"App v1", 28, 3, 8} };
        static const int roster[] = {6, 3, 10, 5, 7, 2};  /* David Alice Hannah Carol Eve joe */
        idx = find_proj(c, "proj_1");
        if (idx >= 0) build(c, idx, s, 9, ms, 1, roster, 6);
    }

    /* ===== proj_2 - Platform Migration (8 tasks, 1 milestone) ===== */
    {
        static const Step s[] = {
            /*0*/ {"Assessment",      "inventory & risks", 2,3,5, 0.3f, PM,     2, -1,-1},
            /*1*/ {"Target Arch",     "cloud design",      3,4,7, 0.3f, BE,     1,  0,-1},
            /*2*/ {"IaC Modules",     "terraform",         3,5,8, 0.4f, DEVOPS, 1,  1,-1},
            /*3*/ {"Data Migration",  "ETL & cutover plan",4,6,10,0.5f, BE,     0,  1,-1},
            /*4*/ {"Service Port",    "containerize",      4,6,9, 0.4f, BE,     0,  2,-1},
            /*5*/ {"Cutover Rehearsal","dry runs",         2,3,5, 0.5f, DEVOPS, 0,  3, 4},
            /*6*/ {"Go-Live",         "production cutover",1,2,4, 0.7f, DEVOPS, 0,  5,-1},
            /*7*/ {"Decommission",    "retire old stack",  1,2,3, 0.2f, DEVOPS, 0,  6,-1},
        };
        static const MS ms[] = { {"Migration Go-Live", 24, 3, 6} };
        static const int roster[] = {8, 5, 2, 9, 11};  /* Frank Carol joe Grace jimi */
        idx = find_proj(c, "proj_2");
        if (idx >= 0) build(c, idx, s, 8, ms, 1, roster, 5);
    }

    /* Tune milestone deadlines relative to their actual forecast so the portfolio
     * shows a realistic MIX of on-track / at-risk / late instead of all-late. */
    {
        int pi, mi, k = 0;
        for (pi = 0; pi < c->project_count; pi++) {
            Project *p = c->projects[pi];
            for (mi = 0; mi < p->milestone_count; mi++) {
                int fc = milestone_forecast_day(p, p->milestones[mi]);
                if (fc < 0) continue;
                switch (k % 3) {
                    case 0: p->milestones[mi]->deadline_day = fc + 8;          break; /* on track */
                    case 1: p->milestones[mi]->deadline_day = fc + 1;          break; /* at risk  */
                    default:p->milestones[mi]->deadline_day = fc > 4 ? fc - 4 : 0; break; /* late */
                }
                k++;
            }
        }
    }

    if (company_save(c)) printf("SHA_inc enriched and saved.\n");
    else                 printf("Save failed.\n");
    company_destroy(c);
    return 0;
}
