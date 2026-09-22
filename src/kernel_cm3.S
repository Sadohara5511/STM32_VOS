    .syntax unified
    .thumb
    .text
    .align 2

    .global vosPendSVHandler
    .global PendSV_Handler
    .type vosPendSVHandler, %function
    .set PendSV_Handler, vosPendSVHandler

    .extern g_vosKernelCB
    .extern g_vosDispatchTask
    .extern vosPendSVPrepare

/* Cortex-M3 exception entry has already stacked r0-r3, r12, lr, pc, xPSR.
 * Save the callee-saved registers r4-r11 into the task stack before switching.
 * The stack pointer for the current task is stored in its task control block. */
vosPendSVHandler:
    cpsid   i
    mrs     r0, psp
    ldr     r1, =g_vosKernelCB
    ldr     r2, [r1, #4]          /* run_task.next_ptr */
    cbz     r2, 1f
    stmdb   r0!, {r4-r11}
    str     r0, [r2, #16]         /* task->stacK_top */
1:
    bl      vosPendSVPrepare
    cbz     r0, 2f
    ldr     r1, [r0, #16]
    cbz     r1, 2f
    ldmia   r1!, {r4-r11}
    msr     psp, r1
2:
    cpsie   i
    bx      lr
    .size vosPendSVHandler, .-vosPendSVHandler
