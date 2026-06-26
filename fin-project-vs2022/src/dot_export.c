#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dot_export.h"
#include "constants.h"
#include "util.h"

int dot_write(const Project *p, const char *filename) {
    int i, j;
    FILE *f = fopen(filename, "w");
    if (!f) { printf("  Could not write '%s'\n", filename); return 0; }

    fprintf(f, "digraph \"%s\" {\n", p->name);
    fprintf(f, "  rankdir=LR;\n");
    fprintf(f, "  node [fontname=\"Helvetica\" fontsize=11];\n");
    fprintf(f, "  edge [color=\"#555555\"];\n\n");

    /* Sentinels (START/END) are internal scheduling anchors, not real tasks, so
     * they are omitted - roots simply have no incoming edge, leaves none out.
     * This matches the console DAG view (render_dag), which also hides them. */

    /* Task nodes */
    for (i = 0; i < p->task_count; i++) {
        Task *t = p->tasks[i];
        const char *fill = t->is_critical ? "#F44336" : "#2196F3";

        fprintf(f, "  %d [label=\"[%d] %s\\nexp: %.1fd | risk: %.0f/10",
                t->id, t->id, t->title, t->pert_expected, t->risk * 10.0f);

        if (t->sched_start >= 0)
            fprintf(f, "\\nday %d-%d | slack: %d",
                    t->sched_start, t->sched_end, t->slack);

        fprintf(f, "\" style=filled fillcolor=\"%s\" fontcolor=white];\n", fill);
    }
    fprintf(f, "\n");

    /* Edges between user tasks (END sentinel skipped) */
    for (i = 0; i < p->task_count; i++) {
        Task *t = p->tasks[i];
        for (j = 0; j < t->post_ids.count; j++) {
            if (t->post_ids.data[j] == END_NODE_ID) continue;
            fprintf(f, "  %d -> %d;\n", t->id, t->post_ids.data[j]);
        }
    }

    /* Alt edges (plan B) - dashed */
    for (i = 0; i < p->task_count; i++) {
        Task *t = p->tasks[i];
        for (j = 0; j < t->alt_ids.count; j++)
            fprintf(f, "  %d -> %d [style=dashed color=\"#FF9800\" label=\"alt\"];\n",
                    t->id, t->alt_ids.data[j]);
    }

    /* Milestone markers */
    if (p->milestone_count > 0) {
        fprintf(f, "\n");
        for (i = 0; i < p->milestone_count; i++) {
            Milestone *m = p->milestones[i];
            fprintf(f, "  m%d [label=\"M: %s\\nday %d\" shape=diamond"
                    " style=filled fillcolor=\"#FFC107\" fontcolor=black];\n",
                    m->id, m->name, m->deadline_day);
            for (j = 0; j < m->task_count; j++)
                fprintf(f, "  %d -> m%d [style=dotted color=\"#FFC107\"];\n",
                        m->task_ids[j], m->id);
        }
    }

    fprintf(f, "}\n");
    fclose(f);
    return 1;
}

/* Write the .dot, print confirmation, and open it in the default viewer. */
int export_dot(const Project *p, const char *filename) {
    if (!dot_write(p, filename)) return 0;
    printf("  Saved: %s\n", filename);
    open_in_default_app(filename);
    return 1;
}
