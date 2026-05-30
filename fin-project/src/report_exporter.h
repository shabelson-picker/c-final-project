#ifndef REPORT_EXPORTER_H
#define REPORT_EXPORTER_H

#include "project.h"
#include "company.h"

/*
 * Write a self-contained HTML project report to 'filename' and open it in the
 * default browser. Sections: header, task-status table, Gantt chart, and the
 * dependency graph rendered inline as SVG via Graphviz (`dot -Tsvg`).
 * If Graphviz is not installed, the graph section shows a warning instead.
 * Returns 1 on success, 0 on file error.
 */
int export_report_html(const Project *p, const Company *c, const char *filename);

#endif /* REPORT_EXPORTER_H */
