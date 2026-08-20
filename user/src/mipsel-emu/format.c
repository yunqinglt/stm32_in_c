#include "format.h"

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *buf;
    size_t size;
    size_t length;
} FormatOutput;

static void output_char(FormatOutput *output, char value) {
    if (output->buf && output->size != 0u &&
        output->length < output->size - 1u) {
        output->buf[output->length] = value;
    }
    if (output->length != SIZE_MAX) ++output->length;
}

static void output_repeat(FormatOutput *output, char value, size_t count) {
    size_t stored = 0;

    if (output->buf && output->size != 0u &&
        output->length < output->size - 1u) {
        size_t available = output->size - 1u - output->length;
        stored = count < available ? count : available;
        while (stored != 0u) {
            output->buf[output->length++] = value;
            --stored;
            --count;
        }
    }
    if (SIZE_MAX - output->length < count) {
        output->length = SIZE_MAX;
    } else {
        output->length += count;
    }
}

static void output_string(FormatOutput *output, const char *value) {
    if (!value) value = "(null)";
    while (*value != '\0') output_char(output, *value++);
}

static size_t unsigned_digits(uint32_t value, unsigned int base,
                              char digits[32]) {
    static const char alphabet[] = "0123456789abcdef";
    size_t count = 0;

    do {
        digits[count++] = alphabet[value % base];
        value /= base;
    } while (value != 0u);
    return count;
}

static void output_unsigned(FormatOutput *output, uint32_t value,
                            unsigned int base, size_t width,
                            char padding) {
    char digits[32];
    size_t count = unsigned_digits(value, base, digits);

    if (width > count) output_repeat(output, padding, width - count);
    while (count != 0u) output_char(output, digits[--count]);
}

static void output_signed(FormatOutput *output, int value, size_t width,
                          char padding) {
    uint32_t magnitude;
    bool negative = value < 0;
    char digits[32];
    size_t count;
    size_t total;

    /* Unsigned subtraction avoids overflowing when VALUE is INT_MIN. */
    magnitude = negative ? 0u - (uint32_t)value : (uint32_t)value;
    count = unsigned_digits(magnitude, 10u, digits);
    total = count + (negative ? 1u : 0u);

    if (padding == '0' && negative) output_char(output, '-');
    if (width > total) output_repeat(output, padding, width - total);
    if (padding != '0' && negative) output_char(output, '-');
    while (count != 0u) output_char(output, digits[--count]);
}

static size_t parse_width(const char **format) {
    size_t width = 0;

    while (**format >= '0' && **format <= '9') {
        unsigned int digit = (unsigned int)(**format - '0');
        if (width > (SIZE_MAX - digit) / 10u) {
            width = SIZE_MAX;
        } else {
            width = width * 10u + digit;
        }
        ++*format;
    }
    return width;
}

int mipsel_snprintf(char *buf, size_t buf_size, const char *format, ...) {
    FormatOutput output = {
        .buf = buf,
        .size = buf_size,
        .length = 0,
    };
    va_list args;

    if (!format) {
        if (buf && buf_size != 0u) buf[0] = '\0';
        return 0;
    }

    va_start(args, format);
    while (*format != '\0') {
        bool zero_padding;
        size_t width;
        char conversion;

        if (*format != '%') {
            output_char(&output, *format++);
            continue;
        }

        ++format;
        if (*format == '%') {
            output_char(&output, '%');
            ++format;
            continue;
        }

        zero_padding = *format == '0';
        if (zero_padding) ++format;
        width = parse_width(&format);
        conversion = *format;
        if (conversion == '\0') {
            output_char(&output, '%');
            break;
        }
        ++format;

        switch (conversion) {
            case 's': {
                const char *value = va_arg(args, const char *);
                size_t length = 0;

                if (!value) value = "(null)";
                while (value[length] != '\0') ++length;
                if (width > length) {
                    output_repeat(&output, zero_padding ? '0' : ' ',
                                  width - length);
                }
                output_string(&output, value);
                break;
            }
            case 'd':
                output_signed(&output, va_arg(args, int), width,
                              zero_padding ? '0' : ' ');
                break;
            case 'u':
                output_unsigned(&output, va_arg(args, unsigned int), 10u,
                                width, zero_padding ? '0' : ' ');
                break;
            case 'x':
                output_unsigned(&output, va_arg(args, unsigned int), 16u,
                                width, zero_padding ? '0' : ' ');
                break;
            default:
                /* Preserve an unsupported directive visibly.  Internal
                 * callers only use the documented subset. */
                output_char(&output, '%');
                output_char(&output, conversion);
                break;
        }
    }
    va_end(args);

    if (buf && buf_size != 0u) {
        size_t terminator = output.length < buf_size
                                ? output.length : buf_size - 1u;
        buf[terminator] = '\0';
    }
    return output.length > (size_t)INT_MAX ? INT_MAX : (int)output.length;
}
