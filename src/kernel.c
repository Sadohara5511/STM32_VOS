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

/* Cortex-M scheduler state.  Register save/restore is performed by the
 * PendSV handler in kernel_cm4/cm3.S; this file only selects the next task. */
vosTaskHandle_t g_vosDispatchTask;

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
    if (task == NUL || task == VOS_END_PTR) {
        return;
    }
#endif

    g_vosDispatchTask = task;
#if defined(__CORTEX_M) || defined(__arm__) || defined(__thumb__)
    *(volatile uint32_t *)0xE000ED04UL |= (1UL << 28); /* ICSR.PENDSVSET */
#endif
}

/* Called by PendSV_Handler after hardware has entered handler mode. */
vosTaskHandle_t vosPendSVPrepare(void)
{
    vosTaskCB_t *list = g_vosKernelCB.run_task.next_ptr;
    vosTaskCB_t *next = g_vosDispatchTask;

    if (next == NUL) {
        return list;
    }
    if (list != NUL && list != next) {
        vosTaskEnque(&g_vosKernelCB.ready_que, list);
    }
    //vosTargetTaskDeque(&g_vosKernelCB.ready_que, next);   //不要だろう
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
    vosMemset(&g_vosKernelCB, 0, sizeof(g_vosKernelCB));
    vosMemset(g_vosTaskCB, 0, sizeof(g_vosTaskCB));
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
    g_vosKernelCB.start_kernel = true;
    /* Control transfers to the first task when PendSV is serviced. */
    //g_vosKernelCB.run_task.next_ptr = first;  // Setting in PendSV.
    vosTaskDispatch(first);

    while(g_vosKernelCB.start_kernel)
        ;   // It might call the IDLE task.
    return VOS_OK;
}

/**
 * [API] task create
 */
vosTaskHandle_t  vosTaskCreate(void (*task)(void), uint32_t pri, uint32_t stack_size, uint32_t *stack)
{
	vosTaskCB_t	*new_tcb = vosTask_getTaskCB();
	if (new_tcb == NUL) {
		return NUL;
	}
	new_tcb->task_pri = pri;
	new_tcb->stack_size = stack_size;
	new_tcb->stacK_top = stack;
	new_tcb->task = task;

	vosTaskEnque(&g_vosKernelCB.ready_que, new_tcb);
	return new_tcb;
}
