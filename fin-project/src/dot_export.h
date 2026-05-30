#ifndef DOT_EXPORT_H
#define DOT_EXPORT_H

#include "project.h"

/*
 * Export the task DAG as a Graphviz .dot file.
 * Critical-path tasks are filled red, others light blue.
 * Writes to filename, then opens it with the default system viewer.
 * Returns 1 on success, 0 on file error.
 */
int export_dot(const Project *p, const char *filename);

/* Write the Graphviz .dot to filename without opening it (for embedding). */
int dot_write(const Project *p, const char *filename);

#endif /* DOT_EXPORT_H */
