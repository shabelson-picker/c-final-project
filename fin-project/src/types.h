#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include "constants.h"

/* Skill bitmask - combine with | to require multiple skills */
typedef enum {
    SKILL_NONE      = 0,
    SKILL_FRONTEND  = 1 << 0,
    SKILL_BACKEND   = 1 << 1,
    SKILL_HARDWARE  = 1 << 2,
    SKILL_EMBEDDED  = 1 << 3,
    SKILL_QA        = 1 << 4,
    SKILL_DEVOPS    = 1 << 5,
    SKILL_DESIGN    = 1 << 6,
    SKILL_PM        = 1 << 7
} Skill;

#define SKILL_COUNT 8
/* Display names - index i maps to Skill (1 << i) */
static const char * const SKILL_NAMES[SKILL_COUNT] = {
    "Frontend", "Backend", "Hardware", "Embedded",
    "QA", "DevOps", "Design", "PM"
};

typedef enum {
    STATUS_TODO,
    STATUS_IN_PROGRESS,
    STATUS_DONE,
    STATUS_CANCELLED,
    STATUS_BLOCKED
} TaskStatus;

typedef struct {
    int year;
    int month;
    int day;
} Date;

typedef struct {
    int project_id;
    int task_id;
} TaskRef;

#endif /* TYPES_H */
