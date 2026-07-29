#ifndef _CONFIG_H
#define _CONFIG_H

/*
 * ===========================================================================
 * MIPS Emulator Configuration
 * ===========================================================================
 * This file contains macros to control the compilation options for the emulator.
 */

/*
 * EMU_TARGET_MODE:
 * 1 = Full Emulator (Desktop/Server environment)
 *     Supports full TLB simulation, caching behaviors, full logging, and 
 *     potentially a GDB stub for debugging.
 * 0 = Embedded-specific Emulator (e.g. running on an STM32 MCU)
 *     Optimized for constrained resources. Uses simplified memory mapping 
 *     or lightweight TLB, removes heavy logging, and strips down debugging features.
 */
#ifndef EMU_TARGET_MODE
#define EMU_TARGET_MODE 1
#endif

#if EMU_TARGET_MODE == 1
    #define CFG_ENABLE_FULL_TLB      1
    #define CFG_ENABLE_DEBUG_LOG     1
    #define CFG_ENABLE_GDB_STUB      1
    #define CFG_ENABLE_FPU           1
#else
    #define CFG_ENABLE_FULL_TLB      0
    #define CFG_ENABLE_DEBUG_LOG     0
    #define CFG_ENABLE_GDB_STUB      0
    #define CFG_ENABLE_FPU           0
#endif

#endif /* _CONFIG_H */
