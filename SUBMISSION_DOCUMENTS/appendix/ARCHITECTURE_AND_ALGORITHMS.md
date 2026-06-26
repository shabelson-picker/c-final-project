# Architectural & Algorithmic Decisions

A record of *why* the system is built the way it is - the structural choices, the
algorithms, and the trade-offs behind them. See `diagrams/` for the matching data-model,
GUI-flow, and scheduler diagrams.

---

## 1. Module structure (logical .c/.h split)

The code is split by responsibility, not convenience:

| Layer | Modules | Responsibility |
|-------|---------|----------------|
| **Domain** | `task`, `milestone`, `team`, `project`, `company` | The data model + invariants |
| **Containers** | `dynamic_int_array`, `task_ref_array` | Generic growable arrays |
| **Algorithms** | `algorithms` | CPM, assignment, conflict resolution |
| **Rendering** | `renderer`, `dot_export`, `report_exporter`, `reports` | Gantt/DAG, HTML, `.dot`, workload |
| **Persistence** | `persistence` | Directory-bundle save/load |
| **UI** | `menus`, `ui`, `file_browser`, `roles` | Menus, input, colors, role gating |
| **Sandbox** | `sim` | Day-by-day what-if simulation |
| **Entry** | `main` | Startup, company select, role sign-in |

The compile-time dependency direction is one-way (UI -> algorithms -> domain ->
containers); the domain never includes the UI.

---

## 2. Core data structures

- **`Company` owns `TeamMember**` and `Project**`** (heap arrays of pointers, doubling
  growth). Members live on the company, **not** the project - a person can be on many
  projects without duplication.
- **`Project`** owns its `Task**` and `Milestone**`, plus two **sentinel tasks**
  (`START`, `END`, zero duration) so the DAG always has a single source and sink.
- **`Task`** holds three-point PERT, a risk weight, a `required_skills` bitmask, four
  dependency lists (`pre` / `post` / `alt` / `work_pre`), assignment fields, and
  scheduler outputs (`sched_start/end`, `slack`, `is_critical`, `topo_order`).
- **`DynamicIntArray`** - the one growable primitive everything reuses: doubling growth,
  plus `sort_insert` / `find_index` (binary search) for the ordered roster lists.

### Master / apprentice (single source of truth)
The project roster `Project.member_ids` is the **master**. `TeamMember.project_ids` is a
**derived apprentice**, never persisted - it is rebuilt from all projects on load
(`rebuild_member_project_ids`). Same idea for `TeamMember.tasks`: assignment is the truth
(`Task.assignee_id`), and the per-member task view is **derived by scanning** tasks, so
it can never drift out of sync.

### TaskRef
Task IDs are **per-project**, not globally unique, so a cross-project reference is a
`(project_id, task_id)` pair (`TaskRef`).

### Derived id -> pointer index (performance)
The app is **read-heavy**: scheduling and rendering traverse the task graph constantly,
while tasks rarely mutate. `project_find_task` / `company_find_member` were O(n) linear
scans called O(n^2) across topo-sort, the CPM passes, and the resolver. They now consult
a derived `id -> pointer` table (`Project.by_id`, `Company.mem_by_id`), rebuilt lazily
when a `dirty` flag is set on any structural change. It is a **verified shortcut**: a hit
must satisfy `slot->id == requested`, otherwise it falls back to the linear scan - so
behavior is identical, just O(1) on the hot path. (C has no standard hashmap; an index
table is the idiomatic answer.)

---

## 3. Scheduling - Critical Path Method

`schedule_project(strategy)` is textbook CPM over the task DAG:
1. **`topo_sort`** - Kahn's algorithm; in-degree counts both real dependencies (`pre`)
   and resource edges (`work_pre`), so the order is valid for both.
2. **`forward_pass`** - earliest start = `max(predecessor ends, work_pre ends, min_start
   floor)`; earliest end = start + effective duration.
3. **`backward_pass`** - latest start, **slack** = latest - earliest, and **critical
   path** = the zero-slack chain.
4. **`topo_order`** stamped for stable display (the Gantt's DAG column).

### PERT and the three strategies
Each task carries `min / likely / max`. `pert_expected = (min + 4*likely + max) / 6`,
`variance = ((max-min)/6)^2`. `effective_duration(task, strategy)` selects what the
scheduler actually uses:
- **Earliest-deadline** - `pert_expected` (nominal plan).
- **Risk-weighted critical** - `pert_expected * (1 + risk*0.5)` (inflate risky tasks).
- **Pessimistic** - `pert_max` (worst case).

One function, three behaviors - so changing strategy is a single parameter, consistently
threaded through assignment and rescheduling.

---

## 4. Resource assignment - greedy + policies

`assign_members_greedy(company, idx, strategy)`:
- **`find_best_member`** scores qualified members (skills cover `required_skills`) by a
  4-field lexicographic key built from `(free_day, member_load, skill-surplus, id)`. The
  **policy** decides field priority:
  - **Earliest-free** - tightest makespan (default).
  - **Balanced** - spread load (minimize per-member days).
  - **Best-fit** - specialists first, keep generalists free (`surplus = popcount(skills &
    ~required)`).
  The policy is module-level (`assign_set_policy`), so menus need no signature churn.
- When the chosen member is already busy in the current schedule, an **`work_pre`
  resource edge** is added (not a DAG edge - it constrains timing without polluting the
  logical dependency graph), then the project is rescheduled. This repeats as a bounded
  pass loop until everything is assigned or no progress is made.

### Pins
A **manually-assigned** task keeps its owner across reschedules (`clear_work_assignments`
skips it). A **fixed-time** task (vacation) additionally keeps its **window**.

---

## 5. Resource-overlap resolution (fixpoint)

Greedy assignment only catches conflicts visible in the *pre-push* schedule; pushing one
task can shove it into a sibling's window. So `resolve_resource_overlaps` runs a
**fixpoint**:
1. reschedule, 2. group tasks by `(assignee, start)` via `qsort`, 3. a single **linear
neighbour scan** (the sorted-interval property means only adjacent same-member tasks can
overlap), 4. link earlier -> later with a `work_pre` edge, 5. repeat until a pass adds
**zero** edges.

Edges are oriented by current `sched_start`, so the resource graph is **always acyclic**
(an overlap implies no existing path either way). It is **feasibility repair, not
optimization**: deterministic, no objective, converges to one legal overlap-free
schedule. (A slack-aware *optimizer* would introduce an objective and reversible choices,
turning this into local search over an NP-hard problem - explicitly out of scope.)

---

## 6. Cross-project member calendar (the keystone)

The scheduler is per-project, but members are shared. Naively, each project booked a
shared member from day 0, **double-booking them across projects undetected.**
`compute_external_floor` fixes this: schedule fields are project-relative offsets, so a
helper (`date_to_days`, Howard-Hinnant civil day-number) places every project on one
**absolute timeline**. For each member it takes the **max committed end-day across other
projects**, converts back to this project's frame, and seeds both `member_free_day` and a
per-task `min_start` floor (which `forward_pass` clamps to). Result: project P is
serialized after a shared member's existing commitments, deterministically. This is the
single highest-value correctness feature - and it is **coarse by design** (prefix
serialization, not interval gap-filling; the interval-aware version is future work).

---

## 7. Vacations & splitting (overrides)

A **vacation** is the composition of the two existing override flags: `manually_assigned`
(assignee override) + `fixed_time` (time override), with `required_skills = 0` so it is
never picked for real work. `forward_pass` leaves its window in place; the resolver routes
the *flexible* task around it. **Manual split** (`project_split_task`) decomposes a task
into two chained tasks (part 1 keeps predecessors, part 2 takes successors) - reusing the
dependency engine rather than true preemption.

*(Designed, not yet built: auto-split a task **around** a vacation via a lightweight
`Subtask` segment list - schedule-time-only, mergeable, with the scheduler operating on
the flattened leaf segments. This generalizes to time-shared assignees. See `.memory`.)*

---

## 8. Access control (roles & privileges)

`roles.cfg` (a plain text data file, shipped next to the exe via the build's
copy-to-output) defines roles as `(name, privilege list)`. Privileges are a **9-bit
bitmask** (`VIEW_OWN ... ADMIN`); `ADMIN` implies all. At startup `choose_role` sets a
session mask, and every menu action calls `priv_require(PRIV_X)` - authorization at action
time (deny + named message), not menu hiding, which is simple and oral-exam-explainable.

---

## 9. Persistence - directory bundle

Save format is a **human-readable directory bundle**: `company/` -> `company.yaml`,
`team.yaml`, and `projects/<name>/project.yaml`. Chosen over a single binary blob because
it is diff-able, hand-editable, and survives schema drift. **Derived** state (the id
indexes, `project_ids`, `min_start`) is never persisted - it is rebuilt on load, keeping
the format minimal and identity stable.

---

## 10. Why dynamic memory

Nothing is fixed-capacity: companies, projects, tasks, members, dependency lists, and the
id indexes all grow on demand via `malloc`/`realloc` (doubling). The model is inherently
unbounded - a portfolio can have any number of projects, a task any number of
dependencies - so static arrays would either waste memory or impose arbitrary caps.
Ownership is explicit and one-directional (see the data model): each `*_destroy` frees
exactly what its struct owns, and the master/apprentice rule guarantees no double-free of
shared references (apprentices hold IDs/indices, never owning pointers).

---

## 11. UI architecture

A single generic `run_menu(ctx, render, options, handler)` drives every menu via a
function-pointer handler (a C stand-in for the strategy/command pattern), which fixed an
earlier class of "invalid input exits the menu" bugs. Breadcrumb navigation, consistent
input helpers with validation, and EOF-safe loops (every menu/input loop unwinds cleanly
on real `feof` instead of spinning) round it out.

---

## 12. Roadmap - planned extensions (designed, not yet built)

These are the next architectural moves. They are **not implemented** - listed here so the
design intent is on record. The current system stands on its own without them.

### Company-scale unified scheduler (the v2 of §6)
Today the scheduler is per-project and shared members are coupled only by a **coarse**
cross-project floor (prefix serialization). The planned mode schedules **all projects as
one unified project**: every task on a single absolute timeline (anchored at each
project's `start_date`), the member pool as **one global resource set**, and CPM +
greedy assignment + the overlap fixpoint run over the **union**. This makes cross-project
conflict detection **exact and interval-aware** (a member busy in project A blocks them in
B at the same absolute time; gaps become usable) - the single highest-value scheduling
upgrade, subsuming the coarse calendar. Mechanically: resource (`work_pre`) edges now
cross projects, so they key by `TaskRef(project_id, task_id)`; `find_best_member` reads a
member's *global* load; `resolve_resource_overlaps` groups same-member tasks company-wide;
the per-task `min_start` hack disappears. Per-project scheduling stays as an option; a
company-menu "Schedule all" drives the unified pass. (Open: whether to allow cross-project
*dependencies*, and whether project starts are fixed or slidable.)

### Subcontractor / temp worker (extends §4)
A **third staffing option** beyond on-roster assignment and "hire from company". A
subcontractor is a **temporary, external** worker assigned **exclusively to one task** -
brought in to cover a task the roster can't, does only that task, then leaves. Modeled as
a `TeamMember` with an `is_temp` flag + a bound task: `find_best_member` never considers a
temp for any other task, the bound task is pinned to them, and they carry no cross-project
commitments (external -> floor 0). A natural hook for a future **cost/budget** field
(external hire = money). Entry points: a third choice in the `resolve_unassigned` cascade
and a sim "hire subcontractor" event.

### Auto-split around a vacation (the `Subtask` layer, see §7)
True preemption: when a member's task would straddle their vacation, split it at the block
boundary into schedule-time-only **segments** (a lightweight `Subtask` list on the task),
schedule the flattened leaf segments, then merge contiguous ones back. The same
machinery, pointed at a member's *free gaps* instead of a vacation, generalizes to
**time-shared assignees** (interleaving work into the gaps between commitments).
