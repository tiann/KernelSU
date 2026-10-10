/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Minimal ARM64 bootstrap linked directly into a caller-provided kernel text
 * cave. All ksu_ext_* and ksu_* absolute symbols are supplied by the offline
 * injector. Relative branches/ADRP references remain valid after KASLR. The
 * appended capsule is read through the linear map, not through the kernel image
 * mapping, because it lives beyond the link-time _end.
 *
 * The stub takes the place of the async_synchronize_full() call in
 * kernel_init_freeable(), i.e. it runs after do_basic_setup(): printk and the
 * console are up, so every message below is a plain `ksu_boot_log` call.  The
 * injector resolves printk as an *optional* symbol; a kernel without it yields
 * a zero definition and all messages silently become no-ops.
 */

    .section .text.ksu_bootstrap, "ax"
    .balign 16
    .global ksu_bootstrap
    .type ksu_bootstrap, %function
ksu_bootstrap:
    /* BTI c, encoded as HINT on CPUs without BTI. */
    .inst 0xd503245f
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!

    /* A stub that ran is the first thing a boot log has to show. */
    adrp x0, ksu_hello_message
    add x0, x0, :lo12:ksu_hello_message
    bl ksu_boot_log

    /* Preserve the original kernel_init() call first. */
    bl ksu_ext_async_synchronize_full

    /*
     * x23: capsule, i.e. the linear map alias of the appended capsule:
     * capsule_phys = __pa_symbol(_text) + capsule_image_offset
     * capsule_virt = (capsule_phys - memstart_addr) | PAGE_OFFSET
     */
    bl ksu_capsule_address
    mov x23, x0

    /* Reject a missing/truncated/wrong capsule before touching its module. */
    ldr x8, [x23, #0]
    ldr x9, =ksu_capsule_magic
    cmp x8, x9
    b.ne .Lfail_capsule_magic
    ldr w8, [x23, #8]
    ldr w9, =ksu_capsule_version
    cmp w8, w9
    b.ne .Lfail_capsule_version
    ldr w8, [x23, #12]
    ldr w9, =ksu_capsule_header_size
    cmp w8, w9
    b.ne .Lfail_capsule_header_size
    ldr x8, [x23, #16]
    ldr x9, =ksu_capsule_size
    cmp x8, x9
    b.ne .Lfail_capsule_size
    ldr x8, [x23, #24]
    ldr x9, =ksu_module_capsule_offset
    cmp x8, x9
    b.ne .Lfail_module_offset
    ldr x8, [x23, #32]
    ldr x9, =ksu_module_size
    cmp x8, x9
    b.ne .Lfail_module_size
    ldr x8, [x23, #40]
    ldr x9, =ksu_fixup_capsule_offset
    cmp x8, x9
    b.ne .Lfail_fixup_offset
    ldr x8, [x23, #48]
    ldr x9, =ksu_fixup_count
    cmp x8, x9
    b.ne .Lfail_fixup_count

    adrp x0, ksu_capsule_ok_message
    add x0, x0, :lo12:ksu_capsule_ok_message
    ldr x1, =ksu_module_size
    ldr x2, =ksu_fixup_count
    bl ksu_boot_log

    /* load_module() owns and vfree()s this temporary copy. */
    ldr x0, =ksu_module_size
    bl ksu_ext_vmalloc
    cbz x0, .Lfail_vmalloc
    // x19: vmalloc-ed ksu module
    mov x19, x0

    ldr x8, =ksu_module_capsule_offset
    add x1, x23, x8
    ldr x2, =ksu_module_size
    bl ksu_ext_memcpy

    /* Apply compact { Elf64_Sym file offset, kernel image offset } fixups. */
    ldr x8, =ksu_fixup_capsule_offset
    add x20, x23, x8
    ldr x21, =ksu_fixup_count
    adrp x22, ksu_image_base
    add x22, x22, :lo12:ksu_image_base
    cbz x21, .Lfixups_done
.Lfixup_loop:
    ldr w8, [x20]
    ldr x9, [x20, #8]
    add x10, x19, x8
    mov w11, #0xfff1              /* SHN_ABS */
    strh w11, [x10, #6]           /* Elf64_Sym.st_shndx */
    add x9, x22, x9
    str x9, [x10, #8]             /* Elf64_Sym.st_value */
    add x20, x20, #16
    subs x21, x21, #1
    b.ne .Lfixup_loop
.Lfixups_done:

    /* Oversized, zeroed target_load_info storage; only hdr/len are seeded. */
    ldr x20, =ksu_load_info_size
    sub sp, sp, x20
    mov x21, sp
    mov x8, x20
.Lzero_load_info:
    stp xzr, xzr, [x21], #16
    subs x8, x8, #16
    b.ne .Lzero_load_info

    ldr x9, =ksu_load_info_hdr_offset
    str x19, [sp, x9]
    ldr x8, =ksu_module_size
    ldr x9, =ksu_load_info_len_offset
    str x8, [sp, x9]
    mov x0, sp
    /* uargs: the capsule's NUL terminated load args, not a user pointer. */
    ldr x8, =ksu_load_args_capsule_offset
    add x1, x23, x8
    mov x2, xzr
    bl ksu_ext_load_module
    add sp, sp, x20

    /* x24 was saved in the prologue and is free by now. */
    mov x24, x0
    adrp x0, ksu_load_ok_message
    add x0, x0, :lo12:ksu_load_ok_message
    adrp x1, ksu_load_failed_message
    add x1, x1, :lo12:ksu_load_failed_message
    cmp x24, xzr
    csel x0, x1, x0, lt
    mov x1, x24
    bl ksu_boot_log

.Lbootstrap_out:
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x29, x30, [sp], #16
    ret
    .size ksu_bootstrap, . - ksu_bootstrap

/*
 * Every capsule check lands here with the failing field index in w1, so a boot
 * log says which field the stub disagreed with instead of staying silent.
 */
    .balign 16
.Lfail_capsule_magic:
    mov w1, #1
    b .Lcapsule_failed
.Lfail_capsule_version:
    mov w1, #2
    b .Lcapsule_failed
.Lfail_capsule_header_size:
    mov w1, #3
    b .Lcapsule_failed
.Lfail_capsule_size:
    mov w1, #4
    b .Lcapsule_failed
.Lfail_module_offset:
    mov w1, #5
    b .Lcapsule_failed
.Lfail_module_size:
    mov w1, #6
    b .Lcapsule_failed
.Lfail_fixup_offset:
    mov w1, #7
    b .Lcapsule_failed
.Lfail_fixup_count:
    mov w1, #8
    b .Lcapsule_failed
.Lcapsule_failed:
    adrp x0, ksu_capsule_failed_message
    add x0, x0, :lo12:ksu_capsule_failed_message
    bl ksu_boot_log
    b .Lbootstrap_out

.Lfail_vmalloc:
    adrp x0, ksu_vmalloc_failed_message
    add x0, x0, :lo12:ksu_vmalloc_failed_message
    ldr x1, =ksu_module_size
    bl ksu_boot_log
    b .Lbootstrap_out

/*
 * printk(fmt, ...) through a *direct* branch: the injector resolves
 * `ksu_ext_printk` either to the kernel's printk or to `ksu_no_printk` below,
 * and the branch is PC-relative, so KASLR does not matter.  An absolute address
 * baked in at patch time would be the link-time one and is wrong on every
 * relocated boot.
 *
 * x0 is the format, x1..x3 are its arguments; the register state the caller
 * keeps live lives in x19..x24, which printk preserves.
 */
    .balign 16
    .type ksu_boot_log, %function
ksu_boot_log:
    .inst 0xd503245f              /* BTI c */
    b ksu_ext_printk              /* printk returns to our caller */
    .size ksu_boot_log, . - ksu_boot_log

/* The logger target on a kernel that has no printk. */
    .balign 16
    .type ksu_no_printk, %function
ksu_no_printk:
    .inst 0xd503245f              /* BTI c */
    ret
    .size ksu_no_printk, . - ksu_no_printk

/*
 * The capsule's virtual address, reached through the linear map because it
 * lives beyond the link-time _end, i.e. outside the kernel image mapping.
 * Both the bootstrap and the strndup adapter below need it (the adapter has to
 * recognise the capsule's load args without one), so it lives in one place.
 * Returns in x0 and clobbers x8 only.
 */
    .balign 16
    .type ksu_capsule_address, %function
ksu_capsule_address:
    .inst 0xd503245f              /* BTI c */
    adrp x0, ksu_image_base
    add x0, x0, :lo12:ksu_image_base
    adrp x8, ksu_ext_kimage_voffset
    add x8, x8, :lo12:ksu_ext_kimage_voffset
    ldr x8, [x8]
    sub x0, x0, x8
    ldr x8, =ksu_capsule_image_offset
    add x0, x0, x8
    adrp x8, ksu_ext_memstart_addr
    add x8, x8, :lo12:ksu_ext_memstart_addr
    ldr x8, [x8]
    sub x0, x0, x8
    ldr x8, =ksu_page_offset
    orr x0, x0, x8
    ret
    .size ksu_capsule_address, . - ksu_capsule_address

/*
 * This replaces the arm64_memblock_init() call that reserves
 * [kernel_start, _end). x0 remains the physical kernel start; extending x1
 * makes the same reservation cover the appended capsule, then the original
 * memblock_reserve() returns directly to arm64_memblock_init().
 */
    .balign 16
    .global ksu_memblock_reserve_wrapper
    .type ksu_memblock_reserve_wrapper, %function
ksu_memblock_reserve_wrapper:
    .inst 0xd503245f              /* BTI c */
    ldr x8, =ksu_reserve_extension
    add x1, x1, x8
    b ksu_ext_memblock_reserve
    .size ksu_memblock_reserve_wrapper, . - ksu_memblock_reserve_wrapper

/*
 * load_module() calls strndup_user(uargs, ...).  The pre-exec PID 1 has no
 * usable userspace mapping, so the injector redirects that one call here: the
 * capsule's load args are a *kernel* address and have to be duplicated with
 * kstrdup instead.  Every other (userspace) caller keeps the original path.
 *
 * The comparison target is the capsule's load args, which only exists at run
 * time (KASLR), which is why the address is recomputed here instead of being
 * baked in by the injector.
 */
    .balign 16
    .global ksu_strndup_user_adapter
    .type ksu_strndup_user_adapter, %function
ksu_strndup_user_adapter:
    .inst 0xd503245f              /* BTI c */
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    stp x19, x20, [sp, #-16]!
    mov x19, x0                   /* uargs */
    bl ksu_capsule_address
    ldr x8, =ksu_load_args_capsule_offset
    add x0, x0, x8
    cmp x19, x0
    b.ne .Lduplicate_is_a_user_pointer
    mov x0, x19
    ldr w1, =ksu_gfp_kernel
    bl ksu_ext_kstrdup
    cbnz x0, .Lduplicate_kernel_args_done
    mov x0, #-12                   /* ERR_PTR(-ENOMEM) */
    b .Lduplicate_kernel_args_done
.Lduplicate_is_a_user_pointer:
    mov x0, x19
    bl ksu_ext_strndup_user
.Lduplicate_kernel_args_done:
    ldp x19, x20, [sp], #16
    ldp x29, x30, [sp], #16
    ret
    .size ksu_strndup_user_adapter, . - ksu_strndup_user_adapter

    .ltorg

    .section .rodata.ksu_bootstrap, "a"
    .balign 8
ksu_hello_message:
    .asciz "\0016KernelSU: bootstrap: Hello, KernelSU!\n"
ksu_capsule_ok_message:
    .asciz "\0016KernelSU: bootstrap: capsule ok, module=%lu bytes, fixups=%lu\n"
ksu_capsule_failed_message:
    .asciz "\0013KernelSU: bootstrap: capsule check #%d failed, injection skipped\n"
ksu_vmalloc_failed_message:
    .asciz "\0013KernelSU: bootstrap: vmalloc(%lu) failed, injection skipped\n"
ksu_load_ok_message:
    .asciz "\0016KernelSU: bootstrap: load_module -> %ld\n"
ksu_load_failed_message:
    .asciz "\0013KernelSU: bootstrap: load_module -> %ld\n"
