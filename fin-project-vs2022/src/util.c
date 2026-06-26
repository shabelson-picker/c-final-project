#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

void str_copy(char *dst, const char *src, size_t size) {
    if (size == 0) return;
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

void open_in_default_app(const char *path) {
    char cmd[512];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", path);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" &", path);
#endif
    system(cmd);
}
