# Project Mayhem Management

A console-based, multi-project **portfolio scheduling engine** written in pure
C99 (Critical Path Method, PERT, resource leveling, Gantt/HTML reports).
Final project — HIT, *Introduction to Computer Science*, Spring 2026.

---

## Directory structure

```
fin-project/                    ← base: implementation + submission
├── README.md                   ← this file
├── fin-project-vs2022/         ← C source project, toolset v145 (newest Visual Studio)
├── fin-project.sln / .slnx     ← solution for fin-project-vs2022
├── fin-project-vs2019/         ← same project retargeted to VS2019 (toolset v142) — submission build
├── dist/                       ← prebuilt distribution (compiled .exe + data)
├── tests/                      ← throwaway unit-test harnesses (not part of the program)
├── mayhem_demo/  my_companies/ ← sample saved company bundles (the app's data files)
├── x64/                        ← build output (safe to delete)
├── roles.cfg                   ← runtime role / privilege configuration
├── מטלה מסכמת … .pdf           ← assignment brief
└── SUBMISSION_DOCUMENTS/
    ├── PRESENTATION.md / .pdf   ← project presentation (Part B)
    ├── AI_USAGE.md / .pdf       ← AI-usage declaration
    ├── diagrams/               ← Graphviz / Mermaid diagram sources used in the presentation
    └── appendix/               ← supporting documents
        (ARCHITECTURE_AND_ALGORITHMS, EXECUTIVE_OVERVIEW, EXTRAPOLATION,
         SOLO_LOG, TODO_TO_100, assignment, design-brief, diagrams)
```

---

## Implementation (the program)

| Path | What it is |
|------|------------|
| `fin-project-vs2022/` | The C source project (`src/*.c`, `*.h`), toolset **v145** — the newest Visual Studio. Opened via `fin-project.sln` / `.slnx` at the base. |
| `fin-project-vs2019/` | The same project retargeted to **Visual Studio 2019** (toolset `v142`, solution format 16) — use this if opening on an older Visual Studio. Self-contained `.sln` + `.vcxproj` + `src/`. |
| `dist/` | Prebuilt distribution (compiled `.exe` + data files). |
| `tests/` | Throwaway unit-test harnesses written during development (not part of the program). |
| `mayhem_demo/`, `my_companies/` | Sample saved company bundles (the app's data files). |
| `roles.cfg` | Runtime role / privilege configuration, loaded next to the executable. |
| `x64/` | Build output — safe to delete. |

## `SUBMISSION_DOCUMENTS/` — everything for the Moodle submission

| Path | What it is |
|------|------------|
| `PRESENTATION.md` / `PRESENTATION.pdf` | The project presentation (Part B). Markdown is [Marp](https://marp.app); the PDF is the rendered deck. |
| `AI_USAGE.md` / `AI_USAGE.pdf` | The AI-usage declaration (how AI tools were used throughout development). |
| `diagrams/` | Graphviz (`.dot` → `.png`/`.svg`) and Mermaid diagram sources used in the presentation. |
| `appendix/` | Supporting documents — architecture & algorithms, executive overview, design brief, roadmap, development log, and the assignment spec. |

---

## Building

- **Newest Visual Studio:** open `fin-project.sln` (or `fin-project.slnx`) at the base.
- **Visual Studio 2019:** open `fin-project-vs2019/fin-project/fin-project.sln`.

Pick a configuration (`Debug`/`Release`) and platform (`x64`/`Win32`) and build.
Standard C99, MSVC, `/W4`, **0 warnings**. No external dependencies are required
to build or run; Graphviz is optional (only to render the dependency graph
embedded in HTML reports).
