/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Minimal x86-64 bootstrap linked directly into a caller-provided kernel text
 * cave.  All ksu_ext_* and ksu_* symbols are supplied by the offline injector.
 * Relative branches and RIP-relative references stay valid under KASLR; the
 * capsule is reached through the kernel image mapping because the injector
 * places it inside the decompressed image, just past _end.
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
    .type ksu_bootstrap, @function
ksu_bootstrap:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    subq $8, %rsp                            /* keep the ABI's 16-byte alignment */

    /* A stub that ran is the first thing a boot log has to show. */
    leaq ksu_hello_message(%rip), %rdi
    call ksu_boot_log

    /* Preserve the original kernel_init() call first. */
    call ksu_ext_async_synchronize_full

    /* r12: capsule, through the direct map (see ksu_capsule_address). */
    call ksu_capsule_address
    movq %rax, %r12

    /* Reject a missing/truncated/wrong capsule before touching its module. */
    movq ksu_capsule_magic_value(%rip), %rax
    cmpq %rax, 0(%r12)
    jne .Lfail_capsule_magic
    movl $ksu_capsule_version, %eax
    cmpl %eax, 8(%r12)
    jne .Lfail_capsule_version
    movl $ksu_capsule_header_size, %eax
    cmpl %eax, 12(%r12)
    jne .Lfail_capsule_header_size
    movq $ksu_capsule_size, %rax
    cmpq %rax, 16(%r12)
    jne .Lfail_capsule_size
    movq $ksu_module_capsule_offset, %rax
    cmpq %rax, 24(%r12)
    jne .Lfail_module_offset
    movq $ksu_module_size, %rax
    cmpq %rax, 32(%r12)
    jne .Lfail_module_size
    movq $ksu_fixup_capsule_offset, %rax
    cmpq %rax, 40(%r12)
    jne .Lfail_fixup_offset
    movq $ksu_fixup_count, %rax
    cmpq %rax, 48(%r12)
    jne .Lfail_fixup_count

    leaq ksu_capsule_ok_message(%rip), %rdi
    movq $ksu_module_size, %rsi
    movq $ksu_fixup_count, %rdx
    call ksu_boot_log

    /* load_module() owns and vfree()s this temporary copy. */
    movq $ksu_module_size, %rdi
    call ksu_ext_vmalloc
    testq %rax, %rax
    jz .Lfail_vmalloc
    movq %rax, %r13                         /* r13: vmalloc-ed module */
    movq %r13, %rdi
    movq %r12, %rsi
    addq $ksu_module_capsule_offset, %rsi
    movq $ksu_module_size, %rdx
    call ksu_ext_memcpy

    /* Apply compact { Elf64_Sym file offset, kernel image offset } fixups. */
    movq %r12, %r14
    addq $ksu_fixup_capsule_offset, %r14
    movq $ksu_fixup_count, %r15
    leaq ksu_image_base(%rip), %rbx
    testq %r15, %r15
    jz .Lfixups_done
.Lfixup_loop:
    movl 0(%r14), %eax
    movq 8(%r14), %rcx
    addq %r13, %rax
    movw $0xfff1, 6(%rax)                   /* SHN_ABS */
    leaq (%rbx,%rcx), %rdx
    movq %rdx, 8(%rax)                      /* st_value */
    addq $16, %r14
    decq %r15
    jnz .Lfixup_loop
.Lfixups_done:

    /* Oversized, zeroed target_load_info storage; only hdr/len are seeded. */
    movq %rsp, %rbp
    movq $ksu_load_info_size, %rax
    subq %rax, %rsp
    andq $-16, %rsp
    movq %rsp, %rdi
    movq %rax, %rcx
    shrq $3, %rcx
    xorl %eax, %eax
    rep stosq

    movq $ksu_load_info_hdr_offset, %rcx
    movq %r13, (%rsp,%rcx)
    movq $ksu_load_info_len_offset, %rcx
    movq $ksu_module_size, %rdx
    movq %rdx, (%rsp,%rcx)

    movq %rsp, %rdi
    /* uargs: the capsule's NUL terminated load args, not a user pointer. */
    movq %r12, %rsi
    addq $ksu_load_args_capsule_offset, %rsi
    xorl %edx, %edx
    call ksu_ext_load_module
    movq %rbp, %rsp

    /* r12 held the capsule pointer, which is dead by now. */
    movq %rax, %r12
    leaq ksu_load_ok_message(%rip), %rdi
    leaq ksu_load_failed_message(%rip), %rcx
    testq %r12, %r12
    cmovsq %rcx, %rdi
    movq %r12, %rsi
    call ksu_boot_log

.Lbootstrap_out:
    addq $8, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    ret
    .size ksu_bootstrap, . - ksu_bootstrap

/*
 * Every capsule check lands here with the failing field index in %esi, so a
 * boot log says which field the stub disagreed with instead of staying silent.
 */
    .balign 16
.Lfail_capsule_magic:
    movl $1, %esi
    jmp .Lcapsule_failed
.Lfail_capsule_version:
    movl $2, %esi
    jmp .Lcapsule_failed
.Lfail_capsule_header_size:
    movl $3, %esi
    jmp .Lcapsule_failed
.Lfail_capsule_size:
    movl $4, %esi
    jmp .Lcapsule_failed
.Lfail_module_offset:
    movl $5, %esi
    jmp .Lcapsule_failed
.Lfail_module_size:
    movl $6, %esi
    jmp .Lcapsule_failed
.Lfail_fixup_offset:
    movl $7, %esi
    jmp .Lcapsule_failed
.Lfail_fixup_count:
    movl $8, %esi
    jmp .Lcapsule_failed
.Lcapsule_failed:
    leaq ksu_capsule_failed_message(%rip), %rdi
    call ksu_boot_log
    jmp .Lbootstrap_out

.Lfail_vmalloc:
    leaq ksu_vmalloc_failed_message(%rip), %rdi
    movq $ksu_module_size, %rsi
    call ksu_boot_log
    jmp .Lbootstrap_out

/*
 * printk(fmt, ...) through a *direct* branch: the injector resolves
 * `ksu_ext_printk` either to the kernel's printk or to `ksu_no_printk` below,
 * and the branch is PC-relative, so KASLR does not matter.  An absolute address
 * baked in at patch time would be the link-time one and is wrong on every
 * relocated boot.
 *
 * %rdi is the format, %rsi/%rdx/%rcx are its arguments; the register state the
 * caller keeps live lives in rbx/rbp/r12..r15, which printk preserves.
 */
    .balign 16
    .type ksu_boot_log, @function
ksu_boot_log:
    jmp ksu_ext_printk                      /* printk returns to our caller */
    .size ksu_boot_log, . - ksu_boot_log

/* The logger target on a kernel that has no printk. */
    .balign 16
    .type ksu_no_printk, @function
ksu_no_printk:
    ret
    .size ksu_no_printk, . - ksu_no_printk

/*
 * The capsule's address, reached through the direct map.  The capsule sits past
 * `_end`, which is outside the kernel image mapping: `cleanup_highmap()` trims
 * that back to `roundup(_brk_end, PMD_SIZE)`, so the RIP-relative image address
 * is only mapped for as long as the brk area happens to reach.
 *
 *   capsule_phys = __pa_symbol(_text) + capsule_image_offset
 *   capsule      = page_offset_base + capsule_phys
 *
 * with `__pa_symbol(x) = x - __START_KERNEL_map + phys_base`.  Both `phys_base`
 * and `page_offset_base` are randomized at boot (the virtual and the physical
 * KASLR offsets are independent), so they are read at run time; everything else
 * here is invariant.  Both the bootstrap and the strndup adapter below need
 * this address, so it lives in one place.  Returns in %rax, clobbers
 * %rax/%rcx/%rdx.
 */
    .balign 16
    .type ksu_capsule_address, @function
ksu_capsule_address:
    leaq ksu_bootstrap(%rip), %rax
    subq $ksu_bootstrap_image_offset, %rax   /* _text, runtime address */
    movl $ksu_kernel_map_delta, %edx         /* -__START_KERNEL_map, zero extended */
    addq %rdx, %rax
    leaq ksu_phys_base(%rip), %rcx
    addq (%rcx), %rax                        /* __pa_symbol(_text) */
    addq $ksu_capsule_image_offset, %rax     /* capsule physical address */
    leaq ksu_page_offset_base(%rip), %rcx
    movq (%rcx), %rcx                        /* direct map base */
    addq %rax, %rcx
    movq %rcx, %rax
    ret
    .size ksu_capsule_address, . - ksu_capsule_address

/*
 * This replaces the early_reserve_memory() call that reserves
 * [_text, __end_of_kernel_reserve).  rdi still holds the physical kernel
 * start; extending rsi makes the same reservation cover the appended
 * capsule, then the original memblock_reserve() returns directly to the
 * caller.
 */
    .balign 16
    .global ksu_memblock_reserve_wrapper
    .type ksu_memblock_reserve_wrapper, @function
ksu_memblock_reserve_wrapper:
    addq $ksu_reserve_extension, %rsi
    jmp ksu_ext_memblock_reserve
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
    .type ksu_strndup_user_adapter, @function
ksu_strndup_user_adapter:
    pushq %rbx
    movq %rdi, %rbx                         /* uargs */
    call ksu_capsule_address
    addq $ksu_load_args_capsule_offset, %rax
    cmpq %rbx, %rax
    jne .Lduplicate_is_a_user_pointer
    movq %rbx, %rdi
    movl $ksu_gfp_kernel, %esi
    call ksu_ext_kstrdup
    popq %rbx
    testq %rax, %rax
    jnz .Lduplicate_kernel_args_done
    movq $-12, %rax                         /* ERR_PTR(-ENOMEM) */
    ret
.Lduplicate_is_a_user_pointer:
    movq %rbx, %rdi
    popq %rbx
    jmp ksu_ext_strndup_user
.Lduplicate_kernel_args_done:
    ret
    .size ksu_strndup_user_adapter, . - ksu_strndup_user_adapter

    .section .rodata.ksu_bootstrap, "a"
    .balign 8
    /* Supplied by the injector; `ldr x9, =ksu_capsule_magic` on arm64 is the
     * same thing.  Loading it from memory keeps the byte order in one place. */
ksu_capsule_magic_value:
    .quad ksu_capsule_magic
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
