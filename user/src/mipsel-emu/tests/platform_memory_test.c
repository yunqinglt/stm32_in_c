#include "platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

typedef struct {
    uint8_t bytes[64];
    unsigned int reads;
    unsigned int writes;
} test_memory_t;

static bool test_read(void *opaque, uint32_t pa, void *dst, size_t len) {
    test_memory_t *memory = opaque;
    ++memory->reads;
    memcpy(dst, memory->bytes + pa, len);
    return true;
}

static bool test_write(void *opaque, uint32_t pa, const void *src,
                       size_t len) {
    test_memory_t *memory = opaque;
    ++memory->writes;
    memcpy(memory->bytes + pa, src, len);
    return true;
}

int main(void) {
    static const platform_memory_ops_t ops = {
        .read = test_read,
        .write = test_write,
    };
    test_memory_t memory = {0};
    uint8_t copy[8];
    uint32_t bus_value;
    unsigned int reads;
    unsigned int writes;

    CHECK(!platform_memory_configure(NULL, NULL, sizeof(memory.bytes)));
    CHECK(platform_memory_configure(&ops, &memory, sizeof(memory.bytes)));
    CHECK(platform_memory_size() == sizeof(memory.bytes));
    CHECK(platform_memory_read(sizeof(memory.bytes), NULL, 0));
    CHECK(platform_memory_write(sizeof(memory.bytes), NULL, 0));

    write32(4, UINT32_C(0x78563412));
    CHECK(memory.bytes[4] == 0x12 && memory.bytes[5] == 0x34 &&
          memory.bytes[6] == 0x56 && memory.bytes[7] == 0x78);
    CHECK(read32(4) == UINT32_C(0x78563412));
    CHECK(platform_bus_read(4, 4, &bus_value));
    CHECK(bus_value == UINT32_C(0x78563412));
    CHECK(platform_bus_write(1, 2, UINT32_C(0x0000bbaa)));
    CHECK(memory.bytes[1] == 0xaa && memory.bytes[2] == 0xbb);
    CHECK(platform_bus_read(1, 2, &bus_value));
    CHECK(bus_value == UINT32_C(0x0000bbaa));
    CHECK(!platform_bus_read(0, 3, &bus_value));
    CHECK(!platform_bus_read(0, 1, NULL));
    CHECK(!platform_bus_write(0, 3, 0));

    CHECK(platform_memory_fill(10, 0xa5, 40));
    memset(copy, 0xa5, sizeof(copy));
    CHECK(memcmp(memory.bytes + 10, copy, sizeof(copy)) == 0);
    CHECK(memory.bytes[49] == 0xa5);
    CHECK(memory.writes >= 3); /* write32 plus two fill chunks. */

    reads = memory.reads;
    writes = memory.writes;
    CHECK(!platform_memory_read(63, copy, 2));
    CHECK(!platform_memory_write(63, copy, 2));
    CHECK(!platform_memory_fill(63, 0, 2));
    CHECK(!platform_bus_read(63, 2, &bus_value));
    CHECK(!platform_bus_write(63, 2, 0));
    CHECK(memory.reads == reads && memory.writes == writes);

    CHECK(platform_memory_read(10, copy, sizeof(copy)));
    CHECK(memcmp(copy, memory.bytes + 10, sizeof(copy)) == 0);
    return 0;
}
