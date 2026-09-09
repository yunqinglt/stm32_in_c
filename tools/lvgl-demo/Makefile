LVGL_DIR ?= ../lvgl
CROSS_COMPILE ?= ../mipsel-linux-musl/bin/mipsel-unknown-linux-musl-

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)gcc

LVGL_SOURCES := $(shell find $(LVGL_DIR)/src -type f -name '*.c' -print)
LVGL_DEMO_SOURCES := \
	$(LVGL_DIR)/demos/lv_demos.c \
	$(LVGL_DIR)/demos/benchmark/lv_demo_benchmark.c \
	$(wildcard $(LVGL_DIR)/demos/benchmark/assets/*.c) \
	$(LVGL_DIR)/demos/widgets/lv_demo_widgets.c \
	$(wildcard $(LVGL_DIR)/demos/widgets/assets/*.c)
SOURCES := main.c $(LVGL_SOURCES) $(LVGL_DEMO_SOURCES)
OBJECTS := $(SOURCES:.c=.o)

CPPFLAGS := -I. -I$(LVGL_DIR) -I$(LVGL_DIR)/src -I$(LVGL_DIR)/demos \
	-DLV_CONF_INCLUDE_SIMPLE
CFLAGS := -Os -g0 -std=gnu11 -mips32r2 -mabi=32 -msoft-float -EL \
          -ffunction-sections -fdata-sections -fno-common \
          -Wall -Wextra -Wno-unused-parameter
LDFLAGS := -static -mips32r2 -mabi=32 -msoft-float -EL \
           -Wl,--gc-sections -Wl,-z,max-page-size=4096
LDLIBS := -lm

.PHONY: all clean

all: lvgl-demo

lvgl-demo: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

clean:
	$(RM) lvgl-demo $(OBJECTS) $(OBJECTS:.o=.d)
