#define _GNU_SOURCE

#include "lvgl.h"
#include "lv_demos.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define FB_DEVICE "/dev/fb0"
#define EXPECTED_WIDTH 640U
#define EXPECTED_HEIGHT 480U

struct framebuffer {
    int fd;
    uint8_t *pixels;
    size_t map_length;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
};

static struct framebuffer framebuffer;

static uint32_t monotonic_milliseconds(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint32_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

static void framebuffer_flush(lv_display_t *display, const lv_area_t *area,
                              uint8_t *pixel_map)
{
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;
    uint32_t width;
    uint32_t row;

    if (x1 < 0)
        x1 = 0;
    if (y1 < 0)
        y1 = 0;
    if (x2 >= (int32_t)framebuffer.width)
        x2 = (int32_t)framebuffer.width - 1;
    if (y2 >= (int32_t)framebuffer.height)
        y2 = (int32_t)framebuffer.height - 1;

    if (x1 > x2 || y1 > y2) {
        lv_display_flush_ready(display);
        return;
    }

    width = (uint32_t)(x2 - x1 + 1);
    for (row = (uint32_t)y1; row <= (uint32_t)y2; ++row) {
        uint8_t *destination = framebuffer.pixels +
            row * framebuffer.stride + (uint32_t)x1 * sizeof(uint16_t);
        const uint8_t *source = pixel_map +
            (row - (uint32_t)area->y1) *
            (uint32_t)(area->x2 - area->x1 + 1) * sizeof(uint16_t) +
            (uint32_t)(x1 - area->x1) * sizeof(uint16_t);

        memcpy(destination, source, width * sizeof(uint16_t));
    }

    lv_display_flush_ready(display);
}

static int framebuffer_open(void)
{
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;

    memset(&framebuffer, 0, sizeof(framebuffer));
    framebuffer.fd = open(FB_DEVICE, O_RDWR | O_CLOEXEC);
    if (framebuffer.fd < 0) {
        fprintf(stderr, "lvgl-demo: cannot open %s: %s\n", FB_DEVICE,
                strerror(errno));
        return -1;
    }
    if (ioctl(framebuffer.fd, FBIOGET_FSCREENINFO, &fix) < 0 ||
        ioctl(framebuffer.fd, FBIOGET_VSCREENINFO, &var) < 0) {
        fprintf(stderr, "lvgl-demo: cannot query framebuffer: %s\n",
                strerror(errno));
        close(framebuffer.fd);
        framebuffer.fd = -1;
        return -1;
    }
    if (var.bits_per_pixel != 16 || var.xres != EXPECTED_WIDTH ||
        var.yres != EXPECTED_HEIGHT || fix.line_length < var.xres * 2U) {
        fprintf(stderr,
                "lvgl-demo: unsupported framebuffer %ux%u %ubpp stride %u\n",
                var.xres, var.yres, var.bits_per_pixel, fix.line_length);
        close(framebuffer.fd);
        framebuffer.fd = -1;
        return -1;
    }

    framebuffer.width = var.xres;
    framebuffer.height = var.yres;
    framebuffer.stride = fix.line_length;
    framebuffer.map_length = fix.smem_len;
    if (framebuffer.map_length < framebuffer.stride * framebuffer.height)
        framebuffer.map_length = framebuffer.stride * framebuffer.height;
    framebuffer.pixels = mmap(NULL, framebuffer.map_length,
                               PROT_READ | PROT_WRITE, MAP_SHARED,
                               framebuffer.fd, 0);
    if (framebuffer.pixels == MAP_FAILED) {
        fprintf(stderr, "lvgl-demo: cannot map framebuffer: %s\n",
                strerror(errno));
        close(framebuffer.fd);
        framebuffer.fd = -1;
        framebuffer.pixels = NULL;
        return -1;
    }

    memset(framebuffer.pixels, 0, framebuffer.map_length);
    return 0;
}

static void framebuffer_close(void)
{
    if (framebuffer.pixels != NULL)
        munmap(framebuffer.pixels, framebuffer.map_length);
    if (framebuffer.fd >= 0)
        close(framebuffer.fd);
}

int main(void)
{
    lv_display_t *display;
    uint32_t previous_tick;

    if (framebuffer_open() != 0)
        return EXIT_FAILURE;

    lv_init();
    display = lv_display_create(framebuffer.width, framebuffer.height);
    if (display == NULL) {
        fprintf(stderr, "lvgl-demo: cannot create LVGL display\n");
        framebuffer_close();
        return EXIT_FAILURE;
    }

    static lv_color_t draw_buffer[EXPECTED_WIDTH * 64];
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, framebuffer_flush);
    lv_demo_benchmark();

    fprintf(stderr, "lvgl-demo: running LVGL benchmark on %ux%u RGB565 framebuffer\n",
            framebuffer.width, framebuffer.height);
    previous_tick = monotonic_milliseconds();
    for (;;) {
        uint32_t current_tick = monotonic_milliseconds();
        lv_tick_inc(current_tick - previous_tick);
        previous_tick = current_tick;
        lv_timer_handler();
        usleep(5000);
    }
}
