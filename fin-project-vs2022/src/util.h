#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

/// <summary>
/// Copy src into a fixed-size destination buffer without overflowing, always
/// null-terminating. Copies at most size-1 bytes; a no-op when size is 0.
/// Centralises the "strncpy then force the terminator" idiom used throughout.
/// </summary>
/// <param name="dst">Destination buffer.</param>
/// <param name="src">Source string.</param>
/// <param name="size">Total size of dst in bytes.</param>
void str_copy(char *dst, const char *src, size_t size);

/// <summary>Open a file path with the OS default application (browser, image
/// viewer, etc.). Windows: `start`; otherwise: `xdg-open`.</summary>
/// <param name="path">File to open.</param>
void open_in_default_app(const char *path);

#endif /* UTIL_H */
