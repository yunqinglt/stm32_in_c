#ifndef MIPSEL_EMU_IMAGE_LOADER_H
#define MIPSEL_EMU_IMAGE_LOADER_H

#include "config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * A random-access, read-only image.  The callback may be backed by a host
 * file, memory-mapped SPI flash, or a board-specific flash driver.  It must
 * either transfer the complete requested range or return false.
 */
typedef bool (*mipsel_image_read_fn)(void *opaque, uint32_t offset,
                                     void *destination, size_t length);

typedef struct mipsel_image {
    void *opaque;
    uint32_t size;
    mipsel_image_read_fn read;
} mipsel_image_t;

typedef struct mipsel_memory_range {
    uint32_t start;
    uint32_t end;
} mipsel_memory_range_t;

typedef struct mipsel_elf_load_range {
    uint32_t start;
    uint32_t end;
    bool executable;
} mipsel_elf_load_range_t;

typedef struct mipsel_elf_info {
    uint32_t entry;
    size_t load_segment_count;
    mipsel_elf_load_range_t
        load_segments[MIPSEL_EMU_ELF_MAX_LOAD_SEGMENTS];
} mipsel_elf_info_t;

typedef enum mipsel_image_status {
    MIPSEL_IMAGE_OK = 0,
    MIPSEL_IMAGE_INVALID_ARGUMENT,
    MIPSEL_IMAGE_SOURCE_IO,
    MIPSEL_IMAGE_BAD_FORMAT,
    MIPSEL_IMAGE_UNSUPPORTED,
    MIPSEL_IMAGE_TOO_MANY_SEGMENTS,
    MIPSEL_IMAGE_OUT_OF_RANGE,
    MIPSEL_IMAGE_OVERLAP,
    MIPSEL_IMAGE_NO_LOAD_SEGMENTS,
    MIPSEL_IMAGE_INVALID_ENTRY,
    MIPSEL_IMAGE_MEMORY_IO,
    MIPSEL_IMAGE_FDT_PROPERTY_MISSING,
} mipsel_image_status_t;

const char *mipsel_image_status_string(mipsel_image_status_t status);

/*
 * Validate an ELF32 little-endian MIPS executable, then copy all PT_LOAD
 * segments into the configured platform memory backend.  Validation is a
 * complete first pass, so malformed images do not partially modify RAM.
 * Returns MIPSEL_IMAGE_UNSUPPORTED when ELF support is compiled out.
 */
mipsel_image_status_t mipsel_elf_load(
    const mipsel_image_t *image,
    const mipsel_memory_range_t *reserved_ranges,
    size_t reserved_range_count,
    mipsel_elf_info_t *info);

/* Validate and copy an FDT blob into the configured platform memory. */
mipsel_image_status_t mipsel_dtb_load(const mipsel_image_t *image,
                                      uint32_t destination,
                                      uint32_t capacity,
                                      uint32_t *loaded_size);

/*
 * Place an external initramfs as high in RAM as possible while avoiding the
 * supplied kernel/DTB ranges.  The returned range contains physical guest
 * addresses suitable for linux,initrd-start/end.
 * Returns MIPSEL_IMAGE_UNSUPPORTED when initramfs support is compiled out.
 */
mipsel_image_status_t mipsel_initramfs_load(
    const mipsel_image_t *image,
    const mipsel_memory_range_t *reserved_ranges,
    size_t reserved_range_count,
    uint32_t alignment,
    mipsel_memory_range_t *loaded_range);

/*
 * Update pre-existing linux,initrd-start/end properties in /chosen.  The FDT
 * resides in platform memory, so this works with both flat RAM and PSRAM.
 * Four-byte and eight-byte property encodings are accepted; no blob growth
 * or heap allocation is performed.
 */
mipsel_image_status_t mipsel_fdt_set_initramfs(uint32_t dtb_address,
                                               uint32_t dtb_capacity,
                                               uint32_t initrd_start,
                                               uint32_t initrd_end);

#endif
