.macro ENTRY_ALIGNED name, alignment
    .type \name, #function
    .hidden \name  // Hide this as a global symbol, so we do not incur plt calls.
    .global \name
    .balign \alignment
\name:
    .cfi_startproc
.endm

.macro ENTRY name
    ENTRY_ALIGNED \name, 16
.endm

.macro END name
    .cfi_endproc
    .size \name, .-\name
.endm


.extern ksu_trace_sys_enter

ENTRY ksu_syscall_trace_enter_call_trace_sys_enter_hook_trampoline
stp x29, x30, [sp, #-0x90]!
stp x0,  x1,  [sp, #0x10]
stp x2,  x3,  [sp, #0x20]
stp x4,  x5,  [sp, #0x30]
stp x6,  x7,  [sp, #0x40]
stp x8,  x9,  [sp, #0x50]
stp x10, x11, [sp, #0x60]
stp x12, x13, [sp, #0x70]
stp x14, x15, [sp, #0x80]

bl ksu_trace_sys_enter

ldp x0,  x1,  [sp, #0x10]
ldp x2,  x3,  [sp, #0x20]
ldp x4,  x5,  [sp, #0x30]
ldp x6,  x7,  [sp, #0x40]
ldp x8,  x9,  [sp, #0x50]
ldp x10, x11, [sp, #0x60]
ldp x12, x13, [sp, #0x70]
ldp x14, x15, [sp, #0x80]
ldp x29, x30, [sp], #0x90

// TODO: create hook_t in static memory
.global ksu_syscall_trace_enter_call_trace_sys_enter_hook_trampoline_to_original
ksu_syscall_trace_enter_call_trace_sys_enter_hook_trampoline_to_original:
nop
nop
nop
nop

END ksu_syscall_trace_enter_call_trace_sys_enter_hook_trampoline
