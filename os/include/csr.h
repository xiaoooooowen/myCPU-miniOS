#ifndef MINIOS_CSR_H
#define MINIOS_CSR_H

#include <stdint.h>

/*
 * CSR 地址常量定义
 */

/* 机器模式 CSR */
#define CSR_MHARTID    0xf14
#define CSR_MSTATUS    0x300
#define CSR_MEDELEG    0x302
#define CSR_MIDELEG    0x303
#define CSR_MIE        0x304
#define CSR_MTVEC      0x305
#define CSR_MCOUNTEREN 0x306
#define CSR_MSCRATCH   0x340
#define CSR_MEPC       0x341
#define CSR_MCAUSE     0x342
#define CSR_MTVAL      0x343
#define CSR_MIP        0x344

/* 监管模式 CSR */
#define CSR_SSTATUS    0x100
#define CSR_SIE        0x104
#define CSR_STVEC      0x105
#define CSR_SSCRATCH   0x140
#define CSR_SEPC       0x141
#define CSR_SCAUSE     0x142
#define CSR_STVAL      0x143
#define CSR_SIP        0x144
#define CSR_SATP       0x180

/* mstatus/sstatus 字段位掩码 */
#define MSTATUS_MIE     (1ULL << 3)
#define MSTATUS_MPIE    (1ULL << 7)
#define MSTATUS_MPP_M   (3ULL << 11)
#define MSTATUS_MPP_S   (1ULL << 11)
#define MSTATUS_MPP_U   (0ULL << 11)

#define SSTATUS_SIE     (1ULL << 1)
#define SSTATUS_SPIE    (1ULL << 5)
#define SSTATUS_SPP     (1ULL << 8)

/* mie/sie 中断使能位 */
#define MIE_MSIE        (1ULL << 3)
#define MIE_MTIE        (1ULL << 7)
#define MIE_MEIE        (1ULL << 11)

#define SIE_SSIE        (1ULL << 1)
#define SIE_STIE        (1ULL << 5)
#define SIE_SEIE        (1ULL << 9)

/* mip/sip 中断挂起位 */
#define MIP_MSIP        (1ULL << 3)
#define MIP_MTIP        (1ULL << 7)
#define MIP_MEIP        (1ULL << 11)

#define SIP_SSIP        (1ULL << 1)
#define SIP_STIP        (1ULL << 5)
#define SIP_SEIP        (1ULL << 9)

/* 异常/中断 cause 编码 */
#define CAUSE_ECALL_U   8
#define CAUSE_ECALL_S   9
#define CAUSE_ECALL_M   11
#define CAUSE_TIMER_S   5
#define CAUSE_TIMER_M   7
#define CAUSE_EXT_S     9
#define CAUSE_EXT_M     11

/*
 * 通用 CSR 访问宏（通过内联汇编）
 */

#define _csr_read(csr) ({                           \
    uint64_t _v;                                   \
    __asm__ volatile("csrr %0, " #csr : "=r"(_v)); \
    _v;                                            \
})

#define csr_read(csr) _csr_read(csr)

#define _csr_write(csr, value)                     \
    __asm__ volatile("csrw " #csr ", %0" :: "r"((uint64_t)(value)))

#define csr_write(csr, value) _csr_write(csr, value)

#define _csr_set(csr, mask)                        \
    __asm__ volatile("csrs " #csr ", %0" :: "r"((uint64_t)(mask)))

#define csr_set(csr, mask) _csr_set(csr, mask)

#define _csr_clear(csr, mask)                      \
    __asm__ volatile("csrc " #csr ", %0" :: "r"((uint64_t)(mask)))

#define csr_clear(csr, mask) _csr_clear(csr, mask)

/*
 * 统一的 Trap CSR 访问接口
 *
 * 设计初衷：当前 MiniOS 运行在 M 模式，后续可能切换到 S 模式。
 * 通过 `MINIOS_USE_S_MODE` 宏控制，避免代码散落裸 CSR 名称。
 *
 * 使用方式：
 *   trap_vector_write(handler_addr);   // 代替 csr_write(mtvec, addr) 或 csr_write(stvec, addr)
 *   uint64_t pc = trap_epc_read();     // 代替 csr_read(mepc) 或 csr_read(sepc)
 */

#ifdef MINIOS_USE_S_MODE
#  define TRAP_VEC      stvec
#  define TRAP_EPC      sepc
#  define TRAP_CAUSE    scause
#  define TRAP_TVAL     stval
#  define TRAP_STATUS   sstatus
#  define TRAP_SCRATCH  sscratch
#  define TRAP_IE       sie
#  define TRAP_IP       sip
#else
#  define TRAP_VEC      mtvec
#  define TRAP_EPC      mepc
#  define TRAP_CAUSE    mcause
#  define TRAP_TVAL     mtval
#  define TRAP_STATUS   mstatus
#  define TRAP_SCRATCH  mscratch
#  define TRAP_IE       mie
#  define TRAP_IP       mip
#endif

#define trap_vector_read()    csr_read(TRAP_VEC)
#define trap_vector_write(v)  csr_write(TRAP_VEC, v)
#define trap_epc_read()       csr_read(TRAP_EPC)
#define trap_epc_write(v)     csr_write(TRAP_EPC, v)
#define trap_cause_read()     csr_read(TRAP_CAUSE)
#define trap_cause_write(v)   csr_write(TRAP_CAUSE, v)
#define trap_tval_read()      csr_read(TRAP_TVAL)
#define trap_tval_write(v)    csr_write(TRAP_TVAL, v)
#define trap_status_read()    csr_read(TRAP_STATUS)
#define trap_status_write(v)  csr_write(TRAP_STATUS, v)
#define trap_scratch_read()   csr_read(TRAP_SCRATCH)
#define trap_scratch_write(v) csr_write(TRAP_SCRATCH, v)

#define trap_ie_read()        csr_read(TRAP_IE)
#define trap_ie_write(v)      csr_write(TRAP_IE, v)
#define trap_ip_read()        csr_read(TRAP_IP)
#define trap_ip_write(v)      csr_write(TRAP_IP, v)

/*
 * 中断使能控制
 */

#ifdef MINIOS_USE_S_MODE
#  define STATUS_IE_BIT   SSTATUS_SIE
#  define STATUS_PIE_BIT  SSTATUS_SPIE
#  define STATUS_PP_MASK  SSTATUS_SPP
#else
#  define STATUS_IE_BIT   MSTATUS_MIE
#  define STATUS_PIE_BIT  MSTATUS_MPIE
#  define STATUS_PP_MASK  MSTATUS_MPP_M
#endif

static inline void local_irq_enable(void) {
    csr_set(TRAP_STATUS, STATUS_IE_BIT);
}

static inline void local_irq_disable(void) {
    csr_clear(TRAP_STATUS, STATUS_IE_BIT);
}

static inline uint64_t local_irq_save(void) {
    uint64_t status = trap_status_read();
    local_irq_disable();
    return status;
}

static inline void local_irq_restore(uint64_t status) {
    trap_status_write((trap_status_read() & ~STATUS_IE_BIT) | (status & STATUS_IE_BIT));
}

#endif /* MINIOS_CSR_H */
