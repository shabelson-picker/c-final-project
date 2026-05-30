# Project Mayhem Management - Diagrams

All three diagrams are **Mermaid**. To use in draw.io:
**+ (Insert) -> Advanced -> Mermaid...**, paste a block.
(Or render/tweak at https://mermaid.live and export SVG/PNG.)

---

## 1. Data-structure UML (class diagram)

`*--` = owns (composition, frees on destroy);  `o--` = references (no ownership).

```mermaid
classDiagram
    class Company {
        +char name[]
        +char save_dir[]
        +TeamMember** members
        +int member_count / capacity
        +int next_member_id
        +Project** projects
        +int project_count / capacity
    }
    class Project {
        +char name[]
        +char save_dir[]
        +Date start_date
        +Task* start_node
        +Task* end_node
        +Task** tasks
        +Milestone** milestones
        +DynamicIntArray member_ids
        +int next_task_id / next_milestone_id
    }
    class Task {
        +int id
        +char title[]
        +TaskStatus status
        +float pert_min/likely/max/expected/variance
        +float risk
        +uint32 required_skills
        +DynamicIntArray pre_ids
        +DynamicIntArray post_ids
        +DynamicIntArray alt_ids
        +DynamicIntArray work_pre_ids
        +int assignee_id
        +int manually_assigned
        +int milestone_id
        +int sched_start/sched_end
        +int latest_start/slack
        +int is_critical/topo_order
    }
    class Milestone {
        +int id
        +char name[]
        +int deadline_day
        +int priority
        +int* task_ids
        +int task_count
    }
    class TeamMember {
        +int id
        +char name[]
        +char role[]
        +uint32 skills
        +float availability
        +DynamicIntArray project_ids
        +TaskRefArray tasks
    }
    class DynamicIntArray {
        +int* data
        +int count
        +int capacity
    }
    class TaskRefArray {
        +TaskRef* data
        +int count
        +int capacity
    }
    class TaskRef {
        +int project_id
        +int task_id
    }
    class Date {
        +int year
        +int month
        +int day
    }
    class TaskStatus {
        <<enum>>
        TODO
        IN_PROGRESS
        DONE
        CANCELLED
        BLOCKED
    }
    class Skill {
        <<enum bitmask>>
        FRONTEND BACKEND HARDWARE EMBEDDED
        QA DEVOPS DESIGN PM
    }

    Company "1" *-- "many" TeamMember : owns
    Company "1" *-- "many" Project : owns
    Project "1" *-- "many" Task : owns
    Project "1" *-- "many" Milestone : owns
    Project "1" *-- "2" Task : START/END sentinels
    Project "1" o-- "1" DynamicIntArray : member_ids (IDs)
    Task "1" *-- "4" DynamicIntArray : pre/post/alt/work_pre
    Task ..> TaskStatus
    Task ..> Skill : required_skills
    Milestone "1" o-- "many" Task : task_ids (IDs)
    TeamMember "1" o-- "1" DynamicIntArray : project_ids (derived)
    TeamMember "1" *-- "1" TaskRefArray : tasks
    TaskRefArray "1" *-- "many" TaskRef
    Project ..> Date
```

---

## 2. GUI flow chart

```mermaid
flowchart TD
    START([main]) --> M0{New / Load / Exit}
    M0 -->|New| NEW[company_new_interactive\nname + dir_browser]
    M0 -->|Load| LOAD[company_load_interactive\ndir_browser]
    M0 -->|Exit| BYE([goodbye + quit])
    NEW --> CO
    LOAD --> CO

    CO{{menu_company\nProjects / Team / Save / Exit}}
    CO -->|Save| CO
    CO -->|1999| EGG[[Project Mayhem rules]]
    CO --> PROJ{{menu_company_projects\nNew / Open}}
    CO --> TEAM{{menu_company_team\nAdd / Remove}}

    PROJ -->|New| CREATE[create_project]
    PROJ -->|Open| OPEN

    OPEN{{open_project\nTasks/Deps/Milestones/Schedule/Members}}
    OPEN --> TASKS{{menu_tasks\nList/Add/Remove/Edit/Status/Link/Deps/Assign}}
    OPEN --> DEPS{{menu_deps\nlink + view}}
    OPEN --> MILE{{menu_milestones\nList/Add}}
    OPEN --> SCH
    OPEN --> ROST[[roster checklist\nrun_checklist]]

    TASKS -->|Assign| PIN[manual_assign\npin member -> task]
    TASKS -->|Deps| DEPS

    SCH{{menu_schedule\nGenerate/Report/Gantt/DAG/Export .dot/Export report}}
    SCH -->|Generate| GEN[[scheduler pipeline\nsee diagram 3]]
    SCH -->|Export report| RPT[export_report_html\nHTML + inline SVG DAG]
```

---

## 3. Scheduler / assignment / Gantt pipeline

```mermaid
flowchart TD
    A[menu_schedule: Generate] --> B{pick strategy}
    B --> C[assign_members_greedy c, idx, strategy]

    subgraph ASSIGN [assign_members_greedy]
        C --> C1[clear_work_assignments\nkeep manually_assigned pins]
        C1 --> C2[schedule_project strategy]
        C2 --> C3{pass loop\nup to MAX_ASSIGN_PASSES}
        C3 --> C4[sort tasks by sched_start]
        C4 --> C5[assignment_pass:\nfor each task find_best_member\nadd work_pre on member conflict]
        C5 --> C6{all assigned?}
        C6 -->|no, progress made| C7[schedule_project strategy] --> C3
        C6 -->|yes / no progress| C8[resolve_resource_overlaps]
    end

    subgraph SP [schedule_project]
        S1[topo_sort\nKahn, pre + work_pre] --> S2[forward_pass\nsched_start = max pre/work_pre ends]
        S2 --> S3[backward_pass\nlatest_start, slack, is_critical]
        S3 --> S4[stamp topo_order]
    end
    C2 -.calls.-> SP

    subgraph RES [resolve_resource_overlaps - fixpoint]
        R1[schedule_project] --> R2[group tasks by assignee, sort by start]
        R2 --> R3{adjacent same-member overlap?}
        R3 -->|yes| R4[add work_pre earlier->later] --> R1
        R3 -->|no edges added| R5[done - no overlaps]
    end
    C8 -.calls.-> RES

    C8 --> D[schedule_project strategy]
    D --> E[render_gantt\nrows sorted by start, DAG-order column]
    E --> F[resolve_unassigned\ncascade: suggest off-roster member]
    F --> G[company_save]
```
