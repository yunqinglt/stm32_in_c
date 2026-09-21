#ifndef _COMPILER_H
#define _COMPILER_H

#include <stdint.h>

/* Keep the small firmware-oriented vocabulary usable by the hosted Windows
 * build too.  MSVC has no C-level weak/alias attributes; code that needs a
 * portable symbol wrapper uses an ordinary function instead. */
#if defined(_MSC_VER)
/* Symbol config */
#define __WEAK
#define __ALIAS(ALIAS)
#define __NAKED
#define __REWRITE
#define __WARNING(msg) __declspec(deprecated(msg))

/* Memory aligning */
#define __ALIGN_2   __declspec(align(2))
#define __ALIGN_4   __declspec(align(4))
#define __ALIGN_8   __declspec(align(8))
#define __PACKED

/* Function behavior */
#define __STATIC    static
#define __INLINE    inline
#define __STATIC_INLINE     static inline
#define __STATIC_FORCEINLINE    static __forceinline
#define __USED
#else
/* Symbol config */
#define __WEAK  __attribute__((weak))
#define __ALIAS(ALIAS)    __attribute__((weak, alias(ALIAS)))
#define __NAKED __attribute__((naked))
#define __REWRITE   // Just tell that I'm rewriting a weak function
#define __WARNING(msg) __attribute__((deprecated(msg)))

/* Memory aligning */
#define __ALIGN_2   __attribute__((aligned(2)))
#define __ALIGN_4   __attribute__((aligned(4)))
#define __ALIGN_8   __attribute__((aligned(8)))
#define __PACKED    __attribute__((packed))

/* Function behavior */
#define __STATIC    static
#define __INLINE    inline
#define __STATIC_INLINE     static inline
#define __STATIC_FORCEINLINE    __attribute__((always_inline)) __STATIC_INLINE
#define __USED      __attribute__((used))
#endif

/* These CMSIS-style names are legacy conveniences and are not used by the
 * emulator itself.  Windows SDK/MinGW headers use __O as an internal
 * identifier, so do not export the aliases into a hosted Windows translation
 * unit where they would rewrite system headers. */
#if !defined(_WIN32) && !defined(_WIN64)
#define     __O     volatile
#define     __IO    volatile

#define     __IM     volatile const
#define     __OM     volatile
#define     __IOM    volatile
#endif

#define MIPSEL_CONCAT_INTERNAL(a, b) a##b
#define MIPSEL_CONCAT(a, b) MIPSEL_CONCAT_INTERNAL(a, b)
#ifndef __reserved
#define __reserved(n, x) uint8_t MIPSEL_CONCAT(__reserved_, n)[x]
#endif

#define __BIT_CONCAT_INTERNAL(a, b) a##b
#define __BIT_CONCAT(a, b)  __BIT_CONCAT_INTERNAL(a, b)

#ifndef __reserved_bit
#define __reserved_bit(n, x)    uint32_t __BIT_CONCAT(__reserved_bit_, n) : x
#endif

// Advanced type
typedef enum {
    Ok,
    Err,
} ResultTag;

typedef struct {
    ResultTag tag;
    union {
        uint32_t ok;
        uint32_t reason;
    } value;
} Result;

__STATIC_FORCEINLINE Result OK(uint32_t val) {
    Result result = {Ok, {0}};
    result.tag = Ok;
    result.value.ok = val;
    return result;
}

__STATIC_FORCEINLINE Result ERR(uint32_t val) {
    Result result = {Err, {0}};
    result.tag = Err;
    result.value.reason = val;
    return result;
}

// repr: Result
#define TEST_RESULT(res) ((res).tag == Ok)

// use rarely
#define UNWRAP(res) \
    { \
        if (TEST_RESULT(res)) return res.value.ok; \
        else return res.value.reason; \
    }


#endif
