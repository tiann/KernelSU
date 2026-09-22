#ifndef KSU_TEST_PTRACE_H
#define KSU_TEST_PTRACE_H
/* A field fixture, NOT a replacement definition of a kernel ABI layout. The
 * module compile separately checks these field names against actual headers.
 */
struct pt_regs {
    unsigned long a0, a1, a2, a3, a4, a5, a7, orig_a0, ra, s0, sp, epc;
    unsigned long regs[31], pc;
    unsigned long di, si, dx, r10, cx, r8, r9, bp, ax, ip, orig_ax;
};
#endif
