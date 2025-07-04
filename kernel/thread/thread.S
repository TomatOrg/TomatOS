.section .text

/**
 * void thread_resume(thread_frame_t* thread);
 */
.global thread_resume_finish
thread_resume_finish:
    /* restore the stack of the thread */
    movq %rdi, %rsp

    /* restore the callee-saved registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    /*
     * enable interrupts and return (we assume that the caller
     * wants interrupts to be enabled) must be one after another
     */
    sti
    ret

/**
 * void switch_to_scheduler(thread_frame_t** frame, void* scheduler_function, void* scheduler_stack);
 */
.global switch_to_scheduler
switch_to_scheduler:
    /* save callee-saved registers */
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* save the current stack */
    movq %rsp, (%rdi)

    /*
     * switch to scheduler stack and jump into the
     * scheduler function
     *
     * same as before sti is required to have interrupts enabled
     * in the scheduler
     */
    movq %rdx, %rax
    sti
    jmp *%rsi

/**
 * void switch_to_scheduler(void* scheduler_function, void* scheduler_stack);
 */
.global jump_to_scheduler
jump_to_scheduler:
    /*
     * switch to scheduler stack and jump into the
     * scheduler function
     *
     * same as before sti is required to have interrupts enabled
     * in the scheduler
     */
    movq %rsi, %rax
    sti
    jmp *%rdi
