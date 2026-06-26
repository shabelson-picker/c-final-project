# Project Mayhem Management - Executive Overview

## What it is
Project Mayhem Management is a **console-based, multi-project portfolio planner** written in
pure C99. It models a software company as a set of **projects**, each a graph of
**tasks** with dependencies and **PERT** time estimates, staffed from a shared pool of
**team members**. From that model it computes a real schedule - critical path, earliest
start/finish, slack - assigns people to work, detects and resolves resource conflicts
*within and across projects*, and renders the result as Gantt charts, dependency
graphs, and HTML reports.

It is not a to-do list. It is a small but genuine **project-management engine**: the
same class of tool as MS Project or GanttProject, reduced to its scheduling core and
delivered as a fast, dependency-free terminal application.

## Who it is for
- A **Project Manager** planning one project: build the task DAG, estimate with PERT,
  assign people, generate a schedule, export a report.
- An **Executive** overseeing a portfolio: read-only views across all projects -
  portfolio Gantt, company-wide task timeline, resource workload / over-allocation,
  an executive HTML report.
- A **Team Member**: see only their own assigned tasks and schedule.
- An **Admin**: everything, plus team management.

Roles are real: at sign-in you pick a role from `roles.cfg`, and every action is gated
by a **privilege bitmask** (view / edit / assign / schedule / report / admin).

## What it does
- **Plan** - create projects and tasks; link tasks into a dependency DAG with **cycle
  rejection**; group tasks under milestones with deadlines.
- **Estimate** - three-point **PERT** per task (optimistic / likely / pessimistic) plus a
  risk weight, feeding three scheduling strategies (earliest-deadline, risk-weighted,
  pessimistic worst-case).
- **Schedule** - **Critical Path Method**: topological sort, forward/backward pass,
  slack, critical-path detection.
- **Staff** - greedy member assignment by skill match, with three selectable policies
  (earliest-free, balanced workload, best skill-fit).
- **Resolve conflicts** - a fixpoint resolver serializes a member's overlapping tasks;
  a **cross-project member calendar** prevents double-booking a person who is committed
  on another project (a gap most lightweight tools miss).
- **Handle exceptions** - **vacations** as immovable time blocks the scheduler routes
  around; **manual pins** that lock a task's assignee; **task splitting**.
- **Visualize** - console Gantt on an absolute company timeline (with a day ruler and
  project-start marker), dependency graphs, portfolio and all-task Gantts, and milestone
  status (on-track / at-risk / late).
- **Report & persist** - self-contained, modern HTML reports (per-project and executive),
  Graphviz `.dot`/SVG export, and a human-readable **directory-bundle save format**
  (YAML-ish text files per company / team / project).
- **Simulate** - a day-by-day **sandbox** that clones the company, lets you advance time
  and inject events (complete/cancel tasks, hire/quit, re-plan), then apply or discard -
  the real data is never touched until you commit.

## Originality (vs. existing tools)
Commercial planners (Jira, Asana, Monday, MS Project) are criticized for cluttered UIs,
"timeline" views that are not real Gantts, and **scheduling conflicts that surface too
late** because dependencies and resource leveling are weak. This project deliberately
inverts that: it is intentionally minimal (a terminal app, no dependencies), but its
**scheduling core is genuine** - a real CPM Gantt (not a fake timeline), a dependency
engine with cycle rejection, and a cross-project member calendar that catches resource
clashes **at schedule time** rather than after the fact. The novelty is depth over
breadth: a correct, explainable scheduling kernel rather than a feature checklist.

## User experience
A clean, colored terminal UI: breadcrumb navigation, a generic menu runner with
consistent input validation, status feedback after every action, and zebra-striped
tables. Output is ASCII-only for portability. (The product is themed after *Fight Club*
- "Project Mayhem" - with a few intentional, undocumented easter eggs.)

## Technical envelope
- **Language**: standard C99; built with MSVC (Visual Studio 2022), `/W4`, 0 warnings.
- **Dependencies**: none required to build/run. Graphviz is optional, only to render the
  embedded dependency graph inside HTML reports.
- **Footprint**: ~20 translation units, fully dynamic memory (no fixed caps on
  companies / projects / tasks / members), a hand-rolled growable `DynamicIntArray`, and
  derived `id -> pointer` index tables for O(1) lookup on the read-heavy scheduling paths.

## Roadmap (planned, not yet built)
The scheduling core is designed to grow along one coherent arc:
- **Company-scale unified scheduler** - plan all projects as a single combined project on
  one shared timeline with the staff pool as one global resource set, turning today's
  *coarse* cross-project resource awareness into an *exact, interval-aware* one.
- **Subcontractor / temp worker** - bring in a temporary external worker assigned
  exclusively to a single task (a hook for future cost/budget tracking).
- **Auto-split around vacations** - true preemption: a task that hits a worker's vacation
  splits around it and resumes after, generalizing to time-shared assignees.

(These are recorded as design intent; see `ARCHITECTURE_AND_ALGORITHMS.md` §12 and
`TODO_TO_100.md`.)
