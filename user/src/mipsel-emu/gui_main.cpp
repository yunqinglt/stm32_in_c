extern "C" {
#include "config.h"
#include "console.h"
#include "emu.h"
#include "exception.h"
#include "image_loader.h"
#include "observer.h"
#include "platform.h"
#include "registers.h"
#if MIPSEL_EMU_QT_HAVE_SDL
extern "C" {
#include "platform/sdl/sdl_display.h"
}
#endif
}

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextCursor>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstring>

extern "C" {
extern uint8_t *pool;
extern Registers *state;
extern vmstate_t *status;
int mipsel_emu_cli_main(int argc, char **argv);
}

namespace {

struct ImageBuffer {
    QByteArray bytes;
};

static bool image_read(void *opaque, uint32_t offset, void *destination,
                       size_t length) {
    const ImageBuffer *image = static_cast<const ImageBuffer *>(opaque);
    if (!image || !destination || offset > static_cast<uint32_t>(image->bytes.size()))
        return false;
    if (length > static_cast<size_t>(image->bytes.size()) - offset)
        return false;
    std::memcpy(destination, image->bytes.constData() + offset, length);
    return true;
}

static bool read_image(const QString &path, ImageBuffer *image,
                       QString *error) {
    if (!image) return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Cannot open %1: %2")
            .arg(path, file.errorString());
        return false;
    }
    image->bytes = file.readAll();
    if (static_cast<quint64>(image->bytes.size()) > UINT32_MAX) {
        if (error) *error = QStringLiteral("Image is larger than 4 GiB: %1").arg(path);
        return false;
    }
    if (image->bytes.isEmpty() && file.error() != QFile::NoError) {
        if (error) *error = QStringLiteral("Cannot read %1: %2")
            .arg(path, file.errorString());
        return false;
    }
    return true;
}

static QString image_error(const QString &kind, const QString &path,
                           mipsel_image_status_t status_code) {
    return QStringLiteral("Cannot load %1 '%2': %3")
        .arg(kind, path, QString::fromUtf8(mipsel_image_status_string(status_code)));
}

static const char *const gpr_names[32] = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
};

static const char *gui_exception_name(uint8_t code) {
    switch (code) {
    case EXC_RESET: return "Reset";
    case EXC_SRES: return "SoftReset";
    case EXC_INT: return "Interrupt";
    case EXC_MOD: return "TLBModified";
    case EXC_TLBL: return "TLBLoad";
    case EXC_TLBS: return "TLBStore";
    case EXC_AdEL: return "AddressLoad";
    case EXC_AdES: return "AddressStore";
    case EXC_IBE: return "BusFetch";
    case EXC_DBE: return "BusData";
    case EXC_SC: return "Syscall";
    case EXC_BP: return "Breakpoint";
    case EXC_RI: return "ReservedInstruction";
    case EXC_CpU: return "CoprocessorUnusable";
    case EXC_Ov: return "Overflow";
    case EXC_Tr: return "Trap";
    case EXC_FPE: return "FloatingPoint";
    case EXC_C2E: return "Coprocessor2";
    case EXC_DSP: return "DSP";
    case EXC_WATCH: return "Watch";
    case EXC_MCheck: return "MachineCheck";
    case EXC_THR: return "Thread";
    case EXC_CAH: return "CacheError";
    default: return "Unknown";
    }
}

static const char *gui_vector_name(VectorClass vector_class) {
    switch (vector_class) {
    case MIPS_VECTOR_TLB_REFILL: return "tlb-refill";
    case MIPS_VECTOR_INTERRUPT: return "interrupt";
    case MIPS_VECTOR_CACHE_ERROR: return "cache-error";
    case MIPS_VECTOR_RESET: return "reset";
    case MIPS_VECTOR_GENERAL:
    default: return "general";
    }
}

static QString gui_hex(uint32_t value, int width = 8) {
    return QString::number(static_cast<qulonglong>(value), 16)
        .rightJustified(width, QLatin1Char('0'));
}

class GuiController {
public:
    GuiController() {
        observer.opaque = this;
        observer.exception = &GuiController::observer_exception;
        mipsel_emu_observer_set(&observer);
    }
    ~GuiController() {
        mipsel_emu_observer_set(nullptr);
        if (state) std::free(state);
        if (status) std::free(status);
        if (pool) std::free(pool);
        state = nullptr;
        status = nullptr;
        pool = nullptr;
    }

    bool allocate(QString *error) {
        if (!pool) pool = static_cast<uint8_t *>(std::calloc(1, PLATFORM_MEMORY_SIZE));
        if (!status) status = static_cast<vmstate_t *>(std::calloc(1, sizeof(*status)));
        if (!state) state = static_cast<Registers *>(std::calloc(1, sizeof(*state)));
        if (!pool || !state || !status) {
            if (error) *error = QStringLiteral("Cannot allocate %1 MiB guest RAM")
                .arg(PLATFORM_MEMORY_SIZE / (1024u * 1024u));
            return false;
        }
        if (!platform_memory_bind(pool, PLATFORM_MEMORY_SIZE)) {
            if (error) *error = QStringLiteral("Cannot bind guest RAM backend");
            return false;
        }
        platform_init(&GuiController::uart_thunk, this);
        mipsel_console_config_t monitor_config{};
        monitor_config.registers = state;
        monitor_config.halted = &GuiController::monitor_halted;
        monitor_config.bus_read = &GuiController::monitor_bus_read;
        monitor_config.bus_write = &GuiController::monitor_bus_write;
        monitor_config.target_opaque = this;
        monitor_config.output = &GuiController::monitor_emit;
        monitor_config.output_opaque = this;
        if (!mipsel_console_init(&monitor, &monitor_config)) {
            if (error) *error = QStringLiteral("Cannot initialize target monitor");
            return false;
        }
        monitor_initialized = true;
        return true;
    }

    bool reset(const QString &kernel_path, const QString &dtb_path,
               const QString &initramfs_path, QString *error) {
        if (!allocate(error)) return false;
        if (!initramfs_path.isEmpty() && dtb_path.isEmpty()) {
            if (error) *error = QStringLiteral("An initramfs requires a DTB");
            return false;
        }

        ImageBuffer kernel;
        if (!read_image(kernel_path, &kernel, error)) return false;

        mipsel_memory_range_t dtb_reservation{};
        const mipsel_memory_range_t *reservations = nullptr;
        size_t reservation_count = 0;
        if (!dtb_path.isEmpty()) {
            dtb_reservation = {
                MIPSEL_EMU_DTB_PHYSICAL_ADDRESS,
                MIPSEL_EMU_DTB_PHYSICAL_ADDRESS + MIPSEL_EMU_DTB_RESERVED_SIZE,
            };
            reservations = &dtb_reservation;
            reservation_count = 1;
        }

        if (!platform_memory_fill(0, 0, platform_memory_size())) {
            if (error) *error = QStringLiteral("Cannot clear guest RAM");
            return false;
        }
        elf_info = {};
        mipsel_image_t kernel_image{&kernel, static_cast<uint32_t>(kernel.bytes.size()),
                                    image_read};
        mipsel_image_status_t image_status =
            mipsel_elf_load(&kernel_image, reservations, reservation_count, &elf_info);
        if (image_status != MIPSEL_IMAGE_OK) {
            if (error) *error = image_error(QStringLiteral("kernel"), kernel_path, image_status);
            return false;
        }

        uint32_t dtb_size = 0;
        if (!dtb_path.isEmpty()) {
            ImageBuffer dtb;
            if (!read_image(dtb_path, &dtb, error)) return false;
            mipsel_image_t dtb_image{&dtb, static_cast<uint32_t>(dtb.bytes.size()), image_read};
            image_status = mipsel_dtb_load(&dtb_image, MIPSEL_EMU_DTB_PHYSICAL_ADDRESS,
                                           MIPSEL_EMU_DTB_RESERVED_SIZE, &dtb_size);
            if (image_status != MIPSEL_IMAGE_OK) {
                if (error) *error = image_error(QStringLiteral("DTB"), dtb_path, image_status);
                return false;
            }

            if (!initramfs_path.isEmpty()) {
                ImageBuffer initramfs;
                if (!read_image(initramfs_path, &initramfs, error)) return false;
                mipsel_memory_range_t initrd_reservations[MIPSEL_EMU_ELF_MAX_LOAD_SEGMENTS + 1u];
                size_t initrd_count = 0;
                for (size_t i = 0; i < elf_info.load_segment_count; ++i) {
                    initrd_reservations[initrd_count++] = {
                        elf_info.load_segments[i].start,
                        elf_info.load_segments[i].end,
                    };
                }
                initrd_reservations[initrd_count++] = dtb_reservation;
                mipsel_image_t initramfs_image{
                    &initramfs, static_cast<uint32_t>(initramfs.bytes.size()), image_read};
                mipsel_memory_range_t loaded{};
                image_status = mipsel_initramfs_load(
                    &initramfs_image, initrd_reservations, initrd_count,
                    MIPSEL_EMU_INITRAMFS_ALIGNMENT, &loaded);
                if (image_status != MIPSEL_IMAGE_OK) {
                    if (error) *error = image_error(QStringLiteral("initramfs"),
                                                     initramfs_path, image_status);
                    return false;
                }
                image_status = mipsel_fdt_set_initramfs(
                    MIPSEL_EMU_DTB_PHYSICAL_ADDRESS, dtb_size,
                    loaded.start, loaded.end);
                if (image_status != MIPSEL_IMAGE_OK) {
                    if (error) *error = image_error(QStringLiteral("initramfs metadata"),
                                                     dtb_path, image_status);
                    return false;
                }
            }
        }

        linux_load_reset(state);
        state->pc = elf_info.entry;
        state->next_pc = elf_info.entry + 4u;
        if (!dtb_path.isEmpty()) {
            state->gpr[4] = UINT32_C(0xfffffffe);
            state->gpr[5] = MIPSEL_EMU_DTB_VIRTUAL_ADDRESS;
        }
        platform_reset();
        std::memset(platform_framebuffer_data(), 0, platform_framebuffer_size());
        platform_framebuffer_clear_dirty();
        *status = vmstate_t{};
        status->cpu_ctx = state;
        status->state = STEPPING;
        status->max_ticks = 0;
        uart.clear();
        if (monitor_initialized) mipsel_console_reset(&monitor);
        has_snapshot = false;
        return true;
    }

    void resume() {
        if (status) status->state = RUNNING;
    }

    void pause() {
        if (status) {
            status->state = STEPPING;
            status->steps = 0;
        }
    }

    void step(uint32_t count = 1) {
        if (!state || !status) return;
        status->state = STEPPING;
        status->steps = 0;
        for (uint32_t i = 0; i < count; ++i) {
            mipsel_emu_step(state);
            ++status->ticks;
        }
    }

    void run_batch(uint32_t budget) {
        if (!state || !status || status->state != RUNNING) return;
        const uint32_t completed = mipsel_emu_run_steps(state, budget);
        status->ticks += completed;
    }

    bool running() const { return status && status->state == RUNNING; }
    const QByteArray &uart_text() const { return uart; }
    QString exception_text() const { return exceptions.join(QLatin1Char('\n')); }
    const QByteArray &monitor_text() const { return monitor_capture; }
    mipsel_console_result_t execute_monitor(const QByteArray &line) {
        monitor_capture.clear();
        if (!monitor_initialized) return MIPSEL_CONSOLE_ERROR;
        return mipsel_console_execute(&monitor, line.constData(),
                                      static_cast<size_t>(line.size()));
    }
    const mipsel_elf_info_t &kernel_info() const { return elf_info; }
    bool snapshot_valid() const { return has_snapshot; }
    uint32_t previous_value(int row) const {
        return row >= 0 && row < previous.size() ? previous[row] : 0;
    }
    void save_snapshot() {
        if (!state) return;
        previous.resize(register_count());
        for (int i = 0; i < previous.size(); ++i) previous[i] = register_value(i);
        has_snapshot = true;
    }
    uint32_t register_value(int row) const {
        if (!state) return 0;
        if (row < 32) return state->gpr[row];
        switch (row) {
        case 32: return state->pc;
        case 33: return state->next_pc;
        case 34: return state->hi;
        case 35: return state->lo;
        case 36: return state->cp0.byname.cp0r9_t.cp0r9_n.Count;
        case 37: return state->cp0.byname.cp0r11_t.cp0r11_n.Compare;
        case 38: return state->cp0.byname.cp0r12_t.cp0r12_n.Status;
        case 39: return state->cp0.byname.cp0r13_t.cp0r13_n.Cause;
        case 40: return state->cp0.byname.cp0r14_t.cp0r14_n.EPC;
        case 41: return state->cp0.byname.cp0r8_t.cp0r8_n.BadVAddr;
        default: return 0;
        }
    }
    static int register_count() { return 42; }

private:
    static void observer_exception(void *opaque, const Registers *state,
                                   uint32_t exc_info, uint8_t exc_code,
                                   VectorClass vector_class) {
        auto *controller = static_cast<GuiController *>(opaque);
        if (controller) controller->record_exception(state, exc_info, exc_code,
                                                       vector_class);
    }

    void record_exception(const Registers *exception_state, uint32_t exc_info,
                          uint8_t exc_code, VectorClass vector_class) {
        if (!exception_state) return;
        const QString bd = CAUSE_BD(exception_state)
            ? QStringLiteral(", BD") : QString();
        const QString line = QStringLiteral(
            "! %1 code=%2 pc=%3 info=%4 EPC=%5 Cause=%6 -> %7 (%8%9)")
            .arg(QString::fromLatin1(gui_exception_name(exc_code)))
            .arg(gui_hex(exc_code, 2))
            .arg(gui_hex(exception_state->pc))
            .arg(gui_hex(exc_info))
            .arg(gui_hex(exception_state->cp0.byname.cp0r14_t.cp0r14_n.EPC))
            .arg(gui_hex(exception_state->cp0.byname.cp0r13_t.cp0r13_n.Cause))
            .arg(gui_hex(exception_state->next_pc))
            .arg(QString::fromLatin1(gui_vector_name(vector_class)))
            .arg(bd);
        constexpr int exception_capacity = 128;
        exceptions.append(line);
        if (exceptions.size() > exception_capacity)
            exceptions.remove(0, exceptions.size() - exception_capacity);
    }

    static bool monitor_halted(void *) {
        return status && status->state == STEPPING && status->steps == 0;
    }

    static bool monitor_bus_read(void *, uint32_t address, unsigned width,
                                 uint32_t *value) {
        return platform_bus_read(address, width, value);
    }

    static bool monitor_bus_write(void *, uint32_t address, unsigned width,
                                  uint32_t value) {
        return platform_bus_write(address, width, value);
    }

    static void monitor_emit(void *opaque, const char *bytes, size_t length) {
        auto *controller = static_cast<GuiController *>(opaque);
        if (controller && bytes && length != 0) controller->monitor_capture.append(bytes, length);
    }

    static void uart_thunk(void *opaque, uint8_t byte) {
        auto *controller = static_cast<GuiController *>(opaque);
        if (!controller) return;
        controller->uart.append(static_cast<char>(byte));
        constexpr int max_uart_bytes = 256 * 1024;
        if (controller->uart.size() > max_uart_bytes)
            controller->uart.remove(0, controller->uart.size() - max_uart_bytes);
    }

    QByteArray uart;
    QByteArray monitor_capture;
    mipsel_emu_observer_t observer{};
    QVector<QString> exceptions;
    QVector<uint32_t> previous;
    bool has_snapshot = false;
    bool monitor_initialized = false;
    mipsel_console_t monitor{};
    mipsel_elf_info_t elf_info{};
};

class FramebufferView : public QWidget {
public:
    explicit FramebufferView(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumSize(320, 240);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    QSize sizeHint() const override { return QSize(640, 480); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
        const uchar *pixels = platform_framebuffer_data();
        if (!pixels) return;
        QImage image(pixels, static_cast<int>(platform_framebuffer_width()),
                     static_cast<int>(platform_framebuffer_height()),
                     static_cast<int>(platform_framebuffer_stride_bytes()),
                     QImage::Format_RGB16);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(rect(), image);
    }
};

class MainWindow : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle(QStringLiteral("mipsel-emu Qt6 monitor"));
        resize(1280, 820);
        build_ui();
        timer.setInterval(16);
        QObject::connect(&timer, &QTimer::timeout, this, [this] { tick(); });
        timer.start();
        refresh_views();
    }

    ~MainWindow() override {
#if MIPSEL_EMU_QT_HAVE_SDL
        if (sdl_display) sdl_display_destroy(sdl_display);
#endif
    }

private:
    void build_ui() {
        auto *central = new QWidget(this);
        auto *root = new QVBoxLayout(central);
        auto *files = new QGroupBox(QStringLiteral("Guest images"), central);
        auto *file_form = new QFormLayout(files);
        kernel_edit = add_file_row(file_form, QStringLiteral("vmlinuz / kernel ELF"),
                                   QStringLiteral("./vmlinuz"), false);
        dtb_edit = add_file_row(file_form, QStringLiteral("Device tree (optional)"),
                                QString(), true);
        initramfs_edit = add_file_row(file_form, QStringLiteral("Initramfs (optional)"),
                                      QString(), true);
        root->addWidget(files);

        auto *toolbar = new QHBoxLayout;
        load_button = new QPushButton(QStringLiteral("Load"), central);
        reset_button = new QPushButton(QStringLiteral("Reset"), central);
        run_button = new QPushButton(QStringLiteral("Run"), central);
        pause_button = new QPushButton(QStringLiteral("Pause"), central);
        step_button = new QPushButton(QStringLiteral("Step"), central);
        step_many_button = new QPushButton(QStringLiteral("Step 100"), central);
        clear_uart_button = new QPushButton(QStringLiteral("Clear UART"), central);
        toolbar->addWidget(load_button);
        toolbar->addWidget(reset_button);
        toolbar->addWidget(run_button);
        toolbar->addWidget(pause_button);
        toolbar->addWidget(step_button);
        toolbar->addWidget(step_many_button);
        toolbar->addStretch();
        toolbar->addWidget(clear_uart_button);
        root->addLayout(toolbar);

        auto *split = new QSplitter(Qt::Horizontal, central);
        registers = new QTableWidget(split);
        registers->setColumnCount(3);
        registers->setHorizontalHeaderLabels({QStringLiteral("Register"),
                                              QStringLiteral("Value"),
                                              QStringLiteral("Previous")});
        registers->setRowCount(GuiController::register_count());
        registers->verticalHeader()->setVisible(false);
        registers->horizontalHeader()->setStretchLastSection(true);
        registers->setEditTriggers(QAbstractItemView::NoEditTriggers);
        const QStringList names = register_names();
        for (int row = 0; row < names.size(); ++row)
            registers->setItem(row, 0, new QTableWidgetItem(names[row]));

        tabs = new QTabWidget(split);
        framebuffer = new FramebufferView(tabs);
        tabs->addTab(framebuffer, QStringLiteral("SDL / Framebuffer"));
        uart_view = new QPlainTextEdit(tabs);
        uart_view->setReadOnly(true);
        uart_view->setLineWrapMode(QPlainTextEdit::NoWrap);
        tabs->addTab(uart_view, QStringLiteral("UART"));
        exceptions_view = new QPlainTextEdit(tabs);
        exceptions_view->setReadOnly(true);
        exceptions_view->setLineWrapMode(QPlainTextEdit::NoWrap);
        exceptions_view->setStyleSheet(QStringLiteral("QPlainTextEdit { color: #c62828; }"));
        tabs->addTab(exceptions_view, QStringLiteral("Exceptions"));
        memory_map = new QTreeWidget(tabs);
        memory_map->setHeaderLabels({QStringLiteral("Region"), QStringLiteral("Start"),
                                     QStringLiteral("End"), QStringLiteral("Size"),
                                     QStringLiteral("Type")});
        memory_map->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        tabs->addTab(memory_map, QStringLiteral("Memory map"));

        auto *monitor = new QWidget(tabs);
        auto *monitor_layout = new QVBoxLayout(monitor);
        monitor_output = new QPlainTextEdit(monitor);
        monitor_output->setReadOnly(true);
        monitor_input = new QLineEdit(monitor);
        monitor_input->setPlaceholderText(QStringLiteral("help, tlb [index], translate <va>, reg, mrw <pa>, disasm"));
        monitor_layout->addWidget(monitor_output);
        monitor_layout->addWidget(monitor_input);
        tabs->addTab(monitor, QStringLiteral("Monitor"));
        split->setStretchFactor(0, 0);
        split->setStretchFactor(1, 1);
        split->setSizes({390, 820});
        root->addWidget(split, 1);
        setCentralWidget(central);

        QObject::connect(load_button, &QPushButton::clicked, this, [this] { load_reset(); });
        QObject::connect(reset_button, &QPushButton::clicked, this, [this] { load_reset(); });
        QObject::connect(run_button, &QPushButton::clicked, this, [this] {
            controller.resume();
            update_status();
        });
        QObject::connect(pause_button, &QPushButton::clicked, this, [this] {
            controller.pause();
            refresh_views();
        });
        QObject::connect(step_button, &QPushButton::clicked, this, [this] {
            controller.step();
            refresh_views();
        });
        QObject::connect(step_many_button, &QPushButton::clicked, this, [this] {
            controller.step(100);
            refresh_views();
        });
        QObject::connect(clear_uart_button, &QPushButton::clicked, this, [this] {
            uart_view->clear();
        });
        QObject::connect(monitor_input, &QLineEdit::returnPressed, this, [this] {
            const QString command = monitor_input->text();
            monitor_input->clear();
            execute_monitor(command);
        });
        statusBar()->showMessage(QStringLiteral("No guest loaded"));
#if MIPSEL_EMU_QT_HAVE_SDL
        sdl_display = sdl_display_create_with_framebuffer(
            "mipsel-emu SDL framebuffer",
            reinterpret_cast<pixel_t *>(platform_framebuffer_data()),
            static_cast<int>(platform_framebuffer_width()),
            static_cast<int>(platform_framebuffer_height()),
            static_cast<int>(platform_framebuffer_stride_bytes() / sizeof(pixel_t)));
        if (!sdl_display)
            statusBar()->showMessage(QStringLiteral("SDL mirror unavailable; using Qt framebuffer"));
#endif
    }

    QLineEdit *add_file_row(QFormLayout *form, const QString &label,
                            const QString &value, bool optional) {
        auto *row = new QWidget(form->parentWidget());
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *edit = new QLineEdit(value, row);
        if (optional) edit->setPlaceholderText(QStringLiteral("optional"));
        auto *browse = new QPushButton(QStringLiteral("Browse..."), row);
        layout->addWidget(edit, 1);
        layout->addWidget(browse);
        form->addRow(label, row);
        QObject::connect(browse, &QPushButton::clicked, this, [this, edit] {
            const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Select guest image"),
                                                               edit->text());
            if (!path.isEmpty()) edit->setText(path);
        });
        return edit;
    }

    QStringList register_names() const {
        QStringList names;
        for (const char *name : gpr_names)
            names << QStringLiteral("$%1").arg(QString::fromLatin1(name));
        names << QStringLiteral("pc") << QStringLiteral("next_pc")
              << QStringLiteral("hi") << QStringLiteral("lo")
              << QStringLiteral("CP0.Count") << QStringLiteral("CP0.Compare")
              << QStringLiteral("CP0.Status") << QStringLiteral("CP0.Cause")
              << QStringLiteral("CP0.EPC") << QStringLiteral("CP0.BadVAddr");
        return names;
    }

    void load_reset() {
        QString error;
        if (kernel_edit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Missing kernel"),
                                 QStringLiteral("Choose a vmlinuz or MIPS ELF image."));
            return;
        }
        if (!controller.reset(kernel_edit->text().trimmed(), dtb_edit->text().trimmed(),
                              initramfs_edit->text().trimmed(), &error)) {
            monitor_output->appendPlainText(QStringLiteral("reset: %1").arg(error));
            statusBar()->showMessage(error);
            QMessageBox::critical(this, QStringLiteral("Guest reset failed"), error);
            return;
        }
        monitor_output->appendPlainText(QStringLiteral("guest reset at PC %1")
                                        .arg(hex(controller.register_value(32))));
        rebuild_memory_map();
#if MIPSEL_EMU_QT_HAVE_SDL
        if (sdl_display) {
            sdl_display_mark_dirty(
                sdl_display,
                UiRect{0, 0, static_cast<int>(platform_framebuffer_width()),
                       static_cast<int>(platform_framebuffer_height())});
        }
#endif
        refresh_views();
    }

    void tick() {
        controller.run_batch(4000);
        refresh_views();
#if MIPSEL_EMU_QT_HAVE_SDL
        service_sdl();
#endif
    }

    static QString hex(uint32_t value) {
        return QStringLiteral("0x%1").arg(value, 8, 16, QLatin1Char('0'));
    }

    void refresh_views() {
        for (int row = 0; row < GuiController::register_count(); ++row) {
            const uint32_t value = controller.register_value(row);
            auto *value_item = registers->item(row, 1);
            if (!value_item) {
                value_item = new QTableWidgetItem;
                registers->setItem(row, 1, value_item);
            }
            value_item->setText(hex(value));
            auto *previous_item = registers->item(row, 2);
            if (!previous_item) {
                previous_item = new QTableWidgetItem;
                registers->setItem(row, 2, previous_item);
            }
            previous_item->setText(controller.snapshot_valid()
                                   ? hex(controller.previous_value(row)) : QStringLiteral("-"));
            const bool changed = controller.snapshot_valid() &&
                                  value != controller.previous_value(row);
            const QColor color = changed ? QColor(Qt::red) : palette().text().color();
            value_item->setForeground(color);
            previous_item->setForeground(color);
        }
        const QByteArray &text = controller.uart_text();
        if (uart_view->toPlainText().toUtf8() != text)
            uart_view->setPlainText(QString::fromUtf8(text));
        uart_view->moveCursor(QTextCursor::End);
        const QString exception_text = controller.exception_text();
        if (exceptions_view->toPlainText() != exception_text)
            exceptions_view->setPlainText(exception_text);
        exceptions_view->moveCursor(QTextCursor::End);
        framebuffer->update();
        controller.save_snapshot();
        update_status();
    }

#if MIPSEL_EMU_QT_HAVE_SDL
    void service_sdl() {
        if (!sdl_display) return;
        SdlDisplayEvent event;
        while ((event = sdl_display_poll_event(sdl_display)) != SDL_DISPLAY_EVENT_NONE) {
            if (event == SDL_DISPLAY_EVENT_QUIT) {
                sdl_display_destroy(sdl_display);
                sdl_display = nullptr;
                return;
            }
        }
        platform_framebuffer_rect_t dirty{};
        if (platform_framebuffer_dirty(&dirty)) {
            sdl_display_mark_dirty(sdl_display,
                                   UiRect{static_cast<int>(dirty.x), static_cast<int>(dirty.y),
                                          static_cast<int>(dirty.width), static_cast<int>(dirty.height)});
            platform_framebuffer_clear_dirty();
        }
        (void)sdl_display_present(sdl_display);
    }
#endif

    void update_status() {
        if (!status || !state) {
            statusBar()->showMessage(QStringLiteral("No guest loaded"));
            return;
        }
        const QString mode = controller.running() ? QStringLiteral("RUNNING")
                                                  : QStringLiteral("PAUSED");
        statusBar()->showMessage(QStringLiteral("%1  PC=%2  ticks=%3  RAM=%4 MiB")
                                 .arg(mode, hex(state->pc))
                                 .arg(status->ticks)
                                 .arg(platform_memory_size() / (1024u * 1024u)));
    }

    void rebuild_memory_map() {
        memory_map->clear();
        add_region(QStringLiteral("Guest RAM"), 0, platform_memory_size(), QStringLiteral("RAM"));
        for (size_t i = 0; i < controller.kernel_info().load_segment_count; ++i) {
            const auto &segment = controller.kernel_info().load_segments[i];
            add_region(QStringLiteral("Kernel PT_LOAD %1").arg(i), segment.start,
                       segment.end - segment.start,
                       segment.executable ? QStringLiteral("executable") : QStringLiteral("data"));
        }
        if (!dtb_edit->text().trimmed().isEmpty())
            add_region(QStringLiteral("DTB reservation"), MIPSEL_EMU_DTB_PHYSICAL_ADDRESS,
                       MIPSEL_EMU_DTB_RESERVED_SIZE, QStringLiteral("MMIO/reserved"));
        add_region(QStringLiteral("Framebuffer"), MIPSEL_EMU_FB_MMIO_BASE,
                   platform_framebuffer_size(), QStringLiteral("RGB565 MMIO"));
        add_region(QStringLiteral("UART 16550"), MIPSEL_EMU_UART_MMIO_BASE, 8,
                   QStringLiteral("MMIO"));
    }

    void add_region(const QString &name, uint32_t start, uint32_t size,
                    const QString &kind) {
        auto *item = new QTreeWidgetItem(memory_map);
        item->setText(0, name);
        item->setText(1, hex(start));
        item->setText(2, hex(start + size - 1u));
        item->setText(3, QStringLiteral("%1 bytes").arg(size));
        item->setText(4, kind);
    }

    void execute_monitor(const QString &raw) {
        const QString command_line = raw.trimmed();
        if (command_line.isEmpty()) return;
        monitor_output->appendPlainText(QStringLiteral("> %1").arg(command_line));
        const QStringList parts = command_line.split(QRegularExpression(QStringLiteral("\\s+")),
                                                     Qt::SkipEmptyParts);
        const QString command = parts.value(0).toLower();
        if (command == QStringLiteral("run")) {
            if (state && status) controller.resume();
            else monitor_output->appendPlainText(QStringLiteral("error: no guest loaded"));
        } else if (command == QStringLiteral("pause")) {
            controller.pause();
        } else if (command == QStringLiteral("reset")) {
            load_reset();
        } else if (command == QStringLiteral("status")) {
            update_status();
            monitor_output->appendPlainText(statusBar()->currentMessage());
        } else {
            if (!state || !status) {
                monitor_output->appendPlainText(QStringLiteral("error: no guest loaded"));
            } else {
                const QByteArray input = command_line.toUtf8();
                (void)controller.execute_monitor(input);
                const QByteArray &response = controller.monitor_text();
                if (!response.isEmpty())
                    monitor_output->insertPlainText(QString::fromUtf8(response));
            }
        }
        monitor_output->moveCursor(QTextCursor::End);
        refresh_views();
    }

    GuiController controller;
    QTimer timer;
    QLineEdit *kernel_edit = nullptr;
    QLineEdit *dtb_edit = nullptr;
    QLineEdit *initramfs_edit = nullptr;
    QPushButton *load_button = nullptr;
    QPushButton *reset_button = nullptr;
    QPushButton *run_button = nullptr;
    QPushButton *pause_button = nullptr;
    QPushButton *step_button = nullptr;
    QPushButton *step_many_button = nullptr;
    QPushButton *clear_uart_button = nullptr;
    QTableWidget *registers = nullptr;
    QTabWidget *tabs = nullptr;
    FramebufferView *framebuffer = nullptr;
    QPlainTextEdit *uart_view = nullptr;
    QPlainTextEdit *exceptions_view = nullptr;
    QTreeWidget *memory_map = nullptr;
    QPlainTextEdit *monitor_output = nullptr;
    QLineEdit *monitor_input = nullptr;
#if MIPSEL_EMU_QT_HAVE_SDL
    SdlDisplay *sdl_display = nullptr;
#endif
};

} // namespace

int main(int argc, char **argv) {
    if (argc > 1) return mipsel_emu_cli_main(argc, argv);
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
