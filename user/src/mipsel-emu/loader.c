#include "debugger.h"
#include "emu.h"
#include "exception.h"
#include "platform.h"
#include "registers.h"

#include <elf.h>
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
#define DTB_PHYSICAL_ADDRESS 0x00010000u
#define DTB_RESERVED_SIZE     0x00010000u
#define DTB_VIRTUAL_ADDRESS  (0x80000000u | DTB_PHYSICAL_ADDRESS)
#define FDT_MAGIC            0xd00dfeedu

uint8_t *pool;
Registers *state;
vmstate_t *status;

typedef struct {
    const char *kernel_path;
    const char *dtb_path;
    const char *trace_path;
    bool tui;
    bool run_immediately;
    uint64_t max_steps;
} ProgramOptions;

typedef struct {
    const char *kernel_path;
    const char *dtb_path;
    bool verbose;
} BoardConfig;

typedef struct {
    uint32_t start;
    uint32_t end;
    bool executable;
} LoadRange;

static void usage(FILE *stream, const char *program) {
    fprintf(stream,
            "Usage: %s [options] [kernel.elf]\n"
            "\n"
            "Options:\n"
            "  -k, --kernel FILE     ELF32 little-endian MIPS kernel\n"
            "  -d, --dtb FILE        device tree passed with the MIPS UHI ABI\n"
            "  -t, --tui             enable the ncurses debugger (starts paused)\n"
            "  -r, --run             start running immediately in TUI mode\n"
            "      --trace FILE      write every instruction/exception ('-' is stderr)\n"
            "      --max-steps N     stop after N CPU ticks (0 means unlimited)\n"
            "  -h, --help            show this help\n"
            "\n"
            "TUI keys: Space run/pause, s step, n 100 steps, r reset,\n"
            "          F2 UART input, Ctrl-] leave UART input, q quit.\n"
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

    while ((option = getopt_long(argc, argv, "k:d:trh", long_options,
                                 NULL)) != -1) {
        switch (option) {
            case 'k': options->kernel_path = optarg; break;
            case 'd': options->dtb_path = optarg; break;
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
    return 0;
}

static uint32_t physical_address(uint32_t address) {
    if (address >= UINT32_C(0x80000000) &&
        address <= UINT32_C(0xbfffffff)) {
        return address & UINT32_C(0x1fffffff);
    }
    return address;
}

static bool is_direct_mapped_kernel_address(uint32_t address) {
    return address >= UINT32_C(0x80000000) &&
           address <= UINT32_C(0xbfffffff);
}

static bool range_fits(uint32_t start, uint32_t length, uint32_t limit) {
    return start <= limit && length <= limit - start;
}

static bool ranges_overlap(uint32_t first_start, uint32_t first_end,
                           uint32_t second_start, uint32_t second_end) {
    return first_start < second_end && second_start < first_end;
}

static bool file_offset_fits_long(uint32_t offset) {
#if LONG_MAX < UINT32_MAX
    return offset <= (uint32_t)LONG_MAX;
#else
    (void)offset;
    return true;
#endif
}

static uint16_t read_le16_field(const void *field) {
    const uint8_t *bytes = field;
    return (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8);
}

static uint32_t read_le32_field(const void *field) {
    const uint8_t *bytes = field;
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
           (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static long file_length(FILE *file) {
    long current = ftell(file);
    long length;

    if (current < 0 || fseek(file, 0, SEEK_END) != 0) return -1;
    length = ftell(file);
    if (length < 0 || fseek(file, current, SEEK_SET) != 0) return -1;
    return length;
}

static int load_elf_kernel(const char *path, uint32_t *entry_out,
                           bool verbose, bool reserve_dtb) {
    Elf32_Ehdr header;
    FILE *file = NULL;
    LoadRange *ranges = NULL;
    long length;
    uint32_t entry;
    uint32_t phoff;
    uint32_t elf_version;
    uint16_t elf_type;
    uint16_t machine;
    uint16_t phentsize;
    uint16_t phnum;
    size_t range_count = 0;
    unsigned int load_segments = 0;
    int result = -1;

    file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "cannot open kernel '%s': %s\n", path,
                strerror(errno));
        return -1;
    }
    length = file_length(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0 ||
        fread(&header, 1, sizeof(header), file) != sizeof(header)) {
        fprintf(stderr, "cannot read ELF header from '%s'\n", path);
        goto done;
    }

    elf_type = read_le16_field(&header.e_type);
    machine = read_le16_field(&header.e_machine);
    elf_version = read_le32_field(&header.e_version);
    entry = read_le32_field(&header.e_entry);
    phoff = read_le32_field(&header.e_phoff);
    phentsize = read_le16_field(&header.e_phentsize);
    phnum = read_le16_field(&header.e_phnum);

    if (memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
        header.e_ident[EI_CLASS] != ELFCLASS32 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_ident[EI_VERSION] != EV_CURRENT ||
        elf_type != ET_EXEC || machine != EM_MIPS ||
        elf_version != EV_CURRENT || phentsize < sizeof(Elf32_Phdr)) {
        fprintf(stderr,
                "'%s' is not a supported ELF32 little-endian MIPS "
                "executable\n",
                path);
        goto done;
    }
    if ((uint64_t)phoff + (uint64_t)phnum * phentsize >
        (uint64_t)length) {
        fprintf(stderr, "ELF program-header table is outside '%s'\n", path);
        goto done;
    }
    ranges = calloc(phnum ? phnum : 1u, sizeof(*ranges));
    if (!ranges) {
        fprintf(stderr, "cannot allocate ELF load map: %s\n",
                strerror(errno));
        goto done;
    }

    for (unsigned int i = 0; i < phnum; ++i) {
        Elf32_Phdr segment;
        uint64_t offset = (uint64_t)phoff + (uint64_t)i * phentsize;
        uint32_t type;
        uint32_t file_offset;
        uint32_t virtual_address;
        uint32_t physical;
        uint32_t file_size;
        uint32_t memory_size;
        uint32_t flags;
        uint32_t guest_address;
        uint32_t destination;
        uint32_t range_end;

        if (offset > LONG_MAX || fseek(file, (long)offset, SEEK_SET) != 0 ||
            fread(&segment, 1, sizeof(segment), file) != sizeof(segment)) {
            fprintf(stderr, "cannot read ELF program header %u\n", i);
            goto done;
        }
        type = read_le32_field(&segment.p_type);
        if (type != PT_LOAD) continue;
        file_offset = read_le32_field(&segment.p_offset);
        virtual_address = read_le32_field(&segment.p_vaddr);
        physical = read_le32_field(&segment.p_paddr);
        file_size = read_le32_field(&segment.p_filesz);
        memory_size = read_le32_field(&segment.p_memsz);
        flags = read_le32_field(&segment.p_flags);

        if (file_size > memory_size || !file_offset_fits_long(file_offset) ||
            (uint64_t)file_offset + file_size >
                (uint64_t)length) {
            fprintf(stderr, "invalid PT_LOAD segment %u in '%s'\n", i, path);
            goto done;
        }

        guest_address = physical ? physical : virtual_address;
        destination = physical_address(guest_address);
        if (!range_fits(destination, memory_size,
                        PLATFORM_MEMORY_SIZE)) {
            fprintf(stderr,
                    "PT_LOAD %u does not fit RAM: pa=%08" PRIx32
                    " memsz=%08" PRIx32 "\n",
                    i, destination, memory_size);
            goto done;
        }
        range_end = destination + memory_size;
        for (size_t previous = 0; previous < range_count; ++previous) {
            if (ranges_overlap(destination, range_end,
                               ranges[previous].start,
                               ranges[previous].end)) {
                fprintf(stderr,
                        "PT_LOAD %u overlaps another load segment at "
                        "PA %08" PRIx32 "\n", i, destination);
                goto done;
            }
        }
        if (reserve_dtb &&
            ranges_overlap(destination, range_end, DTB_PHYSICAL_ADDRESS,
                           DTB_PHYSICAL_ADDRESS + DTB_RESERVED_SIZE)) {
            fprintf(stderr,
                    "PT_LOAD %u overlaps reserved DTB memory at PA %08x\n",
                    i, DTB_PHYSICAL_ADDRESS);
            goto done;
        }
        ranges[range_count++] = (LoadRange) {
            .start = destination,
            .end = range_end,
            .executable = (flags & PF_X) != 0,
        };

        if (file_size &&
            (fseek(file, (long)file_offset, SEEK_SET) != 0 ||
             fread(pool + destination, 1, file_size, file) != file_size)) {
            fprintf(stderr, "short read while loading PT_LOAD %u\n", i);
            goto done;
        }
        memset(pool + destination + file_size, 0, memory_size - file_size);
        if (verbose) {
            fprintf(stderr,
                    "loaded PT_LOAD %u: file=%08" PRIx32
                    " mem=%08" PRIx32 " -> PA %08" PRIx32 "\n",
                    i, file_size, memory_size, destination);
        }
        ++load_segments;
    }

    if (!load_segments) {
        fprintf(stderr, "ELF image '%s' has no PT_LOAD segment\n", path);
        goto done;
    }
    if (!is_direct_mapped_kernel_address(entry) || (entry & 3u) != 0 ||
        entry > UINT32_MAX - 4u ||
        physical_address(entry) >= PLATFORM_MEMORY_SIZE) {
        fprintf(stderr, "ELF entry point is invalid: %08" PRIx32 "\n",
                entry);
        goto done;
    }
    for (size_t i = 0; i < range_count; ++i) {
        uint32_t entry_pa = physical_address(entry);
        if (ranges[i].executable && entry_pa >= ranges[i].start &&
            entry_pa < ranges[i].end) {
            *entry_out = entry;
            result = 0;
            goto done;
        }
    }
    fprintf(stderr,
            "ELF entry %08" PRIx32 " is outside executable PT_LOAD memory\n",
            entry);

done:
    free(ranges);
    fclose(file);
    return result;
}

static uint32_t read_be32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 |
           (uint32_t)bytes[2] << 8 | (uint32_t)bytes[3];
}

static int load_dtb(const char *path, uint32_t *virtual_address_out,
                    bool verbose) {
    uint8_t header[8];
    uint32_t total_size;
    FILE *file;
    long length;

    file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "cannot open DTB '%s': %s\n", path,
                strerror(errno));
        return -1;
    }
    length = file_length(file);
    if (length < (long)sizeof(header) || fseek(file, 0, SEEK_SET) != 0 ||
        fread(header, 1, sizeof(header), file) != sizeof(header) ||
        read_be32(header) != FDT_MAGIC) {
        fprintf(stderr, "'%s' is not a valid flattened device tree\n", path);
        fclose(file);
        return -1;
    }
    total_size = read_be32(header + 4);
    if (total_size < sizeof(header) || total_size > (uint32_t)length ||
        total_size > DTB_RESERVED_SIZE ||
        !range_fits(DTB_PHYSICAL_ADDRESS, total_size,
                    PLATFORM_MEMORY_SIZE)) {
        fprintf(stderr, "invalid DTB size in '%s': %" PRIu32 "\n",
                path, total_size);
        fclose(file);
        return -1;
    }
    if (fseek(file, 0, SEEK_SET) != 0 ||
        fread(pool + DTB_PHYSICAL_ADDRESS, 1, total_size, file) != total_size) {
        fprintf(stderr, "short read while loading DTB '%s'\n", path);
        fclose(file);
        return -1;
    }
    fclose(file);
    *virtual_address_out = DTB_VIRTUAL_ADDRESS;
    if (verbose) {
        fprintf(stderr, "loaded DTB: %" PRIu32 " bytes -> PA %08x\n",
                total_size, DTB_PHYSICAL_ADDRESS);
    }
    return 0;
}

static int board_reset(Registers *cpu, void *opaque) {
    BoardConfig *config = opaque;
    uint32_t kernel_entry;
    uint32_t dtb_address = 0;

    memset(pool, 0, PLATFORM_MEMORY_SIZE);
    if (load_elf_kernel(config->kernel_path, &kernel_entry,
                        config->verbose, config->dtb_path != NULL) != 0) {
        return -1;
    }
    if (config->dtb_path &&
        load_dtb(config->dtb_path, &dtb_address, config->verbose) != 0) {
        return -1;
    }

    linux_load_reset(cpu);
    cpu->pc = kernel_entry;
    cpu->next_pc = kernel_entry + 4u;
    if (dtb_address) {
        cpu->gpr[4] = UINT32_C(0xfffffffe); /* UHI: a1 is an FDT. */
        cpu->gpr[5] = dtb_address;
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

    board_config = (BoardConfig) {
        .kernel_path = options.kernel_path,
        .dtb_path = options.dtb_path,
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
            state->pc, PLATFORM_MEMORY_SIZE / (1024u * 1024u),
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
