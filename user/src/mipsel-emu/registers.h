#ifndef _REGISTER_H
#define _REGISTER_H

#include "compiler.h"
#include <stdint.h>

#define r32     uint32_t

typedef enum {
    RUNNING,
    STEPPING,
    HALTING,
} vm_state;

typedef struct {
    vm_state state;
    union {
        uint32_t u_ud;
        void *p_ud;
    } userdata;
} vmstate_pointer;

typedef enum {
    USER,
    SUPERVISOR,
    KERNEL,
} CP0_Status;

typedef struct {
    r32 gpr[32];
    r32 pc;

    // uint32_t cp0[32];
    union {
        r32 regs[32][8];
        struct {
            union {
                r32 cp0r0[8];
                struct { 
                    r32 Index;
                    r32 MVPControl;
                    r32 MVPConf0;
                    r32 MVPConf1;
                    r32 _pad0[4]
                } cp0r0_n; // N0
            } cp0r0_t;
            
            union {
                r32 cp0r1[8];
                struct {
                    r32 Random;
                    r32 VPEControl;
                    r32 VPEConf0;
                    r32 VPEConf1;
                    r32 YQMask;
                    r32 VPESchedule;
                    r32 VPEScheBack;
                    r32 VPEOpt;
                } cp0r1_n; // N1
            } cp0r1_t;

            union {
                r32 cp0r2[8];
                struct {
                    r32 EntryLo0;
                    r32 TCStatus;
                    r32 TCBind;
                    r32 TCRestart;
                    r32 TCHalt;
                    r32 TCCOntext;
                    r32 TCSchedule;
                    r32 TCSchFBack;
                } cp0r2_n; // N2
            } cp0r2_t;

            union {
                r32 cp0r3[8];
                struct {
                    r32 EntryLo1;
                    r32 _pad3[7];
                } cp0r3_n; // N3
            } cp0r3_t;

            union {
                r32 cp0r4[8];
                struct {
                    r32 Context;
                    r32 ContextConfig;
                    r32 _pad4[6];
                } cp0r4_n; // N4
            } cp0r4_t;

            union {
                r32 cp0r5[8];
                struct {
                    r32 PageMask;
                    r32 PageGrain;
                    r32 _pad5[6];
                } cp0r5_n; // N5
            } cp0r5_t;

            union {
                r32 cp0r6[8];
                struct {
                    r32 Wired;
                    r32 SRSConf0;
                    r32 SRSConf1;
                    r32 SRSConf2;
                    r32 SRSConf3;
                    r32 SRSConf4;
                    r32 _pad6[2];
                } cp0r6_n;
            } cp0r6_t;

            union {
                r32 cp0r7[8];
                struct {
                    r32 HWREna;
                    r32 _pad7[7];
                } cp0r7_n;
            } cp0r7_t;


        } byname;
    } cp0;

    struct {
        uint32_t entryhi;
        uint32_t entrylo0;
        uint32_t entrylo1;
        uint32_t pmask;
    } tlb[64];

    uint32_t hi;
    uint32_t lo;

} Registers;

#endif