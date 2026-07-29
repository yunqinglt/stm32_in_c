#ifndef _REGISTER_H
#define _REGISTER_H

#include "compiler.h"
#include <stdint.h>

#define r32     uint32_t
typedef void (*MIPS_Instruction_Handler) (uint32_t instr, Registers *state);

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
            union { r32 cp0r0[8];  struct { r32 Index; r32 MVPControl; r32 MVPConf0; r32 MVPConf1; r32 _pad0[4]; } cp0r0_n; } cp0r0_t;
            union { r32 cp0r1[8];  struct { r32 Random; r32 VPEControl; r32 VPEConf0; r32 VPEConf1; r32 YQMask; r32 VPESchedule; r32 VPEScheBack; r32 VPEOpt; } cp0r1_n; } cp0r1_t;
            union { r32 cp0r2[8];  struct { r32 EntryLo0; r32 TCStatus; r32 TCBind; r32 TCRestart; r32 TCHalt; r32 TCContext; r32 TCSchedule; r32 TCSchFBack; } cp0r2_n; } cp0r2_t;
            union { r32 cp0r3[8];  struct { r32 EntryLo1; r32 _pad3[7]; } cp0r3_n; } cp0r3_t;
            union { r32 cp0r4[8];  struct { r32 Context; r32 ContextConfig; r32 _pad4[6]; } cp0r4_n; } cp0r4_t;
            union { r32 cp0r5[8];  struct { r32 PageMask; r32 PageGrain; r32 _pad5[6]; } cp0r5_n; } cp0r5_t;
            union { r32 cp0r6[8];  struct { r32 Wired; r32 SRSConf0; r32 SRSConf1; r32 SRSConf2; r32 SRSConf3; r32 SRSConf4; r32 _pad6[2]; } cp0r6_n; } cp0r6_t;
            union { r32 cp0r7[8];  struct { r32 HWREna; r32 _pad7[7]; } cp0r7_n; } cp0r7_t;
            union { r32 cp0r8[8];  struct { r32 BadVAddr; r32 _pad8[7]; } cp0r8_n; } cp0r8_t;
            union { r32 cp0r9[8];  struct { r32 Count; r32 _pad9[7]; } cp0r9_n; } cp0r9_t;
            union { r32 cp0r10[8]; struct { r32 EntryHi; r32 _pad10[7]; } cp0r10_n; } cp0r10_t;
            union { r32 cp0r11[8]; struct { r32 Compare; r32 _pad11[7]; } cp0r11_n; } cp0r11_t;
            union { r32 cp0r12[8]; struct { r32 Status; r32 IntCtl; r32 SRSCtl; r32 SRSMap; r32 _pad12[4]; } cp0r12_n; } cp0r12_t;
            union { r32 cp0r13[8]; struct { r32 Cause; r32 _pad13[7]; } cp0r13_n; } cp0r13_t;
            union { r32 cp0r14[8]; struct { r32 EPC; r32 _pad14[7]; } cp0r14_n; } cp0r14_t;
            union { r32 cp0r15[8]; struct { r32 PRId; r32 EBase; r32 CDMMBase; r32 _pad15[5]; } cp0r15_n; } cp0r15_t;
            union { r32 cp0r16[8]; struct { r32 Config; r32 Config1; r32 Config2; r32 Config3; r32 Config4; r32 Config5; r32 _pad16[2]; } cp0r16_n; } cp0r16_t;
            union { r32 cp0r17[8]; struct { r32 LLAddr; r32 _pad17[7]; } cp0r17_n; } cp0r17_t;
            union { r32 cp0r18[8]; struct { r32 WatchLo[8]; } cp0r18_n; } cp0r18_t;
            union { r32 cp0r19[8]; struct { r32 WatchHi[8]; } cp0r19_n; } cp0r19_t;
            union { r32 cp0r20[8]; struct { r32 XContext; r32 _pad20[7]; } cp0r20_n; } cp0r20_t;
            union { r32 cp0r21[8]; struct { r32 _pad21[8]; } cp0r21_n; } cp0r21_t;
            union { r32 cp0r22[8]; struct { r32 _pad22[8]; } cp0r22_n; } cp0r22_t;
            union { r32 cp0r23[8]; struct { r32 Debug; r32 TraceControl; r32 TraceControl2; r32 UserTraceData; r32 TraceBPC; r32 Debug2; r32 _pad23[2]; } cp0r23_n; } cp0r23_t;
            union { r32 cp0r24[8]; struct { r32 DEPC; r32 _pad24[7]; } cp0r24_n; } cp0r24_t;
            union { r32 cp0r25[8]; struct { r32 PerfCnt[8]; } cp0r25_n; } cp0r25_t;
            union { r32 cp0r26[8]; struct { r32 ErrCtl; r32 _pad26[7]; } cp0r26_n; } cp0r26_t;
            union { r32 cp0r27[8]; struct { r32 CacheErr[4]; r32 _pad27[4]; } cp0r27_n; } cp0r27_t;
            union { r32 cp0r28[8]; struct { r32 TagLo; r32 DataLo; r32 _pad28[6]; } cp0r28_n; } cp0r28_t;
            union { r32 cp0r29[8]; struct { r32 TagHi; r32 DataHi; r32 _pad29[6]; } cp0r29_n; } cp0r29_t;
            union { r32 cp0r30[8]; struct { r32 ErrorEPC; r32 _pad30[7]; } cp0r30_n; } cp0r30_t;
            union { r32 cp0r31[8]; struct { r32 DESAVE; r32 _pad31[7]; } cp0r31_n; } cp0r31_t;
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

// Behavior for Status (cp0r12 Select 0)
#define CU30(state)     0x01 // Only CP0 usable
#define RP(state)       (state->cp0[12][0] & 0x08000000)
#define FR(state)       (state->cp0[12][0] & 0x04000000)

#endif