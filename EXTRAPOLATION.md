# Project Mayhem Management - Extrapolation

Written at the end of the `solo-ran` autonomous run, after roadmap items 0-7 were
implemented and tested. This is an honest forward-look: where the product goes
next, grounded in limitations actually observed in the code (not a wishlist).

## Where the system stands
A single-company, multi-project planner with CPM scheduling (topo + forward/backward
pass, PERT, critical path), greedy member assignment with three policies, a
cross-project member calendar, immovable vacation blocks, manual task splitting,
role-based privilege gating, per-project and portfolio Gantts, HTML/.dot export, and
an id-indexed lookup. ~6 focused test harnesses cover the scheduling invariants.

## Known limitations (the seams to pull next)

1. **The cross-project calendar is coarse (prefix serialization).**
   `compute_external_floor` sets a member's availability to the *max* committed
   end-day across other projects. A member who is free in a *gap* between two
   other-project tasks cannot use that gap. **Next:** an interval-aware calendar -
   collect each member's busy `[start,end)` intervals in absolute days across all
   projects, then place each task in the earliest free slot that fits. This is the
   natural v2 of the keystone and the single highest-value improvement.

2. **Scheduling is per-project; there is no global resource scheduler.**
   `resolve_resource_overlaps` only serializes a member's tasks *within one project*.
   Across projects, conflicts are handled only by the static floor computed once at
   assignment time. **Next:** a company-level scheduler that treats all members as one
   shared resource pool and resolves overlaps globally (this subsumes #1).

3. **No true preemption.** Splitting is manual and dependency-based. **Next:**
   auto-split a task that would straddle a vacation (split at the block boundary and
   chain the halves) - the machinery already exists (`project_split_task` + fixed_time).

4. **`VIEW_OWN` is defined but not realized as a view.** The privilege exists in
   roles.cfg and the bitmask, but there is no "my tasks only" filtered screen for a
   Developer. **Next:** a per-member task view gated by `VIEW_OWN` (low risk, completes
   the role feature). *(Implemented as the first post-roadmap item - see SOLO_LOG.)*

5. **One global assignment policy.** `g_assign_policy` is module-level. **Next:**
   per-project (or per-task) policy, persisted with the project.

6. **Milestone list can double-count after a split.** `project_split_task` moves
   `milestone_id` to part 2 but does not remove part 1 from the milestone's internal
   task-id list. **Next:** a `milestone_remove_task` + call it on split/remove.

7. **`min_start` is transient.** Re-running `schedule_project` alone (without a fresh
   assignment pass) drops the cross-project floor. **Next:** either persist the floor
   or always recompute it inside `schedule_project` from the company context.

8. **No role-management UI.** roles.cfg lists an `ADMIN` "manage roles" privilege but
   there is no editor; roles are read-only at startup. **Next:** an admin screen to
   add/edit roles and write roles.cfg back.

## Architecture evolution
- Promote the throwaway `tests/` harnesses into a committed unit-test suite with a
  single runner (`run_all.ps1`) and wire it into a pre-commit/CI check.
- The id index uses a verified-slot guard today; a generation counter on the
  container would let it skip the per-lookup id verification.
- A per-role "view model" layer would make `VIEW_OWN`/`VIEW_PROJECT`/`VIEW_PORTFOLIO`
  first-class (what each role *sees*), rather than gating actions after the fact.
- Tie-in to the parent C++ course: a thread-safe `Company` (one writer, many readers)
  is the natural bridge to the multithreading unit.

## What real PM platforms offer, and where they are criticized (survey)
A scan of Jira, Asana, Monday, MS Project, GanttProject, Wrike (May 2026):

**Features they offer:** Gantt with real dependencies (FS/SS/FF/SF + lag/lead),
critical path, baselines (planned vs actual / slippage), resource management and
leveling (utilization, overallocation), % complete and time tracking, milestones,
dashboards/reporting/KPIs, automations, deep integrations (Git, CI, Slack), multiple
views (board/list/timeline/calendar), and roles/permissions.

**Common critiques:** overwhelming complexity and cluttered UIs ("too many features,
constant setting-tweaking"); "timeline" views that aren't real Gantts; **limited
dependency support and manual timeline recalculation - hidden scheduling conflicts
surface too late**; weak resource management and reporting in lighter tools;
fragmentation across integrated tools; paywalled essentials (Gantt/automation).

**How this project already answers them:** a genuine CPM Gantt (not a fake timeline),
a dependency engine with cycle rejection, and - notably - a cross-project member
calendar that catches resource conflicts *at schedule time* rather than letting them
"surface too late." It is intentionally simple (a console app), sidestepping the
complexity critique.

**Therefore the highest-value, lowest-risk gaps to close here are reporting +
resource management** (the weak spot of lighter tools), then richer dependencies:
- *(Implemented this session)* **Resource workload / over-allocation report** - per
  member, across all projects: task count, committed days, active span, and an
  OVERALLOCATED flag when assigned work overlaps in absolute time. Directly answers
  the "conflicts surface too late" + "weak resource management/reporting" critiques.
- **Dependency types + lag** (FS/SS/FF/SF, +/- lag) - the most-cited scheduling gap.
- **Baseline vs actual** - snapshot a schedule, report slippage.
- **% complete / actual dates** - the `status` enum exists; add progress + a
  planned-vs-actual report.

## If this were a product
Multi-company tenancy, an undo/audit log, resource leveling (smooth peaks, not just
resolve clashes), cost/budget tracking alongside time, and a real calendar (weekends,
holidays, partial `availability` - the field already exists on TeamMember, unused).
