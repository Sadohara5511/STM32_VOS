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
    #define RUN_TASK        0x0
    #define STACK_POINTER   0x4
    #define STACK_TOP       0x14

/* Cortex-M3 exception entry has already stacked r0-r3, r12, lr, pc, xPSR.
 * Save the callee-saved registers r4-r11 into the task stack before switching.
 * The stack pointer for the current task is stored in its task control block. */
/**
 * 特権モードにて動作するタスクディスパッチャ―
 * @note Cortex-M3, M4のpspを使用せずmspを使用した特権モード動作向け
 */
vosPendSVHandler:
	cpsid	i
	mrs		r0, msp

#if 1 /* 現スタックポインタをRUNタスクに退避する */
	stmdb	  r0!, {r4-r11}
	ldr	  r1, =g_vosKernelCB		/* g_vosKernelCB.run_task.next_ptr */
	ldr	  r1, [r1, #RUN_TASK]		/* run_task.next_ptrはg_vosKernelCBの先頭アドレスにある */
	str	  r0, [r1, #STACK_POINTER]	/* スタックポインタをrun_task.stack_pointerに保存 */
#endif
	/* 次遷移タスクのタスクスタックポインタを復元 */
	bl		vosPendSVPrepare			/* return(R0)は、run_task（TaskCB)領域 */
	ldr		r0, [r0, #STACK_POINTER]	/* run_task.stack_pointerから復元 */
	ldmia	r0!, {r4-r11}

	msr		msp, r0
	mov		lr, 0xfffffff9				/* EXE_RETUR code=MSPリターン */
	cpsie	i
	bx		lr
#if 0	//非特権モード
vosPendSVHandler:
	cpsid	i
	mrs		r0, psp
	cbz		r0, 1f
	/* 現スタックポインタをRUNタスクに退避する */
	stmdb	r0!, {r4-r11}
	ldr		r1, =g_vosKernelCB			/* g_vosKernelCB.run_task.next_ptr */
	//ldr	  r2, [r1, #RUN_TASK]		/* run_task.next_ptrはg_vosKernelCBの先頭アドレスにある */
	str		r0, [r1, #STACK_POINTER]	/* run_task.stack_pointerに保存 */
1:
	/* 次遷移タスクのタスクスタックポインタを復元 */
	bl		vosPendSVPrepare
	ldr		r0, [r0, #STACK_POINTER]	/* run_task.stack_pointerから復元 */
//	ldmia	r0!, {r4-r11}

	msr		psp, r0
	mov		lr, 0xfffffffd
	cpsie	i
	bx		lr
#endif
#if 0	//オリジナル
vosPendSVHandler:
	cpsid	i
	mrs		r0, psp
	cbz		r0, 1f
	/* 現スタックポインタをRUNタスクに退避する */
	stmdb	r0!, {r4-r11}
	ldr		r1, =g_vosKernelCB			/* g_vosKernelCB.run_task.next_ptr */
	//ldr	  r2, [r1, #RUN_TASK]		/* run_task.next_ptrはg_vosKernelCBの先頭アドレスにある */
	str		r0, [r1, #STACK_POINTER]	/* run_task.stack_pointerに保存 */
1:
	/* 次遷移タスクのタスクスタックポインタを復元 */
	push	{lr}
	bl		vosPendSVPrepare
	pop		{lr}
	ldr		r1, =g_vosKernelCB			/* g_vosKernelCB.run_task.next_ptr */
	ldr		r0, [r1, #STACK_POINTER]	/* run_task.stack_pointerに保存 */
	ldmia	r0!, {r4-r11}

	msr		psp, r0
	cpsie	i
	bx		lr
#endif
	.size vosPendSVHandler, .-vosPendSVHandler
