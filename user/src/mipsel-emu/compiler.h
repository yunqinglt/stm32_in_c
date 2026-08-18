#ifndef _COMPILER_H
#define _COMPILER_H

#include <stdint.h>

// Symbol config
#define __WEAK  __attribute__((weak))
#define __ALIAS(ALIAS)    __attribute__((weak, alias(ALIAS)))
#define __NAKED __attribute__((naked))
#define __REWRITE   // Just tell that I'm rewriting a weak function
#define __WARNING(msg) __attribute__((deprecated(msg)))

// Memory aligning
#define __ALIGN_4   __attribute__((aligned(4)))
#define __ALIGN_8   __attribute__((aligned(8)))
#define __PACKED    __attribute__((packed))

// Function behavior
#define __STATIC    static
#define __INLINE    inline
#define __STATIC_INLINE     static inline
#define __STATIC_FORCEINLINE    __attribute__((always_inline)) __STATIC_INLINE
#define __USED      __attribute__((used))

// Variable behavior
#define     __O     volatile
#define     __IO    volatile

#define     __IM     volatile const
#define     __OM     volatile
#define     __IOM    volatile

#define __CONCAT_INTERNAL(a, b) a##b
#define __CONCAT(a, b) __CONCAT_INTERNAL(a, b)
#define __reserved(n, x) uint8_t __CONCAT(__reserved_, n)[x]

#define __BIT_CONCAT_INTERNAL(a, b) a##b
#define __BIT_CONCAT(a, b)  __BIT_CONCAT_INTERNAL(a, b)

#define __reserved_bit(n, x)    uint32_t __BIT_CONCAT(__reserved_bit_, n) : x

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
    return (Result) {Ok, .value.ok = val};
}

__STATIC_FORCEINLINE Result ERR(uint32_t val) {
    return (Result) {Err, .value.reason = val};
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