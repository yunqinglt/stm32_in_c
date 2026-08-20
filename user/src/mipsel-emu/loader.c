#include "config.h"
#include "debugger.h"
#include "emu.h"
#include "exception.h"
#include "image_loader.h"
#include "platform.h"
#include "registers.h"
#include "runloop.h"

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_KERNEL_PATH "./vmlinuz"

uint8_t *pool;
Registers *state;
vmstate_t *status;

typedef struct {
    const char *kernel_path;
    const char *dtb_path;
    const char *initramfs_path;
    const char *trace_path;
    bool tui;
    bool run_immediately;
    uint64_t max_steps;
} ProgramOptions;

typedef struct {
    const char *kernel_path;
    const char *dtb_path;
    const char *initramfs_path;
    bool verbose;
} BoardConfig;

typedef struct {
    FILE *file;
    mipsel_image_t image;
} HostImage;

static void usage(FILE *stream, const char *program) {
    fprintf(stream,
            "Usage: %s [options] [kernel.elf]\n"
            "\n"
            "Options:\n"
            "  -k, --kernel FILE     ELF32 little-endian MIPS kernel\n"
            "  -d, --dtb FILE        device tree passed with the MIPS UHI ABI\n"
            "  -i, --initramfs FILE  external initramfs copied near top of RAM\n"
            "  -t, --tui             enable the ncurses debugger (starts paused)\n"
            "  -r, --run             start running immediately in TUI mode\n"
            "      --trace FILE      write every instruction/exception ('-' is stderr)\n"
            "      --max-steps N     stop after N CPU ticks (0 means unlimited)\n"
            "  -h, --help            show this help\n"
            "\n"
            "TUI keys: Space run/pause, s step, n 100 steps, r reset,\n"
            "          F2 UART input, Ctrl-] leave UART input,\n"
            "          F3/: paused-target Monitor, q quit.\n"
            "Headless terminal: Ctrl-] q quits; Ctrl-] Ctrl-] sends Ctrl-].\n",
            program);
}

static bool parse_u64(const char *text, uint64_t *value) {
    char *end = NULL;
    unsigned long long parsed;

    if (!text || text[0] == '\0' || text[0] == '-') return false;
    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno || !end || *end != '\0') return false;
    *value = (uint64_t)parsed;
    return true;
}

static int parse_options(int argc, char **argv, ProgramOptions *options) {
    enum { OPT_TRACE = 1000, OPT_MAX_STEPS };
    static const struct option long_options[] = {
        {"kernel", required_argument, NULL, 'k'},
        {"dtb", required_argument, NULL, 'd'},
        {"initramfs", required_argument, NULL, 'i'},
        {"tui", no_argument, NULL, 't'},
        {"run", no_argument, NULL, 'r'},
        {"trace", required_argument, NULL, OPT_TRACE},
        {"max-steps", required_argument, NULL, OPT_MAX_STEPS},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };
    int option;

    *options = (ProgramOptions) {
        .kernel_path = DEFAULT_KERNEL_PATH,
    };

    while ((option = getopt_long(argc, argv, "k:d:i:trh", long_options,
                                 NULL)) != -1) {
        switch (option) {
            case 'k': options->kernel_path = optarg; break;
            case 'd': options->dtb_path = optarg; break;
            case 'i': options->initramfs_path = optarg; break;
            case 't': options->tui = true; break;
            case 'r': options->run_immediately = true; break;
            case OPT_TRACE: options->trace_path = optarg; break;
            case OPT_MAX_STEPS:
                if (!parse_u64(optarg, &options->max_steps)) {
                    fprintf(stderr, "invalid --max-steps value: %s\n", optarg);
                    return -1;
                }
                break;
            case 'h': usage(stdout, argv[0]); return 1;
            default: usage(stderr, argv[0]); return -1;
        }
    }

    if (optind < argc) options->kernel_path = argv[optind++];
    if (optind != argc) {
        fprintf(stderr, "only one positional kernel path is accepted\n");
        return -1;
    }
    if (options->initramfs_path && !options->dtb_path) {
        fprintf(stderr, "--initramfs requires --dtb so Linux receives its range\n");
        return -1;
    }
#if !MIPSEL_EMU_ENABLE_INITRAMFS
    if (options->initramfs_path) {
        fprintf(stderr, "this build has initramfs loading disabled\n");
        return -1;
    }
#endif
    return 0;
}

static long file_length(FILE *file) {
    long current = ftell(file);
    long length;

    if (current < 0 || fseek(file, 0, SEEK_END) != 0) return -1;
    length = ftell(file);
    if (length < 0 || fseek(file, current, SEEK_SET) != 0) return -1;
    return length;
}

static bool host_image_read(void *opaque, uint32_t offset,
                            void *destination, size_t length) {
    HostImage *source = opaque;

#if LONG_MAX < UINT32_MAX
    if (offset > (uint32_t)LONG_MAX) return false;
#endif
    return source && source->file && destination &&
           fseek(source->file, (long)offset, SEEK_SET) == 0 &&
           fread(destination, 1, length, source->file) == length;
}

static int host_image_open(HostImage *source, const char *path,
                           const char *description) {
    long length;

    *source = (HostImage){0};
    source->file = fopen(path, "rb");
    if (!source->file) {
        fprintf(stderr, "cannot open %s '%s': %s\n", description, path,
                strerror(errno));
        return -1;
    }
    length = file_length(source->file);
    if (length < 0 || (uintmax_t)length > UINT32_MAX) {
        fprintf(stderr, "cannot determine supported size of %s '%s'\n",
                description, path);
        fclose(source->file);
        *source = (HostImage){0};
        return -1;
    }
    source->image = (mipsel_image_t) {
        .opaque = source,
        .size = (uint32_t)length,
        .read = host_image_read,
    };
    return 0;
}

static void host_image_close(HostImage *source) {
    if (source && source->file) fclose(source->file);
    if (source) *source = (HostImage){0};
}

static int report_image_error(const char *description, const char *path,
                              mipsel_image_status_t image_status) {
    fprintf(stderr, "cannot load %s '%s': %s\n", description, path,
            mipsel_image_status_string(image_status));
    return -1;
}

static int load_kernel(const BoardConfig *config, mipsel_elf_info_t *info) {
    HostImage source;
    mipsel_memory_range_t dtb_reservation;
    const mipsel_memory_range_t *reservations = NULL;
    size_t reservation_count = 0;
    mipsel_image_status_t image_status;
    size_t i;

    if (host_image_open(&source, config->kernel_path, "kernel") != 0) {
        return -1;
    }
    if (config->dtb_path) {
        dtb_reservation = (mipsel_memory_range_t) {
            .start = MIPSEL_EMU_DTB_PHYSICAL_ADDRESS,
            .end = MIPSEL_EMU_DTB_PHYSICAL_ADDRESS +
                   MIPSEL_EMU_DTB_RESERVED_SIZE,
        };
        reservations = &dtb_reservation;
        reservation_count = 1;
    }
    image_status = mipsel_elf_load(&source.image, reservations,
                                   reservation_count, info);
    host_image_close(&source);
    if (image_status != MIPSEL_IMAGE_OK) {
        return report_image_error("kernel", config->kernel_path,
                                  image_status);
    }
    if (config->verbose) {
        for (i = 0; i < info->load_segment_count; ++i) {
            fprintf(stderr, "loaded PT_LOAD %zu: PA %08" PRIx32
                    "..%08" PRIx32 "%s\n", i,
                    info->load_segments[i].start,
                    info->load_segments[i].end,
                    info->load_segments[i].executable ? " executable" : "");
        }
    }
    return 0;
}

static int load_dtb_image(const BoardConfig *config, uint32_t *loaded_size) {
    HostImage source;
    mipsel_image_status_t image_status;

    if (host_image_open(&source, config->dtb_path, "DTB") != 0) {
        return -1;
    }
    image_status = mipsel_dtb_load(&source.image,
                                   MIPSEL_EMU_DTB_PHYSICAL_ADDRESS,
                                   MIPSEL_EMU_DTB_RESERVED_SIZE,
                                   loaded_size);
    host_image_close(&source);
    if (image_status != MIPSEL_IMAGE_OK) {
        return report_image_error("DTB", config->dtb_path, image_status);
    }
    if (config->verbose) {
        fprintf(stderr, "loaded DTB: %" PRIu32 " bytes -> PA %08" PRIx32
                "\n", *loaded_size, MIPSEL_EMU_DTB_PHYSICAL_ADDRESS);
    }
    return 0;
}

static int load_initramfs_image(const BoardConfig *config,
                                const mipsel_elf_info_t *elf_info,
                                uint32_t dtb_size) {
    mipsel_memory_range_t
        reservations[MIPSEL_EMU_ELF_MAX_LOAD_SEGMENTS + 1u];
    mipsel_memory_range_t loaded;
    HostImage source;
    mipsel_image_status_t image_status;
    size_t reservation_count = 0;
    size_t i;

    for (i = 0; i < elf_info->load_segment_count; ++i) {
        reservations[reservation_count++] = (mipsel_memory_range_t) {
            .start = elf_info->load_segments[i].start,
            .end = elf_info->load_segments[i].end,
        };
    }
    reservations[reservation_count++] = (mipsel_memory_range_t) {
        .start = MIPSEL_EMU_DTB_PHYSICAL_ADDRESS,
        .end = MIPSEL_EMU_DTB_PHYSICAL_ADDRESS +
               MIPSEL_EMU_DTB_RESERVED_SIZE,
    };

    if (host_image_open(&source, config->initramfs_path, "initramfs") != 0) {
        return -1;
    }
    image_status = mipsel_initramfs_load(
        &source.image, reservations, reservation_count,
        MIPSEL_EMU_INITRAMFS_ALIGNMENT, &loaded);
    host_image_close(&source);
    if (image_status != MIPSEL_IMAGE_OK) {
        return report_image_error("initramfs", config->initramfs_path,
                                  image_status);
    }
    image_status = mipsel_fdt_set_initramfs(
        MIPSEL_EMU_DTB_PHYSICAL_ADDRESS, dtb_size,
        loaded.start, loaded.end);
    if (image_status != MIPSEL_IMAGE_OK) {
        return report_image_error("initramfs metadata in DTB",
                                  config->dtb_path, image_status);
    }
    if (config->verbose) {
        fprintf(stderr, "loaded initramfs: %" PRIu32 " bytes -> PA %08"
                PRIx32 "..%08" PRIx32 "\n",
                loaded.end - loaded.start, loaded.start, loaded.end);
    }
    return 0;
}

static int board_reset(Registers *cpu, void *opaque) {
    BoardConfig *config = opaque;
    mipsel_elf_info_t elf_info;
    uint32_t dtb_size = 0;
    uint32_t dtb_virtual_address = 0;

    if (!platform_memory_fill(0, 0, platform_memory_size())) {
        fprintf(stderr, "cannot clear guest RAM\n");
        return -1;
    }
    if (load_kernel(config, &elf_info) != 0) return -1;
    if (config->dtb_path) {
        if (load_dtb_image(config, &dtb_size) != 0) return -1;
        dtb_virtual_address = MIPSEL_EMU_DTB_VIRTUAL_ADDRESS;
    }
    if (config->initramfs_path &&
        load_initramfs_image(config, &elf_info, dtb_size) != 0) {
        return -1;
    }

    linux_load_reset(cpu);
    cpu->pc = elf_info.entry;
    cpu->next_pc = elf_info.entry + 4u;
    if (dtb_virtual_address) {
        cpu->gpr[4] = UINT32_C(0xfffffffe); /* UHI: a1 is an FDT. */
        cpu->gpr[5] = dtb_virtual_address;
    } else if (config->verbose) {
        fprintf(stderr,
                "warning: no DTB supplied; a MIPS Generic kernel will stop "
                "in prom_init()\n");
    }

    config->verbose = false;
    return 0;
}

static void host_uart_tx(void *opaque, uint8_t byte) {
    (void)opaque;
    debugger_uart_tx(byte);
}

int main(int argc, char **argv) {
    ProgramOptions options;
    BoardConfig board_config;
    DebuggerConfig debugger_config;
    bool debugger_started = false;
    int option_result;
    int result = EXIT_FAILURE;

    option_result = parse_options(argc, argv, &options);
    if (option_result != 0)
        return option_result > 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    pool = calloc(1, PLATFORM_MEMORY_SIZE);
    status = calloc(1, sizeof(*status));
    state = calloc(1, sizeof(*state));
    if (!pool || !status || !state) {
        fprintf(stderr, "cannot allocate emulator state: %s\n", strerror(errno));
        goto done;
    }
    if (!platform_memory_bind(pool, PLATFORM_MEMORY_SIZE)) {
        fprintf(stderr, "cannot bind emulator RAM backend\n");
        goto done;
    }

    board_config = (BoardConfig) {
        .kernel_path = options.kernel_path,
        .dtb_path = options.dtb_path,
        .initramfs_path = options.initramfs_path,
        .verbose = true,
    };
    *status = (vmstate_t) {
        .cpu_ctx = state,
        .state = RUNNING,
        .max_ticks = options.max_steps,
        .reset_callback = board_reset,
        .reset_opaque = &board_config,
    };
    if (board_reset(state, &board_config) != 0) goto done;

    debugger_config = (DebuggerConfig) {
        .enable_tui = options.tui,
        .start_paused = options.tui && !options.run_immediately,
        .trace_path = options.trace_path,
        .refresh_hz = 30,
    };
    fprintf(stderr,
            "starting MIPS32EL at PC=%08" PRIx32
            ", RAM=%u MiB, UART=0x%08x\n",
            state->pc, platform_memory_size() / (1024u * 1024u),
            UART16550_MMIO_BASE);
    if (debugger_init(&debugger_config, state, status) != 0) goto done;
    debugger_started = true;
    platform_init(host_uart_tx, NULL);

    result = startup(state) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    if (!options.tui && options.max_steps &&
        status->ticks >= options.max_steps) {
        fprintf(stderr,
                "stopped after %" PRIu64 " ticks at PC=%08" PRIx32 "\n",
                status->ticks, state->pc);
    }

done:
    if (debugger_started) debugger_shutdown();
    free(state);
    free(status);
    free(pool);
    state = NULL;
    status = NULL;
    pool = NULL;
    return result;
}
