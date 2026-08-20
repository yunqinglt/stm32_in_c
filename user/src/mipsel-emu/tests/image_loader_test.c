#include "image_loader.h"
#include "platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

#define TEST_RAM_SIZE UINT32_C(0x10000)
#define ELF_HEADER_SIZE 52u
#define ELF_PROGRAM_HEADER_SIZE 32u
#define FDT_HEADER_SIZE 40u
#define FDT_BEGIN_NODE UINT32_C(1)
#define FDT_END_NODE UINT32_C(2)
#define FDT_PROP UINT32_C(3)
#define FDT_NOP UINT32_C(4)
#define FDT_END UINT32_C(9)

static uint8_t guest_memory[TEST_RAM_SIZE];

typedef struct memory_image_source {
    const uint8_t *bytes;
    size_t size;
    bool fail_reads;
} memory_image_source_t;

typedef struct fdt_fixture {
    uint8_t bytes[256];
    uint32_t size;
    uint32_t start_value_offset;
    uint32_t end_value_offset;
    uint32_t child_start_value_offset;
    uint32_t child_end_value_offset;
    uint32_t root_end_token_offset;
} fdt_fixture_t;

static void put_le16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void put_be32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static uint32_t get_be32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] << 24 |
           (uint32_t)bytes[1] << 16 |
           (uint32_t)bytes[2] << 8 |
           (uint32_t)bytes[3];
}

static bool memory_image_read(void *opaque, uint32_t offset,
                              void *destination, size_t length) {
    memory_image_source_t *source = opaque;

    if (!source || source->fail_reads || (!destination && length != 0) ||
        offset > source->size || length > source->size - offset) {
        return false;
    }
    if (length != 0) {
        memcpy(destination, source->bytes + offset, length);
    }
    return true;
}

static mipsel_image_t make_image(memory_image_source_t *source,
                                 const uint8_t *bytes, uint32_t size) {
    mipsel_image_t image;

    source->bytes = bytes;
    source->size = size;
    source->fail_reads = false;
    image.opaque = source;
    image.size = size;
    image.read = memory_image_read;
    return image;
}

static bool reset_guest_memory(uint8_t fill) {
    memset(guest_memory, fill, sizeof(guest_memory));
    return platform_memory_bind(guest_memory, sizeof(guest_memory));
}

static uint32_t build_elf(uint8_t *bytes, size_t capacity,
                          size_t segment_count, bool overlap) {
    uint32_t program_offset = ELF_HEADER_SIZE;
    uint32_t data_offset = program_offset +
                           (uint32_t)segment_count * ELF_PROGRAM_HEADER_SIZE;
    size_t i;

    if (segment_count == 0 ||
        capacity < data_offset + segment_count * 4u) {
        return 0;
    }
    memset(bytes, 0, capacity);
    bytes[0] = 0x7fu;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 1u;
    bytes[5] = 1u;
    bytes[6] = 1u;
    put_le16(bytes + 16, 2u);
    put_le16(bytes + 18, 8u);
    put_le32(bytes + 20, 1u);
    put_le32(bytes + 24, UINT32_C(0x80001000));
    put_le32(bytes + 28, program_offset);
    put_le16(bytes + 40, ELF_HEADER_SIZE);
    put_le16(bytes + 42, ELF_PROGRAM_HEADER_SIZE);
    put_le16(bytes + 44, (uint16_t)segment_count);

    for (i = 0; i < segment_count; ++i) {
        uint8_t *program = bytes + program_offset +
                           i * ELF_PROGRAM_HEADER_SIZE;
        uint32_t destination = UINT32_C(0x1000) +
            (overlap ? (uint32_t)i * 4u : (uint32_t)i * UINT32_C(0x100));
        uint32_t guest_address = UINT32_C(0x80000000) + destination;
        size_t j;

        put_le32(program, 1u);
        put_le32(program + 4, data_offset + (uint32_t)i * 4u);
        put_le32(program + 8, guest_address);
        put_le32(program + 12, destination);
        put_le32(program + 16, 4u);
        put_le32(program + 20, 12u);
        put_le32(program + 24, 1u);
        put_le32(program + 28, 4u);
        for (j = 0; j < 4; ++j) {
            bytes[data_offset + i * 4u + j] =
                (uint8_t)(0x40u + i * 4u + j);
        }
    }
    return data_offset + (uint32_t)segment_count * 4u;
}

static int test_elf_load_and_bss(void) {
    uint8_t elf[128];
    memory_image_source_t source;
    mipsel_image_t image;
    mipsel_elf_info_t info;
    uint32_t size;
    size_t i;

    CHECK(reset_guest_memory(0xa5u));
    size = build_elf(elf, sizeof(elf), 1, false);
    CHECK(size != 0);
    image = make_image(&source, elf, size);
    CHECK(mipsel_elf_load(&image, NULL, 0, &info) == MIPSEL_IMAGE_OK);
    CHECK(info.entry == UINT32_C(0x80001000));
    CHECK(info.load_segment_count == 1);
    CHECK(info.load_segments[0].start == UINT32_C(0x1000));
    CHECK(info.load_segments[0].end == UINT32_C(0x100c));
    CHECK(info.load_segments[0].executable);
    for (i = 0; i < 4; ++i) {
        CHECK(guest_memory[0x1000u + i] == (uint8_t)(0x40u + i));
    }
    for (i = 4; i < 12; ++i) {
        CHECK(guest_memory[0x1000u + i] == 0);
    }
    CHECK(guest_memory[0x0fffu] == 0xa5u);
    CHECK(guest_memory[0x100cu] == 0xa5u);
    return 0;
}

static int test_elf_rejections_are_non_destructive(void) {
    uint8_t elf[192];
    memory_image_source_t source;
    mipsel_image_t image;
    mipsel_elf_info_t info;
    uint32_t size;

    CHECK(reset_guest_memory(0x5au));
    size = build_elf(elf, sizeof(elf), 1, false);
    CHECK(size != 0);

    /* The header is complete, but the program-header table is truncated. */
    image = make_image(&source, elf,
                       ELF_HEADER_SIZE + ELF_PROGRAM_HEADER_SIZE - 1u);
    CHECK(mipsel_elf_load(&image, NULL, 0, &info) ==
          MIPSEL_IMAGE_BAD_FORMAT);
    CHECK(guest_memory[0x1000u] == 0x5au);

    size = build_elf(elf, sizeof(elf), 2, true);
    CHECK(size != 0);
    image = make_image(&source, elf, size);
    CHECK(mipsel_elf_load(&image, NULL, 0, &info) ==
          MIPSEL_IMAGE_OVERLAP);
    CHECK(guest_memory[0x1000u] == 0x5au);

    size = build_elf(elf, sizeof(elf), 1, false);
    CHECK(size != 0);
    put_le32(elf + 24, UINT32_C(0x80002000));
    image = make_image(&source, elf, size);
    CHECK(mipsel_elf_load(&image, NULL, 0, &info) ==
          MIPSEL_IMAGE_INVALID_ENTRY);
    CHECK(guest_memory[0x1000u] == 0x5au);
    return 0;
}

static int test_initramfs_top_placement(void) {
    static uint8_t initramfs[0x900];
    memory_image_source_t source;
    mipsel_image_t image;
    mipsel_memory_range_t loaded;
    mipsel_memory_range_t reserved = {
        UINT32_C(0xe800), UINT32_C(0x10000)
    };
    size_t i;

    for (i = 0; i < sizeof(initramfs); ++i) {
        initramfs[i] = (uint8_t)(i * 17u + 3u);
    }
    image = make_image(&source, initramfs, sizeof(initramfs));

    CHECK(reset_guest_memory(0xccu));
    CHECK(mipsel_initramfs_load(&image, NULL, 0, 0x1000u, &loaded) ==
          MIPSEL_IMAGE_OK);
    CHECK(loaded.start == UINT32_C(0xf000));
    CHECK(loaded.end == UINT32_C(0xf900));
    CHECK(memcmp(guest_memory + loaded.start,
                 initramfs, sizeof(initramfs)) == 0);

    CHECK(reset_guest_memory(0xccu));
    CHECK(mipsel_initramfs_load(&image, &reserved, 1, 0x1000u, &loaded) ==
          MIPSEL_IMAGE_OK);
    CHECK(loaded.start == UINT32_C(0xd000));
    CHECK(loaded.end == UINT32_C(0xd900));
    CHECK(memcmp(guest_memory + loaded.start,
                 initramfs, sizeof(initramfs)) == 0);
    CHECK(guest_memory[loaded.start - 1u] == 0xccu);
    CHECK(guest_memory[loaded.end] == 0xccu);
    return 0;
}

static void append_be32(fdt_fixture_t *fixture, uint32_t *cursor,
                        uint32_t value) {
    put_be32(fixture->bytes + *cursor, value);
    *cursor += 4u;
}

static void append_property(fdt_fixture_t *fixture, uint32_t *cursor,
                            uint32_t length, uint32_t name_offset,
                            uint32_t *value_offset) {
    append_be32(fixture, cursor, FDT_PROP);
    append_be32(fixture, cursor, length);
    append_be32(fixture, cursor, name_offset);
    *value_offset = *cursor;
    *cursor += (length + 3u) & ~UINT32_C(3);
}

static void build_fdt(fdt_fixture_t *fixture) {
    static const char start_name[] = "linux,initrd-start";
    static const char end_name[] = "linux,initrd-end";
    const uint32_t reserve_offset = FDT_HEADER_SIZE;
    const uint32_t structure_offset = FDT_HEADER_SIZE + 16u;
    uint32_t cursor = structure_offset;
    uint32_t structure_size;
    uint32_t strings_offset;
    uint32_t strings_size;

    memset(fixture, 0, sizeof(*fixture));
    append_be32(fixture, &cursor, FDT_BEGIN_NODE);
    cursor += 4u; /* Empty root-node name plus alignment. */
    append_be32(fixture, &cursor, FDT_BEGIN_NODE);
    memcpy(fixture->bytes + cursor, "chosen", sizeof("chosen"));
    cursor += 8u;
    append_property(fixture, &cursor, 4u, 0,
                    &fixture->start_value_offset);
    append_property(fixture, &cursor, 8u, sizeof(start_name),
                    &fixture->end_value_offset);
    append_be32(fixture, &cursor, FDT_BEGIN_NODE);
    memcpy(fixture->bytes + cursor, "nested", sizeof("nested"));
    cursor += 8u;
    append_property(fixture, &cursor, 4u, 0,
                    &fixture->child_start_value_offset);
    append_property(fixture, &cursor, 4u, sizeof(start_name),
                    &fixture->child_end_value_offset);
    append_be32(fixture, &cursor, FDT_END_NODE);
    append_be32(fixture, &cursor, FDT_END_NODE);
    fixture->root_end_token_offset = cursor;
    append_be32(fixture, &cursor, FDT_END_NODE);
    append_be32(fixture, &cursor, FDT_END);

    structure_size = cursor - structure_offset;
    strings_offset = cursor;
    memcpy(fixture->bytes + cursor, start_name, sizeof(start_name));
    cursor += sizeof(start_name);
    memcpy(fixture->bytes + cursor, end_name, sizeof(end_name));
    cursor += sizeof(end_name);
    strings_size = (uint32_t)(sizeof(start_name) + sizeof(end_name));
    fixture->size = cursor;

    put_be32(fixture->bytes, UINT32_C(0xd00dfeed));
    put_be32(fixture->bytes + 4, fixture->size);
    put_be32(fixture->bytes + 8, structure_offset);
    put_be32(fixture->bytes + 12, strings_offset);
    put_be32(fixture->bytes + 16, reserve_offset);
    put_be32(fixture->bytes + 20, 17u);
    put_be32(fixture->bytes + 24, 16u);
    put_be32(fixture->bytes + 28, 0u);
    put_be32(fixture->bytes + 32, strings_size);
    put_be32(fixture->bytes + 36, structure_size);
}

static int test_fdt_initramfs_patch(void) {
    fdt_fixture_t fixture;
    memory_image_source_t source;
    mipsel_image_t image;
    uint32_t loaded_size;
    const uint32_t destination = UINT32_C(0x200);
    const uint32_t initrd_start = UINT32_C(0x00123000);
    const uint32_t initrd_end = UINT32_C(0x0012f321);
    const uint8_t *start_value;
    const uint8_t *end_value;
    const uint8_t *child_start_value;
    const uint8_t *child_end_value;

    build_fdt(&fixture);
    image = make_image(&source, fixture.bytes, fixture.size);
    CHECK(reset_guest_memory(0xedu));
    CHECK(mipsel_dtb_load(&image, destination, sizeof(fixture.bytes),
                          &loaded_size) == MIPSEL_IMAGE_OK);
    CHECK(loaded_size == fixture.size);
    CHECK(mipsel_fdt_set_initramfs(destination, sizeof(fixture.bytes),
                                   initrd_start, initrd_end) ==
          MIPSEL_IMAGE_OK);

    start_value = guest_memory + destination + fixture.start_value_offset;
    end_value = guest_memory + destination + fixture.end_value_offset;
    child_start_value = guest_memory + destination +
                        fixture.child_start_value_offset;
    child_end_value = guest_memory + destination +
                      fixture.child_end_value_offset;
    CHECK(get_be32(start_value) == initrd_start);
    CHECK(get_be32(end_value) == 0);
    CHECK(get_be32(end_value + 4) == initrd_end);
    CHECK(get_be32(child_start_value) == 0);
    CHECK(get_be32(child_end_value) == 0);

    /* A structurally unclosed tree must not be accepted at FDT_END. */
    put_be32(guest_memory + destination + fixture.root_end_token_offset,
             FDT_NOP);
    CHECK(mipsel_fdt_set_initramfs(destination, sizeof(fixture.bytes),
                                   initrd_start, initrd_end) ==
          MIPSEL_IMAGE_BAD_FORMAT);
    return 0;
}

int main(void) {
    int result;

    result = test_elf_load_and_bss();
    if (result) return result;
    result = test_elf_rejections_are_non_destructive();
    if (result) return result;
    result = test_initramfs_top_placement();
    if (result) return result;
    return test_fdt_initramfs_patch();
}
