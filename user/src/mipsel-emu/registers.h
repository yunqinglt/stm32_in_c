#ifndef _REGISTER_H
#define _REGISTER_H

#include "compiler.h"
#include <stdint.h>

typedef uint32_t    r32;
typedef void (*MIPS_Instruction_Handler) (uint32_t instr, Registers *state);

typedef enum {
    RESET,
    RUNNING,
    STEPPING,
    HALTING,
} cpu_state;

typedef struct {
    cpu_state state;
    // This is struct pointer for data essential in any state
    Registers *cpu_ctx;
    uint32_t steps;
    uint64_t clock; // clock rate
} vmstate_t;

typedef struct Registers_t {
    r32 gpr[32]; // shadow register set
    r32 pc;
    r32 next_pc; // flush pc at the tail of loop
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
            union { r32 cp0r16[8]; struct { r32 Config; r32 Config1; r32 Config2; r32 Config3; r32 Config4; r32 Config5; r32 _pad16[2]; } cp0r16_n; } cp0r16_t; // R-only
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


#define GET_BITFIELD(reg, pos, len) \
    (((reg) >> (pos)) & ((1U << (len)) - 1))

#define SET_BITFIELD(reg, pos, len, val) \
    (((reg) & ~(((1U << (len)) - 1) << (pos))) | (((val) & ((1U << (len)) - 1)) << (pos)))

// CP0 Register 12 (Status) Bitfield Definitions
#define CP0_STATUS_CU_POS       28
#define CP0_STATUS_CU_LEN       4
#define CP0_STATUS_RP_POS       27
#define CP0_STATUS_RP_LEN       1
#define CP0_STATUS_FR_POS       26
#define CP0_STATUS_FR_LEN       1
#define CP0_STATUS_RE_POS       25
#define CP0_STATUS_RE_LEN       1
#define CP0_STATUS_MX_POS       24
#define CP0_STATUS_MX_LEN       1
#define CP0_STATUS_PX_POS       23
#define CP0_STATUS_PX_LEN       1
#define CP0_STATUS_BEV_POS      22
#define CP0_STATUS_BEV_LEN      1
#define CP0_STATUS_TS_POS       21
#define CP0_STATUS_TS_LEN       1
#define CP0_STATUS_SR_POS       20
#define CP0_STATUS_SR_LEN       1
#define CP0_STATUS_NMI_POS      19
#define CP0_STATUS_NMI_LEN      1
#define CP0_STATUS_IM_POS       8
#define CP0_STATUS_IM_LEN       8
#define CP0_STATUS_KX_POS       7
#define CP0_STATUS_KX_LEN       1
#define CP0_STATUS_SX_POS       6
#define CP0_STATUS_SX_LEN       1
#define CP0_STATUS_UX_POS       5
#define CP0_STATUS_UX_LEN       1
#define CP0_STATUS_KSU_POS      3
#define CP0_STATUS_KSU_LEN      2
#define CP0_STATUS_ERL_POS      2
#define CP0_STATUS_ERL_LEN      1
#define CP0_STATUS_EXL_POS      1
#define CP0_STATUS_EXL_LEN      1
#define CP0_STATUS_IE_POS       0
#define CP0_STATUS_IE_LEN       1

#define STATUS_BEV(state)  GET_BITFIELD((state)->cp0.byname.cp0r12_t.cp0r12_n.Status, CP0_STATUS_BEV_POS, CP0_STATUS_BEV_LEN)
#define STATUS_IM(state)   GET_BITFIELD((state)->cp0.byname.cp0r12_t.cp0r12_n.Status, CP0_STATUS_IM_POS, CP0_STATUS_IM_LEN)
#define STATUS_KSU(state)  GET_BITFIELD((state)->cp0.byname.cp0r12_t.cp0r12_n.Status, CP0_STATUS_KSU_POS, CP0_STATUS_KSU_LEN)
#define STATUS_ERL(state)  GET_BITFIELD((state)->cp0.byname.cp0r12_t.cp0r12_n.Status, CP0_STATUS_ERL_POS, CP0_STATUS_ERL_LEN)
#define STATUS_EXL(state)  GET_BITFIELD((state)->cp0.byname.cp0r12_t.cp0r12_n.Status, CP0_STATUS_EXL_POS, CP0_STATUS_EXL_LEN)
#define STATUS_IE(state)   GET_BITFIELD((state)->cp0.byname.cp0r12_t.cp0r12_n.Status, CP0_STATUS_IE_POS, CP0_STATUS_IE_LEN)

#define GET_CPU_MODE(state) \
    ((STATUS_ERL(state) || STATUS_EXL(state)) ? KERNEL : \
    ((STATUS_KSU(state) == 0x2) ? USER : KERNEL))


// CP0 Register 13 (Cause) Bitfield Definitions
#define CP0_CAUSE_BD_POS        31
#define CP0_CAUSE_BD_LEN        1
#define CP0_CAUSE_TI_POS        30
#define CP0_CAUSE_TI_LEN        1
#define CP0_CAUSE_CE_POS        28
#define CP0_CAUSE_CE_LEN        2
#define CP0_CAUSE_DC_POS        27
#define CP0_CAUSE_DC_LEN        1
#define CP0_CAUSE_PCI_POS       26
#define CP0_CAUSE_PCI_LEN       1
#define CP0_CAUSE_IV_POS        23
#define CP0_CAUSE_IV_LEN        1
#define CP0_CAUSE_WP_POS        22
#define CP0_CAUSE_WP_LEN        1
#define CP0_CAUSE_IP_POS        8
#define CP0_CAUSE_IP_LEN        8
#define CP0_CAUSE_RIPL_POS      10
#define CP0_CAUSE_RIPL_LEN      6
#define CP0_CAUSE_EXCCODE_POS   2
#define CP0_CAUSE_EXCCODE_LEN   5

#define CAUSE_BD(state)         GET_BITFIELD((state)->cp0.byname.cp0r13_t.cp0r13_n.Cause, CP0_CAUSE_BD_POS, CP0_CAUSE_BD_LEN)
#define CAUSE_IP(state)         GET_BITFIELD((state)->cp0.byname.cp0r13_t.cp0r13_n.Cause, CP0_CAUSE_IP_POS, CP0_CAUSE_IP_LEN)
#define CAUSE_RIPL(state)       GET_BITFIELD((state)->cp0.byname.cp0r13_t.cp0r13_n.Cause, CP0_CAUSE_RIPL_POS, CP0_CAUSE_RIPL_LEN)
#define CAUSE_EXCCODE(state)    GET_BITFIELD((state)->cp0.byname.cp0r13_t.cp0r13_n.Cause, CP0_CAUSE_EXCCODE_POS, CP0_CAUSE_EXCCODE_LEN)

// CP0 Register 12 Select 2 (SRSCtl) Bitfield Definitions
#define CP0_SRSCTL_HSS_POS      26
#define CP0_SRSCTL_HSS_LEN      4
#define CP0_SRSCTL_EICSS_POS    18
#define CP0_SRSCTL_EICSS_LEN    4
#define CP0_SRSCTL_ESS_POS      14
#define CP0_SRSCTL_ESS_LEN      4
#define CP0_SRSCTL_PSS_POS      6
#define CP0_SRSCTL_PSS_LEN      4
#define CP0_SRSCTL_CSS_POS      0
#define CP0_SRSCTL_CSS_LEN      4

// CP0 Register 4 Select 0 (Context)
#define CP0_CONTEXT_PTEB_POS    23
#define CP0_CONTEXT_PTEB_LEN    8
#define CP0_CONTEXT_BVPN2_POS   4
#define CP0_CONTEXT_BVPN2_LEN   19


#define SRSCTL_CSS(state)       GET_BITFIELD((state)->cp0.byname.cp0r12_t.cp0r12_n.SRSCtl, CP0_SRSCTL_CSS_POS, CP0_SRSCTL_CSS_LEN)

#define INIT_STATUS     0x10400004
#define INIT_RANDOM     0x3f

#define PRID_OPT        ((uint8_t) "Y" << 24)

__STATIC_FORCEINLINE uint32_t pfn_translate(uint32_t target, Registers *state);
__STATIC_FORCEINLINE void trigger_exception_helper(uint32_t exc, Registers *state, uint32_t exc_info);

#endif