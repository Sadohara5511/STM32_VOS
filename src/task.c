#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "vos_config.h"
#include "vos.h"

/*
 * Task management.
 *
 * The task control block and the kernel control block are declared by vos.h.
 * This module deliberately does not duplicate those declarations; doing so
 * makes the definitions diverge as the public interface evolves.
 */

static vosError_e g_vosTaskError = VOS_OK;

typedef int32_t (*vosTaskEntry_t)(int32_t, char **);
typedef void (*vosTaskEntryVoid_t)(int32_t, char **);

static bool vosTaskIsSentinel(const vosTaskCB_t *task)
{
    return task == (vosTaskCB_t *)VOS_END_PTR;
}

static bool vosTaskAllocated(const vosTaskCB_t *task)
{
    return task != NULL && !vosTaskIsSentinel(task) &&
           task->task_func != NULL;
}

static bool vosTaskHandleValid(vosTaskHandle_t handle)
{
    return handle >= &g_vosTaskCB[0] &&
           handle < &g_vosTaskCB[VOS_TASK_NUM] &&
           vosTaskAllocated(handle);
}

static void vosTaskQueueInit(vosTaskQueHdr_t *queue)
{
    if (queue != NULL) {
        queue->next_ptr = NULL;
    }
}

/* Insert by priority.  Equal priorities are kept FIFO. */
static void vosTaskQueueInsert(vosTaskQueHdr_t *queue, vosTaskCB_t *task)
{
    vosTaskCB_t *previous;
    vosTaskCB_t *current;

    if (queue == NULL || task == NULL) {
        return;
    }

    previous = NULL;
    current = queue->next_ptr;
    while (current != NULL && !vosTaskIsSentinel(current) &&
           current->task_pri <= task->task_pri) {
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

static bool vosTaskQueueRemove(vosTaskQueHdr_t *queue, vosTaskCB_t *task)
{
    vosTaskCB_t *previous;
    vosTaskCB_t *current;

    if (queue == NULL || task == NULL) {
        return false;
    }

    previous = NULL;
    current = queue->next_ptr;
    while (current != NULL && !vosTaskIsSentinel(current)) {
        if (current == task) {
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

static void vosTaskRemoveFromAllQueues(vosTaskCB_t *task)
{
    (void)vosTaskQueueRemove(&g_vosKernelCB.run_task, task);
    (void)vosTaskQueueRemove(&g_vosKernelCB.ready_que, task);
    (void)vosTaskQueueRemove(&g_vosKernelCB.wait_que, task);
    (void)vosTaskQueueRemove(&g_vosKernelCB.dormant_que, task);
}

static vosTaskHandle_t vosTaskCreateInternal(vosTaskEntry_t task,
                                              uint32_t pri,
                                              uint32_t stack_size,
                                              uint32_t *stack)
{
    uint32_t i;

#if (VOS_API_PARAM_CHECK)
    if (task == NULL || stack == NULL || stack_size == 0U ||
        pri < VOS_TASK_PRI_LO || pri > VOS_TASK_PRI_HI) {
        g_vosTaskError = VOS_INVALID_PARAM;
        return NULL;
    }
#else
    (void)pri;
#endif

    for (i = 0U; i < VOS_TASK_NUM; ++i) {
        vosTaskCB_t *candidate = &g_vosTaskCB[i];
        if (candidate->task_func == NULL) {
            candidate->next_ptr = NULL;
            candidate->wait_svc.msg_cb = NULL;
            candidate->task_pri = pri;
            candidate->stack_size = stack_size;
            candidate->stacK_top = stack;
            /* task_func is the legacy public field supplied by vos.h. */
            candidate->task_func = (uint32_t *)(uintptr_t)task;
            vosTaskQueueInsert(&g_vosKernelCB.dormant_que, candidate);
            g_vosTaskError = VOS_OK;
            return candidate;
        }
    }

    g_vosTaskError = VOS_NO_RESOURCE;
    return NULL;
}

void vosInitTask(void)
{
    uint32_t i;

    for (i = 0U; i < VOS_TASK_NUM; ++i) {
        g_vosTaskCB[i].next_ptr = NULL;
        g_vosTaskCB[i].wait_svc.msg_cb = NULL;
        g_vosTaskCB[i].task_pri = 0U;
        g_vosTaskCB[i].stack_size = 0U;
        g_vosTaskCB[i].stacK_top = NULL;
        g_vosTaskCB[i].task_func = NULL;
    }

    vosTaskQueueInit(&g_vosKernelCB.run_task);
    vosTaskQueueInit(&g_vosKernelCB.ready_que);
    vosTaskQueueInit(&g_vosKernelCB.wait_que);
    vosTaskQueueInit(&g_vosKernelCB.dormant_que);
    g_vosTaskError = VOS_OK;
}

vosTaskHandle_t vosCreateTask(vosTaskEntry_t task, uint32_t pri,
                              uint32_t stack_size, uint32_t *stack)
{
    return vosTaskCreateInternal(task, pri, stack_size, stack);
}

/* API spelling used by the design document. */
vosTaskHandle_t vosTaskCreate(void (*task)(int32_t, char **), uint32_t pri,
                              uint32_t stack_size, uint32_t *stack)
{
    return vosTaskCreateInternal((vosTaskEntry_t)(uintptr_t)task, pri,
                                 stack_size, stack);
}

vosError_e vosTaskStart(vosTaskHandle_t handle)
{
    if (!vosTaskHandleValid(handle)) {
        g_vosTaskError = VOS_INVALID_HANDLE;
        return g_vosTaskError;
    }
    if (!vosTaskQueueRemove(&g_vosKernelCB.dormant_que, handle)) {
        g_vosTaskError = VOS_INVALID_PARAM;
        return g_vosTaskError;
    }
    vosTaskQueueInsert(&g_vosKernelCB.ready_que, handle);
    g_vosTaskError = VOS_OK;
    return g_vosTaskError;
}

vosError_e vosTaskStop(vosTaskHandle_t handle)
{
    if (!vosTaskHandleValid(handle)) {
        g_vosTaskError = VOS_INVALID_HANDLE;
        return g_vosTaskError;
    }
    vosTaskRemoveFromAllQueues(handle);
    vosTaskQueueInsert(&g_vosKernelCB.dormant_que, handle);
    g_vosTaskError = VOS_OK;
    return g_vosTaskError;
}

void vosTaskExit(void)
{
    vosTaskCB_t *current = g_vosKernelCB.run_task.next_ptr;
    if (!vosTaskHandleValid(current)) {
        g_vosTaskError = VOS_INVALID_HANDLE;
        return;
    }

    (void)vosTaskQueueRemove(&g_vosKernelCB.run_task, current);
    current->next_ptr = NULL;
    current->wait_svc.msg_cb = NULL;
    current->task_pri = 0U;
    current->stack_size = 0U;
    current->stacK_top = NULL;
    current->task_func = NULL;
    g_vosTaskError = VOS_OK;
}

vosError_e vosErrorGet(void)
{
    return g_vosTaskError;
}

/* Internal queue helpers used by the kernel and service modules. */
void vosTaskEnque(vosTaskQueHdr_t *queue, vosTaskCB_t *task)
{
    vosTaskQueueInsert(queue, task);
}

vosTaskCB_t *vosTaskDeque(vosTaskQueHdr_t *queue)
{
    vosTaskCB_t *task;
    if (queue == NULL) {
        return NULL;
    }
    task = queue->next_ptr;
    if (task == NULL || vosTaskIsSentinel(task)) {
        return NULL;
    }
    queue->next_ptr = task->next_ptr;
    task->next_ptr = NULL;
    return task;
}

bool vosTargetTaskDeque(vosTaskQueHdr_t *queue, vosTaskCB_t *task)
{
    return vosTaskQueueRemove(queue, task);
}
