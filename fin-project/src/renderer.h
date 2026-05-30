#ifndef RENDERER_H
#define RENDERER_H

#include <stdio.h>
#include "project.h"
#include "company.h"

/*
 * Gantt chart - one row per task.
 * PERT range shown as dim dashes, likely duration as solid bright block.
 * Critical path tasks highlighted in red.
 * width: terminal character width for the timeline area.
 */
void render_gantt(const Project *p, const Company *c, int width);

/*
 * Gantt chart as HTML, written to 'out'. Same rows/columns as render_gantt
 * (sorted by start, with DAG order), but uses <span> classes instead of ANSI.
 */
void render_gantt_html(FILE *out, const Project *p, const Company *c, int width);

/*
 * Portfolio Gantt - one bar per project on a shared absolute-date timeline.
 * Each project's span is [start_date, start_date + makespan] where makespan is
 * the max task sched_end. Projects with an invalid/missing start_date are listed
 * as "(no start date)" with no bar.
 */
void render_portfolio_gantt(const Company *c, int width);

/*
 * DAG view - ASCII dependency graph.
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
