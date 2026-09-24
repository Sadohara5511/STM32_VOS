/**
 * カーネル機能
 */
#include "vos.h"

/* カーネルコントロールブロック変数宣言 */
vosKernelCB_t       g_vosKernelCB;
/* タスクコントロールブロック変数宣言 */
vosTaskCB_t         g_vosTaskCB[VOS_TASK_NUM];
/* 次ディスパッチタスク */
vosTaskCB_t*        g_vosDispatchTask;

int32_t     g_vosCriticalCounter;         /* 割り込み抑止解除カウンタ */

/* IDLEタスクコントロールブロックとスタック宣言 */
vosTaskCB_t         g_vosIdleTaskCB;
void vosIdleTask(void*p)
{
	while(1);
}

/**
 * fast fill @32bit architecture
 */
void vosMemset(void *des, uint32_t fill, size_t size)
{
	uint32_t *des_p = des;
    size_t  remain = size & 0x3;

    for(;size > 0; size-=4){
        *des_p++ = fill;
    }
    if(remain){
        uint8_t *des_byte = (uint8_t*)des_p;
        for(;remain > 0; remain--){
            *des_byte++ = fill;
        }
    }
}

/**
 * fast copy @32bit architecture
 */
void vosMemcpy(void *des, void *src, size_t size)
{
	uint32_t *des_p = des;
	uint32_t *src_p = src;
    size_t  remain = size & 0x3;

    for(;size > 0; size-=4){
        *des_p++ = *src_p++;
    }
    if(remain){
        uint8_t *des_byte = (uint8_t*)des_p;
        uint8_t *src_byte = (uint8_t*)src_p;
        for(;remain > 0; remain--){
            *des_byte++ = *src_byte++;
        }
    }
}

static bool queue_empty(const vosTaskQueHdr_t *queue)
{
    return (queue == NUL || queue->next_ptr == NUL /*|| queue->next_ptr == VOS_END_PTR*/);
}

void vosTaskEnque(vosTaskQueHdr_t *queue, vosTaskCB_t *task)
{
    vosTaskCB_t *prev = NUL;
    vosTaskCB_t *list;

#if(VOS_API_PARAM_CHECK)
    if (queue == NUL || task == NUL /*|| task == VOS_END_PTR*/) {
        return;
    }
#endif

    list = queue->next_ptr;
    /* Descending priority; <= keeps equal-priority tasks FIFO. */
    while (list != NUL /*&& list != VOS_END_PTR*/ && list->task_pri >= task->task_pri) {
        prev = list;
        list = list->next_ptr;
    }
    task->next_ptr = list;
    if (prev == NUL) {
        queue->next_ptr = task;
    } else {
        prev->next_ptr = task;
    }
}

vosTaskCB_t *vosTaskDeque(vosTaskQueHdr_t *queue)
{
    vosTaskCB_t *task;
    if (queue_empty(queue)) {
        return NUL;
    }
    task = queue->next_ptr;
    queue->next_ptr = task->next_ptr;
    task->next_ptr = NUL;
    return task;
}

bool vosTargetTaskDeque(vosTaskQueHdr_t *queue, vosTaskCB_t *target)
{
    vosTaskCB_t *prev = NUL;
    vosTaskCB_t *list;

    if (queue == NUL || target == NUL) {
        return false;
    }
    list = queue->next_ptr;
    while (list != NUL /*&& list != VOS_END_PTR*/) {
        if (list == target) {
            if (prev == NUL) {
                queue->next_ptr = list->next_ptr;
            } else {
                prev->next_ptr = list->next_ptr;
            }
            list->next_ptr = NUL;
            return true;
        }
        prev = list;
        list = list->next_ptr;
    }
    return false;
}

vosTaskCB_t *vosTask_getTaskCB(void)
{
	for(int32_t i=0; i < VOS_TASK_NUM; i++)
	{
		if (! g_vosTaskCB[i].task) {
			return &g_vosTaskCB[i];
		}
	}
	return NUL;
}


/**
 * Trigger PendSV
 */
void vosTaskDispatch(vosTaskHandle_t task)
{
#if(VOS_API_PARAM_CHECK)
    if (task == NUL /*|| task == VOS_END_PTR*/) {
        return;
    }
#endif

    g_vosDispatchTask = task;
#if defined(__CORTEX_M) || defined(__arm__) || defined(__thumb__)
    *(volatile uint32_t *)0xE000ED04UL |= (1UL << 28); /* ICSR.PENDSVSET */
#endif
}

/**
 * vosPendSVHandlerからコールされ、キュー管理を更新する
 * @note Called by PendSV_Handler after hardware has entered handler mode.
 */
vosTaskHandle_t vosPendSVPrepare(void)
{
    vosTaskCB_t *run = g_vosKernelCB.run_task.next_ptr;
    vosTaskCB_t *next = g_vosDispatchTask;

    if (next == NUL) {
        return run;
    }
    /* RUNタスクを次遷移先キューにつなぐ */
    if (run != NUL && run != next) {
    	switch(run->next_state) {
    	case VOS_READY:
            vosTaskEnque(&g_vosKernelCB.ready_que, run);
            break;
    	case VOS_WAIT:
            vosTaskEnque(&g_vosKernelCB.wait_que, run);
            break;
    	case VOS_DORMANT:
            vosTaskEnque(&g_vosKernelCB.dormant_que, run);
            break;
    	default:
            break;
    	}
    }
#if 0	/* コール元でキューから外すこと */
    /* ディスパッチするタスクをキューから外しRUNキューにつなぐ */
    if (! vosTargetTaskDeque(&g_vosKernelCB.ready_que, next)) {
        if (! vosTargetTaskDeque(&g_vosKernelCB.wait_que, next)) {
            vosTargetTaskDeque(&g_vosKernelCB.dormant_que, next);
        }
    }
#endif
    g_vosKernelCB.run_task.next_ptr = next;
    g_vosDispatchTask = NUL;
    return next;
}

void vosSysTickHandler(void)
{
    vosTaskCB_t *list;
    vosTaskCB_t *next = g_vosKernelCB.ready_que.next_ptr;

    if (!g_vosKernelCB.start_kernel || next == NUL) {
        return;
    }
    list = g_vosKernelCB.run_task.next_ptr;
#if (VOS_DISPATCH == VOS_TIME_SLICE)
    if (list == NUL || next->task_pri >= list->task_pri) {
        vosTaskDispatch(next);
    }
#else
    if (list == NUL || next->task_pri > list->task_pri) {
        vosTaskDispatch(next);
    }
#endif
}

/**
 * [API] kernel init
 */
void vosKernelInit(void)
{
	extern uint8_t _estack; /* Symbol defined in the linker script */
	extern uint32_t _Min_Stack_Size; /* Symbol defined in the linker script */
    vosMemset(&g_vosKernelCB, 0, sizeof(g_vosKernelCB));
    vosMemset(g_vosTaskCB, 0, sizeof(g_vosTaskCB));
    vosMemset(&g_vosIdleTaskCB, 0, sizeof(g_vosIdleTaskCB));
    g_vosIdleTaskCB.stack_size = (uint32_t)&_estack - (uint32_t)&_Min_Stack_Size;
    g_vosIdleTaskCB.stack_top = (uint32_t*)&_estack;
	g_vosIdleTaskCB.next_state = VOS_READY;
    g_vosIdleTaskCB.task = &vosIdleTask;
    g_vosKernelCB.run_task.next_ptr = &g_vosIdleTaskCB;
}

/**
 * [API] kernel start
 */
vosError_e vosKernelStart(void)
{
    vosTaskCB_t *first;

    if (g_vosKernelCB.start_kernel) {
        return VOS_INVALID_API;
    }

    first = vosTaskDeque(&g_vosKernelCB.ready_que);
    if (first == NUL) {
        return VOS_NOTHING_TASK;
    }
    /* Control transfers to the first task when PendSV is serviced. */
    //g_vosKernelCB.run_task.next_ptr = first;  // Setting in PendSV.
    vosTaskDispatch(first);

    g_vosKernelCB.start_kernel = true;
    while(g_vosKernelCB.start_kernel)
        ;   // It might call the IDLE task.
    return VOS_OK;
}

/**
 * [API] task create
 * @param[in] task	Task function address
 * @param[in] param	Task function parameter
 * @param[in] pri	Task priority
 * @param[in] size	Task stack size
 * @param[in] stack	Task stack top address
 * @return	Task handle
 */
vosTaskHandle_t vosTaskCreate(void (*task)(void*), void*param, VOS_TASKPRI_e pri, size_t size, uint8_t *stack)
{
	uint32_t	*stack_bottom = (uint32_t*)&stack[size];
	vosTaskCB_t	*new_task = vosTask_getTaskCB();
	if (new_task == NUL) {
		return NUL;
	}
    //割り込みによる自動POPレジスタ群
	*(--stack_bottom) = 0x01000000;			//PSR：プログラムステータスレジスタにThumbモード（必須）をセット
	*(--stack_bottom) = (uint32_t)task;		//PC
	*(--stack_bottom) = 0xfffffff9;			//R14(LR)：プロセスEXE_RETUR code
	*(--stack_bottom) = 0;					//R12
	*(--stack_bottom) = 0;					//R3
	*(--stack_bottom) = 0;					//R2
	*(--stack_bottom) = 0;					//R1
	*(--stack_bottom) = (uint32_t)param;	//R0：task parameter
#if 1 //PendSVハンドラでのPOPレジスタ群
	*(--stack_bottom) = 0;				//R11
	*(--stack_bottom) = 0;				//R10
	*(--stack_bottom) = 0;				//R9
	*(--stack_bottom) = 0;				//R8
	*(--stack_bottom) = 0;				//R7
	*(--stack_bottom) = 0;				//R6
	*(--stack_bottom) = 0;				//R5
	*(--stack_bottom) = 0;				//R4
#endif
	new_task->stack_pointer = stack_bottom;
	new_task->task_pri = pri;
	new_task->stack_size = size;
	new_task->stack_top = (uint32_t*)stack;
	new_task->task = task;

	vosTaskEnque(&g_vosKernelCB.ready_que, new_task);
	return new_task;
}
