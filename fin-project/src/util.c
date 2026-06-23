#include <string.h>
#include "util.h"

void str_copy(char *dst, const char *src, size_t size) {
    if (size == 0) return;
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}
