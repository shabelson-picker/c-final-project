# Project Mayhem Management - Diagrams

All three diagrams are **Mermaid**. To use in draw.io:
**+ (Insert) -> Advanced -> Mermaid...**, paste a block.
(Or render/tweak at https://mermaid.live and export SVG/PNG.)
The matching Graphviz sources + rendered SVG/PNG live in `diagrams/` (`dot -Tsvg/-Tpng`).

---

## 1. Data-structure UML (class diagram)

`*--` = owns (composition, frees on destroy);  `o--` = references / derived (no ownership).

```mermaid
classDiagram
    class Company {
        +char name[]
        +char save_dir[]
        +TeamMember** members  (owns)
        +Project** projects  (owns)
        +int next_member_id
        +TeamMember** mem_by_id  (derived id index)
        +int mem_by_id_dirty
    }
    class Project {
        +char name[]
        +char save_dir[]
        +Date start_date
        +Task* start_node / end_node  (sentinels)
        +Task** tasks  (owns)
        +Milestone** milestones  (owns)
        +DynamicIntArray member_ids  (roster, IDs)
        +int next_task_id / next_milestone_id
        +Task** by_id  (derived id index)
        +int by_id_dirty
    }
    class Task {
        +int id
        +char title[] / description[]
        +TaskStatus status
        +float pert_min/likely/max/expected/variance
        +float risk
        +uint32 required_skills
        +DynamicIntArray pre_ids / post_ids / alt_ids / work_pre_ids
        +int assignee_id / manually_assigned
        +int fixed_time  (vacation / immovable)
        +int milestone_id
        +int sched_start/end, latest_start, slack
        +int is_critical, topo_order, min_start
    }
    class Milestone {
        +int id
        +char name[]
        +int deadline_day / priority
        +int* task_ids / int task_count
    }
    class TeamMember {
        +int id
        +char name[]
        +char role[]  (-- roles.cfg)
        +uint32 skills
        +float availability
        +DynamicIntArray project_ids  (derived)
        +TaskRefArray tasks  (derived)
    }
    class DynamicIntArray {
        +int* data
        +int count / capacity  (doubling)
        +sort_insert / find_index (bsearch)
    }
    class TaskRefArray { +TaskRef* data; +int count/capacity }
    class TaskRef { +int project_id; +int task_id }
    class Date { +int year/month/day }
    class Role {
        +char name[]
        +uint32 privs  (Privilege bitmask)
    }
    class Privilege {
        <<enum bitmask>>
        VIEW_OWN VIEW_PROJECT VIEW_PORTFOLIO
        EDIT_TASK EDIT_DEPS ASSIGN
        SCHEDULE REPORT ADMIN(=all)
    }
    class TaskStatus {
        <<enum>>
        TODO IN_PROGRESS DONE CANCELLED BLOCKED
    }
    class Skill {
        <<enum bitmask>>
        FRONTEND BACKEND HARDWARE EMBEDDED QA DEVOPS DESIGN PM
    }

    Company "1" *-- "many" TeamMember : owns
    Company "1" *-- "many" Project : owns
    Company "1" o-- "many" TeamMember : mem_by_id (O(1))
    Project "1" *-- "many" Task : owns
    Project "1" *-- "2" Task : START/END sentinels
    Project "1" *-- "many" Milestone : owns
    Project "1" o-- "many" Task : by_id (O(1))
    Project "1" o-- "1" DynamicIntArray : member_ids (IDs)
    Task "1" *-- "4" DynamicIntArray : pre/post/alt/work_pre
    Task ..> TaskStatus
    Task ..> Skill : required_skills
    Task o-- "1" TeamMember : assignee_id
    Milestone "1" o-- "many" Task : task_ids (IDs)
    TeamMember "1" o-- "1" DynamicIntArray : project_ids (derived)
    TeamMember "1" *-- "1" TaskRefArray : tasks
    TeamMember ..> Skill : skills
    TeamMember ..> Role : role name
    TaskRefArray "1" *-- "many" TaskRef
    Role "1" o-- "many" Privilege : privs
    Project ..> Date
```

---

## 2. GUI flow chart (privilege-gated)

Startup picks the **company first**, then signs in as a **role**; "Sign out" returns to
role sign-in with the company still loaded. Every action is gated by `priv_require`.

```mermaid
flowchart TD
    START([main]) --> M0{"[1] New / [2] Load / [0] Exit"}
    M0 -->|New| NEW[company_new_interactive]
    M0 -->|Load| LOAD[company_load_interactive]
    M0 -->|Exit| BYE([goodbye + quit])
    NEW --> ROLE
    LOAD --> ROLE

    ROLE[choose_role - sign in roles.cfg<br/>sets session privilege mask<br/>-1 = Exit]
    ROLE --> CO
    ROLE -.->|-1| BYE

    CO{{"menu_company<br/>Projects/Team/Save/Portfolio<br/>My tasks/All-task Gantt/Workload/Exec report"}}
    CO -.->|0 Sign out, company stays loaded| ROLE
    CO -->|1999| EGG[[Project Mayhem rules]]

    CO -->|VIEW_PROJECT| PROJ{{menu_company_projects<br/>New / Open}}
    CO -->|ADMIN| TEAM{{menu_company_team<br/>Add / Remove}}
    CO -->|VIEW_PORTFOLIO| PV[render_portfolio_gantt]
    CO -->|VIEW_PORTFOLIO| CG[render_company_gantt]
    CO -->|VIEW_PORTFOLIO| WL[render_workload_report]
    CO -->|VIEW_OWN| MT[show_my_tasks]
    CO -->|REPORT| EX[export_executive_report_html]

    PROJ -->|New ADMIN| CREATE[create_project]
    PROJ -->|Open| OPEN

    OPEN{{open_project<br/>Tasks/Deps/Milestones/Schedule/Members/Simulate}}
    OPEN -->|EDIT_TASK| TASKS{{menu_tasks<br/>List/Add/Remove/Edit/Status/Link/Deps/Assign/Split}}
    OPEN -->|EDIT_DEPS| DEPS{{menu_deps}}
    OPEN --> MILE{{menu_milestones}}
    OPEN --> SCH
    OPEN -->|ASSIGN| ROST[[roster checklist]]
    OPEN -->|SCHEDULE| SIM[[menu_simulate<br/>day-by-day sandbox]]

    TASKS -->|Assign ASSIGN| PIN[manual_assign]
    SCH{{menu_schedule<br/>Generate/Report/Gantt/DAG/Export .dot/Export report/Request vacation}}
    SCH -->|Generate SCHEDULE| GEN[[scheduler pipeline - see diagram 3]]
    SCH -->|Export report REPORT| RPT[export_report_html]
```

---

## 3. Scheduler / assignment / Gantt pipeline

```mermaid
flowchart TD
    A[menu_schedule: Generate] --> B{pick strategy<br/>earliest / risk / pessimistic}
    B --> PCY{pick policy<br/>earliest-free / balanced / best-fit}
    PCY --> C[assign_members_greedy c, idx, strategy]

    subgraph ASSIGN [assign_members_greedy]
        C --> X0[compute_external_floor<br/>cross-project calendar:<br/>seed member_free_day + Task.min_start]
        X0 --> C1[clear_work_assignments<br/>keep manual / fixed_time pins]
        C1 --> C2[schedule_project strategy]
        C2 --> C3{pass loop}
        C3 --> C4[sort tasks by sched_start]
        C4 --> C5[assignment_pass:<br/>find_best_member by policy key,<br/>add work_pre on member conflict]
        C5 --> C6{all assigned?}
        C6 -->|no, progress| C7[schedule_project] --> C3
        C6 -->|yes / stuck| C8[resolve_resource_overlaps]
    end

    subgraph SP [schedule_project - CPM]
        S1[topo_sort<br/>Kahn: pre + work_pre] --> S2[forward_pass<br/>start = max pre/work_pre/min_start<br/>fixed_time keeps window]
        S2 --> S3[backward_pass<br/>latest_start, slack, is_critical]
        S3 --> S4[stamp topo_order]
    end
    C2 -.calls.-> SP

    subgraph RES [resolve_resource_overlaps - fixpoint]
        R1[schedule_project] --> R2[group by assignee, sort by start]
        R2 --> R3{adjacent same-member overlap?}
        R3 -->|yes| R4[add work_pre; fixed block = flexible task waits] --> R1
        R3 -->|none added| R5[done]
    end
    C8 -.calls.-> RES

    C8 --> D[schedule_project strategy]
    D --> E[render_gantt<br/>absolute company timeline + ruler]
    E --> F[resolve_unassigned<br/>cascade: suggest off-roster member]
    F --> G[company_save]
```
