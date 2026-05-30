# solo-ran — Log & Handoff

## Resume (read these first on a fresh/compacted session)
1. `c-course/.memory/MEMORY.md` — full project state + roadmap.
2. `c-course/.claude/skills/solo-ran/SKILL.md` — the contract (rules, grading, routing B).
3. `~/.claude/plans/cozy-wishing-pancake.md` — the execution plan.
This file = running decision log; append timestamped entries as work proceeds.

## Status: SETUP DONE — run NOT yet started (waiting on token-budget reset)
- Auto-accept: `.claude/settings.local.json` -> `permissions.defaultMode: "bypassPermissions"` APPLIED (allow-list preserved). May need one bypass-dialog accept.
- Routing: **B confirmed** — Opus main thread; delegate LIGHT batches to Sonnet subagents (Agent tool `model:"sonnet"`); escalate to Opus if a subagent is stuck.
- Build verified working: `vswhere -> VC\Auxiliary\Build\vcvars64.bat -> msbuild fin-project\fin-project\fin-project.vcxproj /p:Configuration=Debug /p:Platform=x64`. Produces `x64\Debug\fin-project.exe`, 0 errors.

## NOT yet done (do at start of next window)
- Git: `git checkout -b claude-branch` (repo root = `cpp oop`, UNBORN: 0 commits, no branches). Stage ONLY `fin-project/` paths. Baseline commit. NEVER push.
- `fin-project/tests/` scaffold (throwaway bundles + stdin scripts).

## Work order
0. Warnings (L): C4244 cast in dynamic_int_array.c:45 `(int)(fnd_ptr - a->data)`; C4090 const in project.c (project_would_create_cycle/project_find_cyclic_paths traversal); drop empty scheduler.c/.h from vcxproj (C4206).
1. (H) Cross-project member calendar — assignment must see a member's commitments across ALL projects (fix double-booking; demo: SHA_inc Apollo/Borealis/Cobalt, Carol id5). v1: seed member_free_day from member's max committed end-day in other projects. Files: algorithms.c (assign_members_greedy/find_best_member/assignment_pass).
2. (H) Scheduling/assignment policies (skill-match score, workload balance).
3. (H) Vacations = Task fixed_time flag + member-pinned time-locked block.
4. (M) Portfolio Gantt (projects-as-bars, company timeline).
5. (H) Role-based views (Exec/PM/TeamMember) + roles.cfg privileges.
6. (H) Task splitting (T1->T1a+T1b via dependency engine; no real preemption).
7. (L/H) Internals: id->pointer index for project_find_task/company_find_member.
8. If done + tested: write extrapolation.

## Verify each step
msbuild to 0 errors + clear new warnings; behavior via piped stdin to fin-project.exe against THROWAWAY bundle in tests/ (never mutate my_companies/, my_projects/, mayhem_demo/). Then COMMIT (feature/bugfix message) + log entry here.

## Guardrails
No push; stay in fin-project/; no edits to .memory/CLAUDE.md/master course; ASCII-only. Stop at 07:00 ISR or interruption — always land on a clean commit.

---
## Decisions log
- 2026-05-31 ~01:45 ISR: Setup complete. Routing B chosen (delegate light to Sonnet). Run deferred to next budget window. Fresh chat recommended (cheaper than carrying transcript).
- 2026-05-31 ~01:56 ISR: RUN STARTED. Budget ~5h to 07:00 ISR. Discovered the real git repo is the INNER one at `c-course/fin-project` (already on `claude-branch`, history present, remote `c-final-project` -> NEVER push). Outer `cpp oop` repo is unborn; left untouched. All commits happen in the inner repo.
- 2026-05-31 ~02:05 ISR: Item 0 (warnings) DONE. Baseline rebuild = 5 warnings confirmed. Fixes:
  * C4244 (dynamic_int_array.c:45): cast `(int)(fnd_ptr - a->data)` (ptrdiff_t -> int).
  * C4090 (project.c:208/235/241): root cause = `project_find_task`/`project_find_milestone` took `Project*` but read-only callers (`project_find_cyclic_paths`, cycle-print) hold `const Project*`. Fix = make both finders take `const Project*` and still return `Task*`/`Milestone*` -- the idiomatic C "search function" pattern (cf. `strchr` returning `char*` from `const char*`). Adding const at non-const call sites is always legal, so no caller breaks. Alternative rejected: separate `_const` overloads (C has no overloading -> API bloat).
  * C4206 (scheduler.c empty TU): scheduler.c/.h were dead stubs ("superseded by algorithms"), not `#include`d anywhere. Dropped both from the .vcxproj and deleted the files.
  Rebuild result: 0 warnings, 0 errors. Next: item 1 (cross-project member calendar).
- 2026-05-31 ~02:40 ISR: Item 1 (cross-project member calendar) DONE + TESTED.
  * Root cause: `assignment_pass` did `memset(member_free_day,0,...)` -> every project booked a shared member from day 0, blind to other projects (Carol/Apollo-Borealis-Cobalt double-booking).
  * Calendar math: schedule fields are project-relative offsets, so I added `date_to_days` (days-from-civil serial day number) + `date_is_valid` to project.h/.c to place offsets on one absolute timeline.
  * Push mechanism: added transient `Task.min_start` floor; `forward_pass` clamps `sched_start >= min_start`. Cross-project has no shared task to hang a work_pre edge on, so the floor is how a task actually gets pushed. (Vacations/item 3 will reuse min_start.)
  * `compute_external_floor(c, project_idx, floor[])`: for each member, max over OTHER projects' assigned tasks of (other.start_abs + task.sched_end) - self.start_abs, clamped >=0. Seeds member_free_day and stamps t->min_start on assignment (both greedy and pinned branches). Backward compatible: invalid/missing start_date -> floor 0 (old behavior); single-project unaffected.
  * Design choice: min_start is TRANSIENT (recomputed each assign run, not persisted) -> no save-format change. Caveat: re-running schedule_project alone (without re-assign) would drop the floor; acceptable, the schedule-only path doesn't reassign.
  * Policy: scheduling project P treats already-committed projects as immovable and serializes P after them (greedy, deterministic). find_best_member naturally prefers a less-committed qualified member (lower free day) -> also load-balances across projects.
  * Verification: tests/test_calendar.c (pure in-memory, no disk) via tests/run_test.ps1 (compiles all src/*.c minus main.c + harness). One backend specialist across 3 projects: A abs 0..5, B serialized to abs 5..11, C (start +3 days) accumulates A+B -> abs 11, C-frame day 8..12. All 9 asserts PASS. Build 0/0.
  Next: item 2 (scheduling/assignment policies).
- 2026-05-31 ~03:05 ISR: Item 2 (assignment policies) DONE + TESTED.
  * Added AssignPolicy {EARLIEST_FREE, BALANCED, BEST_FIT} + module-level g_assign_policy with assign_set_policy/get (algorithms.h/.c). Module-level (not threaded) so menus need no signature churn across resolve_unassigned + 2 call sites.
  * One comparator, three orderings: 4-int lexicographic key (lower=better) built by score_key() from (free_day, member_load, skill-surplus, member-id). EARLIEST_FREE=free first (tight makespan, default/unchanged behavior); BALANCED=load first (spread); BEST_FIT=surplus first (specialists first, keep generalists free). surplus = popcount32(skills & ~required).
  * Threaded member_load[] (tasks assigned this pass) through assignment_pass alongside member_free_day; incremented on both greedy and pinned-sync assigns. find_best_member now takes (member_load, policy).
  * Menu: schedule Generate prompts policy after strategy; invalid -> EARLIEST_FREE.
  * Verified tests/test_policy.c: specialist + generalist, 2 independent backend tasks. EARLIEST_FREE & BALANCED spread one each (makespan 5); BEST_FIT stacks both on the specialist (makespan 10), generalist idle. 9/9 pass. test_calendar still green (no regression). Build 0/0.
  Next: item 3 (vacations as time-locked member blocks).
- 2026-05-31 ~03:35 ISR: Item 3 (vacations / immovable blocks) DONE + TESTED.
  * Model (per plan): a vacation is a real Task with fixed_time=1 + manually_assigned=1, assignee=the member, required_skills=0, window [sched_start, sched_start+len]. Reuses the existing machinery instead of a parallel data path.
  * Task.fixed_time field added + persisted (persistence.c save/load).
  * forward_pass: `if (t->fixed_time) continue;` -> the window never moves.
  * resolve_resource_overlaps: when a same-member overlap involves a fixed block, the FLEXIBLE task is the waiter (pushed after the block) instead of the default earlier->later; both-fixed pairs are skipped (no infinite loop). No splitting -> a straddling task moves wholesale after the block (splitting is item 6).
  * Creation: project_add_fixed_block(p, label, assignee, start, len) (project.c) + schedule menu option 7 "Request vacation" (pick roster member, start day, length -> reschedule + save).
  * Cross-project bonus: because a vacation is an assigned task with a sched_end, compute_external_floor already propagates it to OTHER projects (member unavailable until vacation end there too) - consistent with item 1's coarseness.
  * Verified tests/test_vacation.c: 10-day task straddling a day3..7 vacation reroutes to start day 7 (ends 17), block stays 3..7; a 2-day task (fresh company) fits before the block and stays at 0. 10/10 pass. Notably scenario 2 first FAILED on the same member because the item-1 calendar correctly kept Carol booked from project 1 until day 17 - confirms the two features compose; isolated to a fresh company. calendar+policy still green.
  Next: item 4 (portfolio/company Gantt).
- 2026-05-31 ~03:55 ISR: Item 4 (portfolio Gantt) DONE + TESTED. *** First routing-B delegation: implemented by a Sonnet subagent, verified by me (Opus). ***
  * render_portfolio_gantt(c, width) (renderer.h/.c): one bar per project on a shared ABSOLUTE-date timeline. Per project span = [date_to_days(start_date), +makespan] (makespan = max sched_end). Timeline bounds t0/t1 across datable projects; bars mapped col = (abs - t0)*width/span. Projects with invalid start_date listed as "(no start date)", no bar. Reuses item-1's date_to_days.
  * Hooked into company menu as option 4 "Portfolio" (menus.c).
  * Delegation: gave Sonnet a precise spec (signature, algorithm, ANSI macros, hook point, build cmd, ASCII/-W4/no-git constraints). It returned a clean /W4 build matching the style. I reviewed the body (correct; makespan computed twice - harmless) and re-verified independently.
  * Verified tests/test_portfolio.c (visual, width 60): Apollo Jan1/10d -> cols 0..17; Borealis Jan15/20d -> cols 24..59 (shifted right + longer); Cobalt -> "(no start date)"; header "34 days span" (Jan1->Feb4) all arithmetically correct. Build 0/0.
  Next: item 5 (role-based views + privileges).
- 2026-05-31 ~04:25 ISR: Item 5 (role-based views + privileges) DONE + TESTED.
  * New roles.h/.c module (added to vcxproj). Privilege bitmask enum matching roles.cfg vocabulary (VIEW_OWN/VIEW_PROJECT/VIEW_PORTFOLIO/EDIT_TASK/EDIT_DEPS/ASSIGN/SCHEDULE/REPORT/ADMIN); PRIV_ALL=0x1FF. ADMIN implies all (priv_has special-case).
  * roles_load() parses roles.cfg (the existing data file - left untouched incl. its em-dashes, it is not compiled C so no C4819): trims, skips #/blank, "role:"/"privileges:" pairs, comma-split tokens -> mask. priv_from_token/priv_name tables. Session state: roles_set_current(name, mask) + priv_has/priv_require (denial message names the missing privilege).
  * main.c choose_role() at startup: finds roles.cfg in cwd then exe dir (get_exe_dir), lists "System Admin (all)" + parsed roles, sets session mask. Falls back to admin-only if cfg missing.
  * menus.c gated by priv_require at action points: company {Projects=VIEW_PROJECT, Team=ADMIN, Portfolio=VIEW_PORTFOLIO}; projects {New=ADMIN, Open=VIEW_PROJECT}; tasks {Add/Remove/Edit/Status=EDIT_TASK, Link=EDIT_DEPS, Assign=ASSIGN}; project Members=ASSIGN; schedule {Generate=SCHEDULE, Report/Gantt/DAG/dot/html=REPORT, Vacation=ASSIGN}. Authorization at action time (deny + message), not menu hiding - simple and oral-exam explainable.
  * Verified: tests/test_roles.c (14 asserts) parses real roles.cfg -> 5 roles, exact masks (Executive 0x84, PM 0xfa, Team Lead 0x7a, Developer 0x3), priv_has semantics incl. ADMIN-implies-all. E2E smoke: ran the exe, startup lists roles and "Signed in as System Admin". roles.cfg copied next to exe (x64/Debug, gitignored) for runtime. All 4 suites green. Build 0/0.
  Next: item 6 (task splitting).
- 2026-05-31 ~04:45 ISR: Item 6 (task splitting) DONE + TESTED.
  * project_split_task(p, id, first_fraction) (project.c): turns task T into "part 1" (PERT * f) and creates "part 2" (PERT * (1-f)), rechained pre -> part1 -> part2 -> (T's old successors). Built on project_link_tasks (reuses its sentinel-detach + cycle rejection). Copies assignee/manually_assigned/skills/risk/status to part 2; moves the milestone (completion marker) to part 2. Clamps f to [0.1, 0.9]; refuses fixed_time blocks (vacations). No real preemption - it is a dependency-engine split.
  * Snapshots T's real successors before rewiring (skips END), then unlinks T from each and links part2->successor; finally part1->part2.
  * Menu: tasks menu option 9 "Split" (gated EDIT_TASK) - pick task + part-1 percent.
  * Verified tests/test_split.c (14 asserts): chain X(2)->A(10)->Y(3), split A 60/40 -> part1=6, part2=4, rewired X->A1->B->Y (Y now depends on B not A1), CPM schedule X0..2 A1 2..8 B 8..12 Y 12..15, makespan preserved (15). All 5 suites green. Build 0/0.
  Next: item 7 (internals/perf: id->pointer index).
- 2026-05-31 ~05:05 ISR: Item 7 (id->pointer index, perf) DONE + TESTED. *** ROADMAP COMPLETE ***
  * Replaced the O(n) linear scans in project_find_task / company_find_member (called O(n^2) across topo_sort, forward/backward pass, resolve, render) with a derived id->pointer direct-index array, rebuilt lazily when a dirty flag is set.
  * Safety: all task/member creation funnels through project_add_task / company_add_member (verified - persistence load uses them too), so dirty is set on every structural change incl. load. Belt-and-suspenders: the index only ever provides a VERIFIED shortcut (slot->id must equal requested id); every miss falls through to the original linear scan, so behavior is identical to pure linear scan, just O(1) on the hit path. OOM during rebuild -> index stays NULL -> linear fallback. project_find_task casts away const for the cache only (logical const).
  * Fields added to Project {by_id,cap,dirty} and Company {mem_by_id,...}; freed in destroy; dirty set in create/add/remove.
  * Verified tests/test_index.c (16 asserts): find after add, after swap-remove, after split, absent-id -> NULL, sentinels, members add/remove. Crucially ALL 6 suites still green - the scheduler hammers find_task, so behavior-preservation is proven. Build 0/0.
  ROADMAP ITEMS 0-7 ALL COMPLETE + TESTED. Next: write the extrapolation (done condition).
- 2026-05-31 ~05:15 ISR: Done condition met -> wrote EXTRAPOLATION.md (forward-look grounded in observed limitations). Then, per "reach as far as you can", started on the low-risk completeness items it flagged (deliberately NOT the risky calendar-v2 scheduler refactor near the time boundary - documented as deferred).
- 2026-05-31 ~05:30 ISR: Post-roadmap items DONE + TESTED.
  * #6 milestone-on-split fix: added milestone_remove_task (milestone.c/.h); project_split_task now detaches part 1 from the milestone before attaching part 2. Verified in test_split.c (milestone lists part 2, not part 1).
  * #4 VIEW_OWN realized: company menu option 5 "My tasks" (gated PRIV_VIEW_OWN) lists a member's assignments across ALL projects with schedule + critical flag - turns the defined-but-unused privilege into an actual view. (UI; build-verified + priv_require unit-tested.)
  All 6 suites green; build 0/0.
- 2026-05-31 ~05:50 ISR: User-directed additions (interactive session) DONE + TESTED.
  * Selectable role menu: choose_role() (main.c) now lists each role WITH its privilege summary (print_privs) and LOOPS until a valid selection (re-prompts on bad input; empty/EOF -> System Admin) instead of silently defaulting.
  * Role in breadcrumb: crumb_draw() (menus.c) shows "[role: <name>]" next to the title bar on every screen (roles_current_name()).
  * Unified company task Gantt: render_company_gantt (renderer.c/.h) - one row per task across ALL projects on a shared absolute-date timeline, each bar colored by its project (6-color cycle) + a legend. Company menu option 6 "All-task Gantt" (gated VIEW_PORTFOLIO). Verified tests/test_company_gantt.c (visual): 4 tasks/2 projects, Apollo cyan cols 0..26, Borealis (+9d) green cols 30..49, 15-day span, legend correct.
  * (Build gotcha: a lingering exe from a smoke run locked fin-project.exe -> LNK1104; killed the process and rebuilt clean.)
  All 6 assert suites still green; build 0/0.
