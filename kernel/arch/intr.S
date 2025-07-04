.cfi_sections .eh_frame, .debug_frame

.section .text
.global common_exception_stub
common_exception_stub:
    .cfi_startproc simple
    .cfi_signal_frame
    .cfi_def_cfa %rsp, 0
    .cfi_offset %rip, 16
    .cfi_offset %rsp, 40
    cld
    pushq %rax
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %rax, 0
    pushq %rbx
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %rbx, 0
    pushq %rcx
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %rcx, 0
    pushq %rdx
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %rdx, 0
    pushq %rsi
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %rsi, 0
    pushq %rdi
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %rdi, 0
    pushq %rbp
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %rbp, 0
    pushq %r8
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %r8, 0
    pushq %r9
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %r9, 0
    pushq %r10
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %r10, 0
    pushq %r11
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %r11, 0
    pushq %r12
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %r12, 0
    pushq %r13
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %r13, 0
    pushq %r14
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %r14, 0
    pushq %r15
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %r15, 0
    movq %rsp, %rdi
    call common_exception_handler
    popq %r15
    .cfi_adjust_cfa_offset -8
    .cfi_restore %r15
    popq %r14
    .cfi_adjust_cfa_offset -8
    .cfi_restore %r14
    popq %r13
    .cfi_adjust_cfa_offset -8
    .cfi_restore %r13
    popq %r12
    .cfi_adjust_cfa_offset -8
    .cfi_restore %r12
    popq %r11
    .cfi_adjust_cfa_offset -8
    .cfi_restore %r11
    popq %r10
    .cfi_adjust_cfa_offset -8
    .cfi_restore %r10
    popq %r9
    .cfi_adjust_cfa_offset -8
    .cfi_restore %r9
    popq %r8
    .cfi_adjust_cfa_offset -8
    .cfi_restore %r8
    popq %rbp
    .cfi_adjust_cfa_offset -8
    .cfi_restore %rbp
    popq %rdi
    .cfi_adjust_cfa_offset -8
    .cfi_restore %rdi
    popq %rsi
    .cfi_adjust_cfa_offset -8
    .cfi_restore %rsi
    popq %rdx
    .cfi_adjust_cfa_offset -8
    .cfi_restore %rdx
    popq %rcx
    .cfi_adjust_cfa_offset -8
    .cfi_restore %rcx
    popq %rbx
    .cfi_adjust_cfa_offset -8
    .cfi_restore %rbx
    popq %rax
    .cfi_adjust_cfa_offset -8
    .cfi_restore %rax
    addq $16, %rsp
    .cfi_adjust_cfa_offset -16
    iretq
    .cfi_endproc
