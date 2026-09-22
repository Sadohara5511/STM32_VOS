#include "vos.h"

/* Cortex-M scheduler state.  Register save/restore is performed by the
 * PendSV handler in kernel_cm4.S; this file only selects the next task. */
vosTaskHandle_t g_vosDispatchTask;

static bool queue_empty(const vosTaskQueHdr_t *queue)
{
    return queue == NULL || queue->next_ptr == NULL ||
           queue->next_ptr == (vosTaskCB_t *)VOS_END_PTR;
}

void vosTaskEnque(vosTaskQueHdr_t *queue, vosTaskCB_t *task)
{
    vosTaskCB_t *previous = NULL;
    vosTaskCB_t *current;

    if (queue == NULL || task == NULL ||
        task == (vosTaskCB_t *)VOS_END_PTR) {
        return;
    }

    current = queue->next_ptr;
    /* Descending priority; <= keeps equal-priority tasks FIFO. */
    while (current != NULL && current != (vosTaskCB_t *)VOS_END_PTR &&
           current->task_pri >= task->task_pri) {
        previous = current;
        current = current->next_ptr;
    }
    task->next_ptr = current;
    if (previous == NULL) {
        queue->next_ptr = task;
    } else {
        previous->next_ptr = task;
    }
}

vosTaskCB_t *vosTaskDeque(vosTaskQueHdr_t *queue)
{
    vosTaskCB_t *task;

    if (queue_empty(queue)) {
        return NULL;
    }
    task = queue->next_ptr;
    queue->next_ptr = task->next_ptr;
    task->next_ptr = NULL;
    return task;
}

bool vosTargetTaskDeque(vosTaskQueHdr_t *queue, vosTaskCB_t *target)
{
    vosTaskCB_t *previous = NULL;
    vosTaskCB_t *current;

    if (queue == NULL || target == NULL) {
        return false;
    }
    current = queue->next_ptr;
    while (current != NULL && current != (vosTaskCB_t *)VOS_END_PTR) {
        if (current == target) {
            if (previous == NULL) {
                queue->next_ptr = current->next_ptr;
            } else {
                previous->next_ptr = current->next_ptr;
            }
            current->next_ptr = NULL;
            return true;
        }
        previous = current;
        current = current->next_ptr;
    }
    return false;
}

void vosKernelInit(void)
{
    g_vosKernelCB.start_kernel = false;
    g_vosKernelCB.run_task.next_ptr = NULL;
    g_vosKernelCB.ready_que.next_ptr = NULL;
    g_vosKernelCB.wait_que.next_ptr = NULL;
    g_vosKernelCB.dormant_que.next_ptr = NULL;
    g_vosDispatchTask = NULL;
    g_vosCriticalCounter = 0;

    for (uint32_t i = 0; i < VOS_TASK_NUM; ++i) {
        g_vosTaskCB[i].next_ptr = NULL;
        g_vosTaskCB[i].wait_svc.msg_cb = NULL;
        g_vosTaskCB[i].task_pri = 0;
        g_vosTaskCB[i].stack_size = 0;
        g_vosTaskCB[i].stacK_top = NULL;
        g_vosTaskCB[i].task_func = NULL;
    }
}

/* Backward-compatible spelling used by the initial source tree. */
void vos_initKernel(void)
{
    vosKernelInit();
}

void vosTaskDispatch(vosTaskHandle_t task)
{
    if (task == NULL || task == (vosTaskHandle_t)VOS_END_PTR) {
        return;
    }
    g_vosDispatchTask = task;
#if defined(__CORTEX_M) || defined(__arm__) || defined(__thumb__)
    *(volatile uint32_t *)0xE000ED04UL |= (1UL << 28); /* ICSR.PENDSVSET */
#endif
}

/* Called by PendSV_Handler after hardware has entered handler mode. */
vosTaskHandle_t vosPendSVPrepare(void)
{
    vosTaskCB_t *current = g_vosKernelCB.run_task.next_ptr;
    vosTaskCB_t *next = g_vosDispatchTask;

    if (next == NULL) {
        return current;
    }
    if (current != NULL && current != next) {
        vosTaskEnque(&g_vosKernelCB.ready_que, current);
    }
    (void)vosTargetTaskDeque(&g_vosKernelCB.ready_que, next);
    g_vosKernelCB.run_task.next_ptr = next;
    g_vosDispatchTask = NULL;
    return next;
}

void vosSysTickHandler(void)
{
    vosTaskCB_t *current;
    vosTaskCB_t *next = g_vosKernelCB.ready_que.next_ptr;

    if (!g_vosKernelCB.start_kernel || next == NULL) {
        return;
    }
    current = g_vosKernelCB.run_task.next_ptr;
#if (VOS_DISPATCH == VOS_TIME_SLICE)
    if (current == NULL || next->task_pri >= current->task_pri) {
        vosTaskDispatch(next);
    }
#else
    if (current == NULL || next->task_pri > current->task_pri) {
        vosTaskDispatch(next);
    }
#endif
}

vosError_e vosKernelStart(void)
{
    vosTaskCB_t *first;

    if (g_vosKernelCB.start_kernel) {
        return VOS_INVALID_API;
    }
    g_vosKernelCB.start_kernel = true;
    first = vosTaskDeque(&g_vosKernelCB.ready_que);
    if (first == NULL) {
        g_vosKernelCB.start_kernel = false;
        return VOS_NO_RESOURCE;
    }
    g_vosKernelCB.run_task.next_ptr = first;
    vosTaskDispatch(first);
    /* Control transfers to the first task when PendSV is serviced. */
    return VOS_OK;
}
