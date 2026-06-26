#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "company.h"

/* Save/load a full company bundle (dir/company.yaml, team.yaml, projects/*)  */
int      company_save(const Company *c);
Company *company_load(const char *dir);

/* Save/load a single project bundle within an already-open company */
int      project_save(const Project *p, const char *dir);
Project *project_load(const char *dir);

#endif /* PERSISTENCE_H */
