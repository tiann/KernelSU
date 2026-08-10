/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Minimal ARM64 bootstrap linked directly into a caller-provided kernel text
 * cave. All ksu_ext_* and ksu_* absolute symbols are supplied by the offline
 * injector. Relative branches/ADRP references remain valid after KASLR. The
 * appended capsule is read through the linear map, not through the kernel image
 * mapping, because it lives beyond the link-time _end.
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

    /* Preserve the original kernel_init() call first. */
    bl ksu_ext_async_synchronize_full

    /* Mirror the init_module() policy gates omitted by load_module(). */
    ldr w0, =ksu_cap_sys_module
    bl ksu_ext_capable
    cbz w0, .Lbootstrap_out
    adrp x8, ksu_ext_modules_disabled
    add x8, x8, :lo12:ksu_ext_modules_disabled
    ldr w8, [x8]
    cbnz w8, .Lbootstrap_out

    /*
     * capsule_phys = __pa_symbol(_text) + capsule_image_offset
     * capsule_virt = (capsule_phys - memstart_addr) | PAGE_OFFSET
     */
    adrp x23, ksu_image_base
    add x23, x23, :lo12:ksu_image_base
    adrp x8, ksu_ext_kimage_voffset
    add x8, x8, :lo12:ksu_ext_kimage_voffset
    ldr x8, [x8]
    sub x23, x23, x8
    ldr x8, =ksu_capsule_image_offset
    add x23, x23, x8
    adrp x8, ksu_ext_memstart_addr
    add x8, x8, :lo12:ksu_ext_memstart_addr
    ldr x8, [x8]
    sub x23, x23, x8
    ldr x8, =ksu_page_offset
    orr x23, x23, x8

    /* Reject a missing/truncated/wrong capsule before touching its module. */
    ldr x8, [x23, #0]
    ldr x9, =ksu_capsule_magic
    cmp x8, x9
    b.ne .Lbootstrap_out
    ldr w8, [x23, #8]
    ldr w9, =ksu_capsule_version
    cmp w8, w9
    b.ne .Lbootstrap_out
    ldr w8, [x23, #12]
    ldr w9, =ksu_capsule_header_size
    cmp w8, w9
    b.ne .Lbootstrap_out
    ldr x8, [x23, #16]
    ldr x9, =ksu_capsule_size
    cmp x8, x9
    b.ne .Lbootstrap_out
    ldr x8, [x23, #24]
    ldr x9, =ksu_module_capsule_offset
    cmp x8, x9
    b.ne .Lbootstrap_out
    ldr x8, [x23, #32]
    ldr x9, =ksu_module_size
    cmp x8, x9
    b.ne .Lbootstrap_out
    ldr x8, [x23, #40]
    ldr x9, =ksu_fixup_capsule_offset
    cmp x8, x9
    b.ne .Lbootstrap_out
    ldr x8, [x23, #48]
    ldr x9, =ksu_fixup_count
    cmp x8, x9
    b.ne .Lbootstrap_out

    ldr w0, =ksu_loading_module_id
    mov w1, #1
    bl ksu_ext_security_kernel_load_data
    cbnz w0, .Lbootstrap_out

    /* load_module() owns and vfree()s this temporary copy. */
    ldr x0, =ksu_module_size
    bl ksu_ext_vmalloc
    cbz x0, .Lbootstrap_out
    mov x19, x0

    mov x0, x19
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

    /* Let the target LSM inspect the final bytes passed to load_module(). */
    mov x0, x19
    ldr x1, =ksu_module_size
    ldr w2, =ksu_loading_module_id
    adrp x3, ksu_load_description
    add x3, x3, :lo12:ksu_load_description
    bl ksu_ext_security_kernel_post_load_data
    cbnz w0, .Lfree_copy

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
    adrp x1, ksu_empty_args
    add x1, x1, :lo12:ksu_empty_args
    mov x2, xzr
    bl ksu_ext_load_module
    add sp, sp, x20
    b .Lbootstrap_out

.Lfree_copy:
    mov x0, x19
    bl ksu_ext_vfree

.Lbootstrap_out:
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x29, x30, [sp], #16
    ret
    .size ksu_bootstrap, . - ksu_bootstrap

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
 * load_module() calls strndup_user(uargs, ...). The pre-exec PID 1 has no
 * usable userspace mapping, so the injector redirects that one call here.
 * Normal future module loads tail-call the original target unchanged.
 */
    .balign 16
    .global ksu_strndup_user_adapter
    .type ksu_strndup_user_adapter, %function
ksu_strndup_user_adapter:
    .inst 0xd503245f              /* BTI c */
    adrp x9, ksu_empty_args
    add x9, x9, :lo12:ksu_empty_args
    cmp x0, x9
    b.eq .Lduplicate_kernel_args
    b ksu_ext_strndup_user
.Lduplicate_kernel_args:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    ldr w1, =ksu_gfp_kernel
    bl ksu_ext_kstrdup
    cbnz x0, .Lduplicate_kernel_args_done
    mov x0, #-12                   /* ERR_PTR(-ENOMEM) */
.Lduplicate_kernel_args_done:
    ldp x29, x30, [sp], #16
    ret
    .size ksu_strndup_user_adapter, . - ksu_strndup_user_adapter

    .ltorg

    .section .rodata.ksu_bootstrap, "a"
    .balign 8
ksu_empty_args:
    .asciz ""
ksu_load_description:
    /* Stable LSM description; changing it can alter policy and audit behavior. */
    .asciz "ksu-image-mvp"
