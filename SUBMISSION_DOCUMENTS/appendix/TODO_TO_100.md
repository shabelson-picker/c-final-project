# Things Left To Do To Get 100

Mapped to the HIT final-assignment rubric (`assignment.md`). The engine is done and
runs clean (build 0 warnings / 0 errors); what remains is mostly **completeness,
packaging, and presentation** - the points that are easy to lose, not hard to earn.

Legend: [x] done   [~] partial   [ ] to do

---

## A. Software project - code (35 pts)

**Doc comments - MANDATORY for *every* function (rubric calls this out explicitly).**
- [~] Core/domain headers documented in VS `/// <summary>` style
  (`dynamic_int_array`, `task_ref_array`, `milestone`, `team`, `task`, `project`,
  `company`, `menus`).
- [ ] Convert the remaining headers to the same `///` style for uniformity:
  `algorithms`, `renderer`, `persistence`, `dot_export`, `report_exporter`, `reports`,
  `ui`, `file_browser`, `roles`, `sim`. **This is the single biggest, cheapest point
  source - do it first.**
- [ ] Spot-check that **no function** anywhere is undocumented (the rubric says *every*).

**Runs correctly / edge cases.**
- [x] Builds 0/0; full menu flow runs.
- [x] EOF-safe input loops (no infinite spin on exhausted input).
- [x] Cycle rejection on dependency links; NULL-guarded graph traversal.
- [ ] **Manual round-trip smoke test** of the new sign-out flow (load company -> sign
  out -> re-sign-in as a different role -> exit) - couldn't be scripted (interactive
  browser); needs one human run.
- [ ] Quick pass for input edge cases in every prompt (negative days, huge numbers,
  empty strings) - most are handled; confirm the few add/edit retry loops.

**Known small defects to close (flagged in `.memory`).**
- [ ] `menu_schedule` "Request vacation" hardcodes `SCHED_EARLIEST_DEADLINE` -> should
  reuse the last-used strategy (strategy isn't remembered; policy already is).
- [ ] Vacation start-day prompt says "project-relative" but the Gantt is now absolute
  company-days -> align the coordinate (or relabel the prompt).
- [ ] Move GUI methods out of `main.c` (`print_banner`/`goodbye` -> `ui`,
  `choose_role`/`print_privs` -> `menus`) so `main` is a thin entry point.

**Logical separation / structs (already satisfied).**
- [x] >> 8 functions; >> 2 structs; clean .c/.h split by responsibility.

---

## B. Program presentation (30 pts) - the slide deck / write-up

The rubric grades a *description*, not just code. Build the deck from the three
summaries + diagrams already in the repo:
- [x] **Diagrams** (redone): data-model UML, GUI flow, scheduler pipeline
  (`diagrams/*.svg|png`, `diagrams.md`).
- [x] **System overview** -> `EXECUTIVE_OVERVIEW.md` (purpose, audience, originality, UX).
- [x] **Architecture & algorithms** -> `ARCHITECTURE_AND_ALGORITHMS.md`
  (structs, CPM/PERT, assignment, cross-project calendar, dynamic memory rationale,
  file usage).
- [ ] **Slides**: condense the above into a presentation. Required beats: purpose +
  audience; originality vs. existing tools; example inputs/outputs (screenshots of the
  Gantt, an HTML report, the workload table); central data structures; central function
  list + params; the main algorithm (CPM) as a flowchart; why dynamic memory; file usage.
- [ ] **Reflection** (rubric item): write up one real challenge + how it was solved.
  Strong candidates already in the history: the **cross-project double-booking** fix, or
  the **resource-overlap fixpoint** (single-pass gap -> fixpoint). One honest paragraph.

---

## C. AI usage declaration (5 pts)
- [ ] Short document listing how AI was used across ideation / planning / development /
  testing, and which tools. The development log (`SOLO_LOG.md`, `ai_log_*`) is the raw
  material - summarize it honestly into a one-page declaration.

---

## D. Project creativity (10 pts)
- [x] Already strong - a genuine CPM scheduler with cross-project resource awareness,
  role-based views, a simulation sandbox, and themed UX. Just **articulate it** in the
  deck (the "originality" section of `EXECUTIVE_OVERVIEW.md`).

---

## E. Packaging & submission (avoid the -10 penalty)
- [ ] **Generate a `.sln` + `.vcxproj` wrapper.** The repo uses VS2022 `.slnx`; HIT
  expects a classic `.sln` in the ZIP. Produce one so the grader can open/build it.
- [ ] **ZIP the SOLUTION folder** with `.h` / `.c` / `.sln` / `.vcxproj`.
- [ ] **Strip build intermediates** before zipping: `x64/`, `.vs/`, `*.obj`, `*.exe`,
  `*.pdb`, `tests/bin/` (the rubric says delete OBJ/EXE; an invalid/too-large ZIP loses
  10 pts).
- [ ] **Verify the ZIP** opens and builds on a clean machine (invalid ZIP = -10).
- [ ] Ensure `roles.cfg` ships (the copy-to-output rule handles the build dir; include
  the source `roles.cfg` in the ZIP too).
- [ ] Submit on time (2 pts/day late, 1 week max).

---

## F. Optional polish (depth, not required for 100)
- [ ] The **auto-split-around-vacation** (`Subtask`) feature - true preemption; a
  standout demo if time allows (design is locked in `.memory`).
- [ ] **Subcontractor / temp worker** - a third staffing option beyond on-roster
  assignment and "hire from company". A *temp, external* worker assigned **exclusively
  to one task**: brought in to cover a task the roster can't, does only that task, then
  leaves. A `TeamMember` with an `is_temp` flag + bound task; `find_best_member` never
  considers a temp for other tasks; the bound task is pinned to them; no cross-project
  commitments (external -> floor 0). Entry points: a third choice in the
  `resolve_unassigned` cascade and a sim "hire subcontractor" event. Natural hook for a
  future **cost/budget** field (external hire = $$). (Design notes in `.memory`.)
- [ ] **Company-scale scheduler strategy** - a mode that plans *all projects as one
  unified project*: every task on a shared absolute timeline, the member pool as one
  global resource set, CPM + assignment + overlap-resolution over the union. Subsumes the
  current coarse cross-project floor with an *exact, interval-aware* calendar (the single
  highest-value scheduling upgrade). Cross-project resource edges keyed by
  `TaskRef(project_id, task_id)`; `find_best_member` sees global load; add a company-menu
  "Schedule all". Big refactor (scheduler moves from `Project*` to company-aware). Design
  notes in `.memory`.
- [ ] Convert demo data churn out of git; keep the ZIP lean.
- [ ] A one-page **user guide** (key bindings / menu map) for the presentation appendix.

---

### Suggested order (highest grade-per-hour first)
1. Finish **doc comments** on the remaining headers (A) - biggest guaranteed points.
2. **Packaging**: `.sln` wrapper + clean ZIP + verify (E) - avoids the -10 cliff.
3. **Slide deck + reflection** from the summaries (B).
4. **AI usage declaration** (C).
5. Close the three **small defects** (A) and run the **manual smoke test**.
6. Only then, optional **subtask** feature (F).
