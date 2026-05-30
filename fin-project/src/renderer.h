#ifndef RENDERER_H
#define RENDERER_H

#include "project.h"

/*
 * Gantt chart — one row per task.
 * PERT range shown as dim dashes, likely duration as solid bright block.
 * Critical path tasks highlighted in red.
 * width: terminal character width for the timeline area.
 */
void render_gantt(const Project *p, int width);

/*
 * DAG view — ASCII dependency graph.
 * Nodes are task IDs/titles; edges show pre->post relationships.
 */
void render_dag(const Project *p);

/*
 * Horizontal dependency chart for one task.
 * Format:  [pre list]  ->  [*** task ***]  ->  [post list]
 * Sentinels (START/END) are hidden. Task is highlighted.
 */
void render_task_deps(const Project *p, const Task *t);

#endif /* RENDERER_H */
