#include "image_loader.h"

#include "platform.h"

#include <stddef.h>
#include <stdint.h>

#if MIPSEL_EMU_ELF_MAX_LOAD_SEGMENTS < 1
#error "MIPSEL_EMU_ELF_MAX_LOAD_SEGMENTS must be at least one"
#endif

#if MIPSEL_EMU_IMAGE_CHUNK_SIZE < 1
#error "MIPSEL_EMU_IMAGE_CHUNK_SIZE must be at least one"
#endif

#if MIPSEL_EMU_ENABLE_ELF_LOADER
#define ELF32_HEADER_SIZE          52u
#define ELF32_PROGRAM_HEADER_SIZE  32u
#define ELF32_PT_LOAD              1u
#define ELF32_PF_X                 1u
#define ELF32_ET_EXEC              2u
#define ELF32_EM_MIPS              8u
#define ELF_VERSION_CURRENT        1u
#endif

#define FDT_MAGIC       UINT32_C(0xd00dfeed)
#define FDT_BEGIN_NODE  UINT32_C(1)
#define FDT_END_NODE    UINT32_C(2)
#define FDT_PROP        UINT32_C(3)
#define FDT_NOP         UINT32_C(4)
#define FDT_END         UINT32_C(9)
#define FDT_HEADER_SIZE 40u

#if MIPSEL_EMU_ENABLE_ELF_LOADER
typedef struct elf_segment_fields {
    uint32_t file_offset;
    uint32_t file_size;
    uint32_t memory_size;
    uint32_t destination;
    uint32_t flags;
} elf_segment_fields_t;
#endif

typedef struct fdt_layout {
    uint32_t total_size;
    uint32_t struct_offset;
    uint32_t struct_size;
    uint32_t strings_offset;
    uint32_t strings_size;
} fdt_layout_t;

#if MIPSEL_EMU_ENABLE_ELF_LOADER
static uint16_t read_le16(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8);
}

static uint32_t read_le32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
           (uint32_t)bytes[1] << 8 |
           (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}
#endif

static uint32_t read_be32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] << 24 |
           (uint32_t)bytes[1] << 16 |
           (uint32_t)bytes[2] << 8 |
           (uint32_t)bytes[3];
}

#if MIPSEL_EMU_ENABLE_INITRAMFS
static void write_be32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}
#endif

#if MIPSEL_EMU_ENABLE_ELF_LOADER
static bool add_fits_u32(uint32_t first, uint32_t second) {
    return first <= UINT32_MAX - second;
}
#endif

static bool range_fits(uint32_t start, uint32_t length, uint32_t limit) {
    return start <= limit && length <= limit - start;
}

#if MIPSEL_EMU_ENABLE_ELF_LOADER || MIPSEL_EMU_ENABLE_INITRAMFS
static bool ranges_overlap(uint32_t first_start, uint32_t first_end,
                           uint32_t second_start, uint32_t second_end) {
    return first_start < second_end && second_start < first_end;
}
#endif

#if MIPSEL_EMU_ENABLE_ELF_LOADER
static uint32_t physical_address(uint32_t address) {
    if (address >= UINT32_C(0x80000000) &&
        address <= UINT32_C(0xbfffffff)) {
        return address & UINT32_C(0x1fffffff);
    }
    return address;
}

static bool direct_mapped_kernel_address(uint32_t address) {
    return address >= UINT32_C(0x80000000) &&
           address <= UINT32_C(0xbfffffff);
}
#endif

static bool image_range_valid(const mipsel_image_t *image,
                              uint32_t offset, size_t length) {
    return image && image->read && length <= UINT32_MAX &&
           offset <= image->size && length <= image->size - offset;
}

static bool image_read(const mipsel_image_t *image, uint32_t offset,
                       void *destination, size_t length) {
    if (!destination && length != 0) {
        return false;
    }
    if (!image_range_valid(image, offset, length)) {
        return false;
    }
    return length == 0 || image->read(image->opaque, offset,
                                      destination, length);
}

static mipsel_image_status_t copy_image_to_memory(
    const mipsel_image_t *image, uint32_t source_offset,
    uint32_t destination, uint32_t length) {
    uint8_t chunk[MIPSEL_EMU_IMAGE_CHUNK_SIZE];
    uint32_t copied = 0;

    while (copied < length) {
        size_t amount = length - copied;
        if (amount > sizeof(chunk)) {
            amount = sizeof(chunk);
        }
        if (!image_read(image, source_offset + copied, chunk, amount)) {
            return MIPSEL_IMAGE_SOURCE_IO;
        }
        if (!platform_memory_write(destination + copied, chunk, amount)) {
            return MIPSEL_IMAGE_MEMORY_IO;
        }
        copied += (uint32_t)amount;
    }
    return MIPSEL_IMAGE_OK;
}

#if MIPSEL_EMU_ENABLE_ELF_LOADER
static mipsel_image_status_t read_elf_segment(
    const mipsel_image_t *image, uint32_t program_header_offset,
    elf_segment_fields_t *segment, bool *is_load) {
    uint8_t bytes[ELF32_PROGRAM_HEADER_SIZE];
    uint32_t guest_address;

    if (!image_read(image, program_header_offset, bytes, sizeof(bytes))) {
        return MIPSEL_IMAGE_SOURCE_IO;
    }
    *is_load = read_le32(bytes) == ELF32_PT_LOAD;
    if (!*is_load) {
        return MIPSEL_IMAGE_OK;
    }

    segment->file_offset = read_le32(bytes + 4);
    guest_address = read_le32(bytes + 12);
    if (guest_address == 0) {
        guest_address = read_le32(bytes + 8);
    }
    segment->destination = physical_address(guest_address);
    segment->file_size = read_le32(bytes + 16);
    segment->memory_size = read_le32(bytes + 20);
    segment->flags = read_le32(bytes + 24);
    return MIPSEL_IMAGE_OK;
}

static bool overlaps_reserved(
    uint32_t start, uint32_t end,
    const mipsel_memory_range_t *reserved_ranges,
    size_t reserved_range_count) {
    size_t i;

    for (i = 0; i < reserved_range_count; ++i) {
        if (reserved_ranges[i].start < reserved_ranges[i].end &&
            ranges_overlap(start, end, reserved_ranges[i].start,
                           reserved_ranges[i].end)) {
            return true;
        }
    }
    return false;
}
#endif

const char *mipsel_image_status_string(mipsel_image_status_t status) {
    switch (status) {
        case MIPSEL_IMAGE_OK: return "success";
        case MIPSEL_IMAGE_INVALID_ARGUMENT: return "invalid argument";
        case MIPSEL_IMAGE_SOURCE_IO: return "image read failed";
        case MIPSEL_IMAGE_BAD_FORMAT: return "malformed image";
        case MIPSEL_IMAGE_UNSUPPORTED: return "unsupported image";
        case MIPSEL_IMAGE_TOO_MANY_SEGMENTS: return "too many load segments";
        case MIPSEL_IMAGE_OUT_OF_RANGE: return "image does not fit guest RAM";
        case MIPSEL_IMAGE_OVERLAP: return "image ranges overlap";
        case MIPSEL_IMAGE_NO_LOAD_SEGMENTS: return "ELF has no load segment";
        case MIPSEL_IMAGE_INVALID_ENTRY: return "invalid ELF entry point";
        case MIPSEL_IMAGE_MEMORY_IO: return "guest memory access failed";
        case MIPSEL_IMAGE_FDT_PROPERTY_MISSING:
            return "FDT /chosen initramfs properties are missing";
        default: return "unknown image error";
    }
}

mipsel_image_status_t mipsel_elf_load(
    const mipsel_image_t *image,
    const mipsel_memory_range_t *reserved_ranges,
    size_t reserved_range_count,
    mipsel_elf_info_t *info) {
#if !MIPSEL_EMU_ENABLE_ELF_LOADER
    (void)image;
    (void)reserved_ranges;
    (void)reserved_range_count;
    if (info) {
        info->entry = 0;
        info->load_segment_count = 0;
    }
    return MIPSEL_IMAGE_UNSUPPORTED;
#else
    uint8_t header[ELF32_HEADER_SIZE];
    uint32_t program_header_offset;
    uint16_t program_header_size;
    uint16_t program_header_count;
    uint32_t entry;
    size_t load_count = 0;
    size_t i;

    if (!image || !image->read || !info ||
        (reserved_range_count != 0 && !reserved_ranges)) {
        return MIPSEL_IMAGE_INVALID_ARGUMENT;
    }
    info->entry = 0;
    info->load_segment_count = 0;

    if (image->size < sizeof(header) ||
        !image_read(image, 0, header, sizeof(header))) {
        return MIPSEL_IMAGE_SOURCE_IO;
    }
    if (header[0] != 0x7fu || header[1] != 'E' || header[2] != 'L' ||
        header[3] != 'F') {
        return MIPSEL_IMAGE_BAD_FORMAT;
    }
    if (header[4] != 1u || header[5] != 1u || header[6] != 1u ||
        read_le16(header + 16) != ELF32_ET_EXEC ||
        read_le16(header + 18) != ELF32_EM_MIPS ||
        read_le32(header + 20) != ELF_VERSION_CURRENT) {
        return MIPSEL_IMAGE_UNSUPPORTED;
    }
    if (read_le16(header + 40) < ELF32_HEADER_SIZE) {
        return MIPSEL_IMAGE_BAD_FORMAT;
    }

    entry = read_le32(header + 24);
    program_header_offset = read_le32(header + 28);
    program_header_size = read_le16(header + 42);
    program_header_count = read_le16(header + 44);
    if (program_header_size < ELF32_PROGRAM_HEADER_SIZE ||
        (uint64_t)program_header_offset +
            (uint64_t)program_header_size * program_header_count >
            image->size) {
        return MIPSEL_IMAGE_BAD_FORMAT;
    }

    /* Validate and build a fixed-capacity load map before touching RAM. */
    for (i = 0; i < program_header_count; ++i) {
        uint64_t offset64 = (uint64_t)program_header_offset +
                            (uint64_t)i * program_header_size;
        elf_segment_fields_t segment;
        mipsel_image_status_t status;
        bool is_load;
        uint32_t end;
        size_t previous;

        status = read_elf_segment(image, (uint32_t)offset64,
                                  &segment, &is_load);
        if (status != MIPSEL_IMAGE_OK) {
            return status;
        }
        if (!is_load) {
            continue;
        }
        if (load_count >= MIPSEL_EMU_ELF_MAX_LOAD_SEGMENTS) {
            return MIPSEL_IMAGE_TOO_MANY_SEGMENTS;
        }
        if (segment.file_size > segment.memory_size ||
            !range_fits(segment.file_offset, segment.file_size,
                        image->size) ||
            !range_fits(segment.destination, segment.memory_size,
                        platform_memory_size())) {
            return MIPSEL_IMAGE_OUT_OF_RANGE;
        }
        end = segment.destination + segment.memory_size;
        for (previous = 0; previous < load_count; ++previous) {
            if (ranges_overlap(segment.destination, end,
                               info->load_segments[previous].start,
                               info->load_segments[previous].end)) {
                return MIPSEL_IMAGE_OVERLAP;
            }
        }
        if (overlaps_reserved(segment.destination, end, reserved_ranges,
                              reserved_range_count)) {
            return MIPSEL_IMAGE_OVERLAP;
        }
        info->load_segments[load_count].start = segment.destination;
        info->load_segments[load_count].end = end;
        info->load_segments[load_count].executable =
            (segment.flags & ELF32_PF_X) != 0;
        ++load_count;
    }

    if (load_count == 0) {
        return MIPSEL_IMAGE_NO_LOAD_SEGMENTS;
    }
    if (!direct_mapped_kernel_address(entry) || (entry & 3u) != 0 ||
        !add_fits_u32(entry, 4u) ||
        physical_address(entry) >= platform_memory_size()) {
        return MIPSEL_IMAGE_INVALID_ENTRY;
    }
    for (i = 0; i < load_count; ++i) {
        uint32_t entry_pa = physical_address(entry);
        if (info->load_segments[i].executable &&
            entry_pa >= info->load_segments[i].start &&
            entry_pa < info->load_segments[i].end) {
            break;
        }
    }
    if (i == load_count) {
        return MIPSEL_IMAGE_INVALID_ENTRY;
    }

    /* Second pass: the complete image has already been validated. */
    load_count = 0;
    for (i = 0; i < program_header_count; ++i) {
        uint32_t offset = program_header_offset +
                          (uint32_t)i * program_header_size;
        elf_segment_fields_t segment;
        mipsel_image_status_t status;
        bool is_load;

        status = read_elf_segment(image, offset, &segment, &is_load);
        if (status != MIPSEL_IMAGE_OK) {
            return status;
        }
        if (!is_load) {
            continue;
        }
        status = copy_image_to_memory(image, segment.file_offset,
                                      segment.destination,
                                      segment.file_size);
        if (status != MIPSEL_IMAGE_OK) {
            return status;
        }
        if (!platform_memory_fill(segment.destination + segment.file_size,
                                  0,
                                  segment.memory_size - segment.file_size)) {
            return MIPSEL_IMAGE_MEMORY_IO;
        }
        ++load_count;
    }

    info->entry = entry;
    info->load_segment_count = load_count;
    return MIPSEL_IMAGE_OK;
#endif
}

static mipsel_image_status_t fdt_layout_from_header(
    const uint8_t header[FDT_HEADER_SIZE], uint32_t capacity,
    fdt_layout_t *layout) {
    uint32_t version;
    uint32_t last_compatible_version;

    if (read_be32(header) != FDT_MAGIC) {
        return MIPSEL_IMAGE_BAD_FORMAT;
    }
    layout->total_size = read_be32(header + 4);
    layout->struct_offset = read_be32(header + 8);
    layout->strings_offset = read_be32(header + 12);
    version = read_be32(header + 20);
    last_compatible_version = read_be32(header + 24);
    layout->strings_size = read_be32(header + 32);
    layout->struct_size = read_be32(header + 36);

    if (layout->total_size < FDT_HEADER_SIZE ||
        layout->total_size > capacity || version < 17u ||
        last_compatible_version > 17u ||
        !range_fits(layout->struct_offset, layout->struct_size,
                    layout->total_size) ||
        !range_fits(layout->strings_offset, layout->strings_size,
                    layout->total_size)) {
        return MIPSEL_IMAGE_BAD_FORMAT;
    }
    return MIPSEL_IMAGE_OK;
}

mipsel_image_status_t mipsel_dtb_load(const mipsel_image_t *image,
                                      uint32_t destination,
                                      uint32_t capacity,
                                      uint32_t *loaded_size) {
    uint8_t header[FDT_HEADER_SIZE];
    fdt_layout_t layout;
    mipsel_image_status_t status;

    if (!image || !image->read || !loaded_size ||
        capacity < FDT_HEADER_SIZE ||
        !range_fits(destination, capacity, platform_memory_size())) {
        return MIPSEL_IMAGE_INVALID_ARGUMENT;
    }
    *loaded_size = 0;
    if (image->size < sizeof(header) ||
        !image_read(image, 0, header, sizeof(header))) {
        return MIPSEL_IMAGE_SOURCE_IO;
    }
    status = fdt_layout_from_header(header, image->size, &layout);
    if (status != MIPSEL_IMAGE_OK) {
        return status;
    }
    if (layout.total_size > capacity) {
        return MIPSEL_IMAGE_OUT_OF_RANGE;
    }
    status = copy_image_to_memory(image, 0, destination,
                                  layout.total_size);
    if (status == MIPSEL_IMAGE_OK) {
        *loaded_size = layout.total_size;
    }
    return status;
}

#if MIPSEL_EMU_ENABLE_INITRAMFS
static uint32_t align_down(uint32_t value, uint32_t alignment) {
    return value - value % alignment;
}
#endif

mipsel_image_status_t mipsel_initramfs_load(
    const mipsel_image_t *image,
    const mipsel_memory_range_t *reserved_ranges,
    size_t reserved_range_count,
    uint32_t alignment,
    mipsel_memory_range_t *loaded_range) {
#if !MIPSEL_EMU_ENABLE_INITRAMFS
    (void)image;
    (void)reserved_ranges;
    (void)reserved_range_count;
    (void)alignment;
    if (loaded_range) {
        loaded_range->start = 0;
        loaded_range->end = 0;
    }
    return MIPSEL_IMAGE_UNSUPPORTED;
#else
    uint32_t upper_limit;
    size_t attempts;

    if (!image || !image->read || !loaded_range || alignment == 0 ||
        (reserved_range_count != 0 && !reserved_ranges)) {
        return MIPSEL_IMAGE_INVALID_ARGUMENT;
    }
    loaded_range->start = 0;
    loaded_range->end = 0;
    upper_limit = platform_memory_size();

    for (attempts = 0; attempts <= reserved_range_count; ++attempts) {
        uint32_t start;
        uint32_t end;
        uint32_t next_limit = upper_limit;
        bool conflict = false;
        size_t i;

        if (image->size > upper_limit) {
            return MIPSEL_IMAGE_OUT_OF_RANGE;
        }
        start = align_down(upper_limit - image->size, alignment);
        end = start + image->size;
        for (i = 0; i < reserved_range_count; ++i) {
            if (reserved_ranges[i].start >= reserved_ranges[i].end) {
                continue;
            }
            if (ranges_overlap(start, end, reserved_ranges[i].start,
                               reserved_ranges[i].end)) {
                conflict = true;
                if (reserved_ranges[i].start < next_limit) {
                    next_limit = reserved_ranges[i].start;
                }
            }
        }
        if (!conflict) {
            mipsel_image_status_t status = copy_image_to_memory(
                image, 0, start, image->size);
            if (status != MIPSEL_IMAGE_OK) {
                return status;
            }
            loaded_range->start = start;
            loaded_range->end = end;
            return MIPSEL_IMAGE_OK;
        }
        if (next_limit >= upper_limit) {
            return MIPSEL_IMAGE_OVERLAP;
        }
        upper_limit = next_limit;
    }
    return MIPSEL_IMAGE_OVERLAP;
#endif
}

#if MIPSEL_EMU_ENABLE_INITRAMFS
static bool memory_read_u32_be(uint32_t address, uint32_t *value) {
    uint8_t bytes[4];
    if (!platform_memory_read(address, bytes, sizeof(bytes))) {
        return false;
    }
    *value = read_be32(bytes);
    return true;
}

static bool fdt_string_equals(uint32_t dtb_address,
                              const fdt_layout_t *layout,
                              uint32_t name_offset,
                              const char *expected) {
    uint32_t offset;

    if (name_offset >= layout->strings_size) {
        return false;
    }
    offset = name_offset;
    for (;;) {
        uint8_t actual;
        if (offset >= layout->strings_size ||
            !platform_memory_read(dtb_address + layout->strings_offset +
                                  offset, &actual, 1)) {
            return false;
        }
        if (actual != (uint8_t)*expected) {
            return false;
        }
        if (actual == 0) {
            return true;
        }
        ++offset;
        ++expected;
    }
}

static bool fdt_write_address(uint32_t address, uint32_t length,
                              uint32_t value) {
    uint8_t bytes[8] = {0};

    if (length != 4u && length != 8u) {
        return false;
    }
    write_be32(bytes + length - 4u, value);
    return platform_memory_write(address, bytes, length);
}
#endif

mipsel_image_status_t mipsel_fdt_set_initramfs(uint32_t dtb_address,
                                               uint32_t dtb_capacity,
                                               uint32_t initrd_start,
                                               uint32_t initrd_end) {
#if !MIPSEL_EMU_ENABLE_INITRAMFS
    (void)dtb_address;
    (void)dtb_capacity;
    (void)initrd_start;
    (void)initrd_end;
    return MIPSEL_IMAGE_UNSUPPORTED;
#else
    uint8_t header[FDT_HEADER_SIZE];
    fdt_layout_t layout;
    mipsel_image_status_t status;
    uint32_t cursor;
    uint32_t structure_end;
    int depth = -1;
    int chosen_depth = -1;
    bool found_start = false;
    bool found_end = false;

    if (initrd_start > initrd_end ||
        !range_fits(dtb_address, FDT_HEADER_SIZE,
                    platform_memory_size()) ||
        !platform_memory_read(dtb_address, header, sizeof(header))) {
        return MIPSEL_IMAGE_INVALID_ARGUMENT;
    }
    status = fdt_layout_from_header(header, dtb_capacity, &layout);
    if (status != MIPSEL_IMAGE_OK) {
        return status;
    }
    if (!range_fits(dtb_address, layout.total_size,
                    platform_memory_size())) {
        return MIPSEL_IMAGE_OUT_OF_RANGE;
    }

    cursor = layout.struct_offset;
    structure_end = layout.struct_offset + layout.struct_size;
    while (cursor < structure_end) {
        uint32_t token;
        if (!range_fits(cursor, 4u, structure_end) ||
            !memory_read_u32_be(dtb_address + cursor, &token)) {
            return MIPSEL_IMAGE_MEMORY_IO;
        }
        cursor += 4u;

        if (token == FDT_BEGIN_NODE) {
            uint32_t name_length = 0;
            uint8_t byte;
            bool is_chosen = true;
            static const char chosen_name[] = "chosen";

            ++depth;
            do {
                if (cursor + name_length >= structure_end ||
                    !platform_memory_read(dtb_address + cursor + name_length,
                                          &byte, 1)) {
                    return MIPSEL_IMAGE_MEMORY_IO;
                }
                if (name_length < sizeof(chosen_name)) {
                    if (byte != (uint8_t)chosen_name[name_length]) {
                        is_chosen = false;
                    }
                } else {
                    is_chosen = false;
                }
                ++name_length;
            } while (byte != 0);
            if (depth == 1 && is_chosen &&
                name_length == sizeof(chosen_name)) {
                chosen_depth = depth;
            }
            if (name_length > UINT32_MAX - 3u) {
                return MIPSEL_IMAGE_BAD_FORMAT;
            }
            cursor += (name_length + 3u) & ~UINT32_C(3);
        } else if (token == FDT_END_NODE) {
            if (depth < 0) {
                return MIPSEL_IMAGE_BAD_FORMAT;
            }
            if (depth == chosen_depth) {
                chosen_depth = -1;
            }
            --depth;
        } else if (token == FDT_PROP) {
            uint32_t length;
            uint32_t name_offset;
            uint32_t padded_length;

            if (!range_fits(cursor, 8u, structure_end) ||
                !memory_read_u32_be(dtb_address + cursor, &length) ||
                !memory_read_u32_be(dtb_address + cursor + 4u,
                                    &name_offset)) {
                return MIPSEL_IMAGE_MEMORY_IO;
            }
            cursor += 8u;
            if (length > UINT32_MAX - 3u) {
                return MIPSEL_IMAGE_BAD_FORMAT;
            }
            padded_length = (length + 3u) & ~UINT32_C(3);
            if (!range_fits(cursor, padded_length, structure_end)) {
                return MIPSEL_IMAGE_BAD_FORMAT;
            }
            if (depth == chosen_depth) {
                if (fdt_string_equals(dtb_address, &layout, name_offset,
                                      "linux,initrd-start")) {
                    if (!fdt_write_address(dtb_address + cursor, length,
                                           initrd_start)) {
                        return MIPSEL_IMAGE_MEMORY_IO;
                    }
                    found_start = true;
                } else if (fdt_string_equals(dtb_address, &layout,
                                             name_offset,
                                             "linux,initrd-end")) {
                    if (!fdt_write_address(dtb_address + cursor, length,
                                           initrd_end)) {
                        return MIPSEL_IMAGE_MEMORY_IO;
                    }
                    found_end = true;
                }
            }
            cursor += padded_length;
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            if (depth != -1) {
                return MIPSEL_IMAGE_BAD_FORMAT;
            }
            return found_start && found_end ? MIPSEL_IMAGE_OK :
                MIPSEL_IMAGE_FDT_PROPERTY_MISSING;
        } else {
            return MIPSEL_IMAGE_BAD_FORMAT;
        }
    }
    return MIPSEL_IMAGE_BAD_FORMAT;
#endif
}
