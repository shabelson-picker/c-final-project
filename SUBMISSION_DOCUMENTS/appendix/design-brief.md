# Project Mayhem Management - Design Brief
**Version 1.0 | May 2026**

---

## Vision

A company-scale project management tool built in C.
a portfolio system where multiple projects coexist, teams are shared across projects,
and every action is governed by a role-based privilege system loaded from configuration.

The tool sits at the junction of planning, scheduling, resource optimization, and
reporting - serving everyone from the executive reading a portfolio Gantt to the
developer checking their task assignments.

---

## Core Concepts

### Projects
Each project is a directed acyclic graph (DAG) of tasks with:
- PERT time estimates (optimistic / likely / pessimistic)
- Risk weights
- Dependencies enforced at link time (cycle detection)
- Critical path computed via CPM (forward + backward pass)
- Milestones with deadlines

### Company Portfolio
A company owns multiple projects. Team members are company-wide resources,
assignable across projects. Executives see across all projects; PMs see only theirs.

### Role System
Roles are loaded from a config file (`roles.cfg`).
Each role has a set of privilege flags. One company may have "Project Manager" and
"Executive". Another may have "Tech Lead", "Scrum Master", "Stakeholder". The
system adapts to the company's structure.

### Authentication
Lightweight - no passwords. On launch: "Who are you?" - pick from the member list.
The session then runs under that member's role and privileges.
A special **System Admin** role always exists (sudo - all privileges) for initial
project setup. Members can switch active role from the main menu if they hold
multiple roles.

---

## User POVs

### Executive
- Read-only across all projects
- Portfolio Gantt: all projects on one timeline
- Team utilization report: who is overloaded, who is idle
- Risk summary: which projects have high-risk critical paths
- Cannot edit tasks, assignments, or dependencies

### Project Manager
- Full CRUD on their assigned project(s)
- Assign team members to tasks
- Run scheduling algorithms (CPM, risk-weighted)
- View Gantt, DAG, dependency charts
- Export .dot graph
- Cannot access other PMs' projects

### Team Member
- View their own assigned tasks
- Update task status (TODO → IN_PROGRESS → DONE)
- View project schedule (read-only)
- Cannot edit dependencies or assignments

### System Admin
- Sudo: all privileges across all projects
- Create projects, define roles, add members
- Load/save company state
- Assign roles to members

---

## Privilege Flags (bitmask, loaded from roles.cfg)

| Flag | Meaning |
|------|---------|
| `PRIV_VIEW_OWN` | View own tasks |
| `PRIV_VIEW_PROJECT` | View full project |
| `PRIV_VIEW_PORTFOLIO` | View all projects |
| `PRIV_EDIT_TASK` | Create/edit/remove tasks |
| `PRIV_EDIT_DEPS` | Add/remove dependencies |
| `PRIV_ASSIGN` | Assign members to tasks |
| `PRIV_SCHEDULE` | Run scheduling algorithms |
| `PRIV_REPORT` | Access reports and exports |
| `PRIV_ADMIN` | System-level operations |

---

## KPIs (what the tool measures and optimizes)

| KPI | Description |
|-----|-------------|
| **Project duration** | Total days from start to end on critical path |
| **Critical path length** | Number of tasks on the critical path |
| **Resource utilization** | % of available member-days actually assigned |
| **Workload balance** | Std deviation of assigned days across members |
| **Risk exposure** | Sum of risk * PERT_expected across critical path |
| **Slack coverage** | % of tasks with slack > 0 (schedule flexibility) |

---

## Task Assignment Optimization (the highlight feature)

**Problem:** Given a scheduled project (CPM done), assign team members to tasks
to minimize total project duration while balancing workload.

**Constraints:**
- Member must have all required skills for the task
- Member availability (0.0-1.0 fraction of a work day)
- No member double-booked on overlapping tasks (future: parallel assignment)

**Algorithm options (in order of complexity):**
1. **Greedy ranked** - for each task on critical path first, assign best-fit member
   (skill match score + availability). Current implementation, improve ranking.
2. **Backtracking** - exhaustive search with pruning. Optimal for small teams (<10).
3. **Bipartite matching** - Hungarian algorithm. Optimal, O(n³).

**MVP target:** Greedy ranked with skill-match scoring and workload balancing penalty.
**Final target:** Backtracking with pruning, presented as "optimizer" in the menu.

---

## Requirements

### Functional
- [ ] Multi-project portfolio (company scope)
- [ ] Role/privilege system loaded from `roles.cfg`
- [ ] Lightweight session (pick member from list, switch role)
- [ ] System Admin role with sudo (always present)
- [ ] Task CRUD with PERT + risk
- [ ] DAG dependency management with cycle detection
- [ ] CPM scheduling (critical path, slack, forward/backward pass)
- [ ] Risk-weighted scheduling variant
- [ ] Task assignment optimization (greedy → backtracking)
- [ ] Gantt chart (ASCII)
- [ ] DAG view (ASCII)
- [ ] Dependency chart per task (ASCII)
- [ ] Portfolio Gantt (executive view)
- [ ] Team utilization report
- [ ] Save/load (YAML-subset persistence)
- [ ] .dot export for Graphviz

### Technical
- Standard C99, gcc -Wall -Wextra
- Modular: one logical unit per .c/.h pair
- Every function has a short doc comment
- Dynamic memory: DynamicIntArray, growable task/member/project arrays
- No external dependencies

### Phase 2+ Features
- Full authentication with passwords
- Network / multi-user
- GUI
- Real-time collaboration

---

## Delivery Milestones

| Milestone | Target | Scope |
|-----------|--------|-------|
| **Beta** | Tomorrow | Single project, all current features stable, compiles clean |
| **Optimization** | Week 1 | Greedy ranked assignment + workload report |
| **Portfolio** | Week 2 | Multi-project, company struct, role/privilege system |
| **Polish** | Week 3 | Function comments, edge cases, presentation prep, ZIP |

---

## MVP Scope (Beta - Tomorrow)

- Single project, all menus working
- Cycle detection on link
- Dependency chart
- Save/load
- Compiles clean, no crashes on normal use
- Function doc comments added (mandatory per rubric)

---

## Open Questions

1. Role switching in-session: can a member hold multiple roles simultaneously, or switch between them?
2. `roles.cfg` format: key-value pairs, or structured (role → privilege list)?
3. Portfolio Gantt: time-aligned across projects, or stacked separately?
4. Assignment optimizer: interactive (show proposal, user confirms) or automatic?
