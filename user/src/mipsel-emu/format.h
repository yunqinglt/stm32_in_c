#ifndef MIPSEL_EMU_FORMAT_H
#define MIPSEL_EMU_FORMAT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Small freestanding formatter used by the portable emulator components.
 * Supported conversions are %s, %d, %u, %x, and %%.  Integer conversions
 * accept an optional field width and the '0' padding flag (for example,
 * "%08x").  The result is always terminated when BUF is non-null and
 * BUF_SIZE is non-zero.
 *
 * The return value is the number of characters that would have been written,
 * excluding the terminating null byte.  This mirrors snprintf's truncation
 * semantics without depending on stdio.
 */
int mipsel_snprintf(char *buf, size_t buf_size, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
