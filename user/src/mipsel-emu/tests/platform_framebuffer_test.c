#include "platform.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (0)

int main(void) {
    platform_framebuffer_rect_t dirty;

    CHECK(platform_framebuffer_bind(platform_framebuffer_data(),
                                    platform_framebuffer_size()));
    platform_framebuffer_clear_dirty();
    write16(MIPSEL_EMU_FB_MMIO_BASE + 2u * 7u, 0x1234u);
    CHECK(read16(MIPSEL_EMU_FB_MMIO_BASE + 2u * 7u) == 0x1234u);
    CHECK(platform_framebuffer_dirty(&dirty));
    CHECK(dirty.x == 7u && dirty.y == 0u &&
          dirty.width == 1u && dirty.height == 1u);

    platform_framebuffer_clear_dirty();
    write32(MIPSEL_EMU_FB_MMIO_BASE + MIPSEL_EMU_FB_STRIDE_BYTES - 1u,
            0xaabbccddu);
    CHECK(platform_framebuffer_dirty(&dirty));
    CHECK(dirty.x == 0u && dirty.y == 0u &&
          dirty.width == MIPSEL_EMU_FB_WIDTH && dirty.height == 2u);
    CHECK(read8(MIPSEL_EMU_FB_MMIO_BASE + MIPSEL_EMU_FB_STRIDE_BYTES - 1u) ==
          0xddu);
    CHECK(read8(MIPSEL_EMU_FB_MMIO_BASE + MIPSEL_EMU_FB_STRIDE_BYTES + 2u) ==
          0xaau);

    platform_framebuffer_clear_dirty();
    CHECK(!platform_framebuffer_dirty(&dirty));
    puts("platform-framebuffer-tests: all checks passed");
    return EXIT_SUCCESS;
}
