#include "vos.h"

/* The kernel owns scheduling state; task and service modules only manipulate
 * the queue headers exposed by vos.h.  Queues are priority ordered and are
 * FIFO for tasks with the same priority. */

vosTaskHandle_t g_vosDispatchTask;

static bool vos_task_queue_is_empty(const vosTaskQueHdr_t *hdr)
{
    return hdr == NULL || hdr->next_ptr == NULL ||
           hdr->next_ptr == (vosTaskCB_t *)VOS_END_PTR;
}

void vosTaskEnque(vosTaskQueHdr_t *hdr_ptr, vosTaskCB_t *task_ptr)
{
    vosTaskCB_t *prev;
    vosTaskCB_t *cur;

    if (hdr_ptr == NULL || task_ptr == NULL ||
        task_ptr == (vosTaskCB_t *)VOS_END_PTR) {
        return;
    }

    task_ptr->next_ptr = NULL;
    prev = NULL;
    cur = hdr_ptr->next_ptr;
    while (cur != NULL && cur != (vosTaskCB_t *)VOS_END_PTR &&
           cur->task_pri >= task_ptr->task_pri) {
        prev = cur;
        cur = cur->next_ptr;
    }

    task_ptr->next_ptr = cur;
    if (prev == NULL) {
        hdr_ptr->next_ptr = task_ptr;
    } else {
        prev->next_ptr = task_ptr;
    }
}

vosTaskCB_t *vosTaskDeque(vosTaskQueHdr_t *hdr_ptr)
{
    vosTaskCB_t *task;

    if (vos_task_queue_is_empty(hdr_ptr)) {
        return NULL;
    }
    task = hdr_ptr->next_ptr;
    hdr_ptr->next_ptr = task->next_ptr;
    task->next_ptr = NULL;
    return task;
}

bool vosTargetTaskDeque(vosTaskQueHdr_t *hdr_ptr, vosTaskCB_t *task_ptr)
{
    vosTaskCB_t *prev = NULL;
    vosTaskCB_t *cur;

    if (hdr_ptr == NULL || task_ptr == NULL) {
        return false;
    }
    cur = hdr_ptr->next_ptr;
    while (cur != NULL && cur != (vosTaskCB_t *)VOS_END_PTR) {
        if (cur == task_ptr) {
            if (prev == NULL) {
                hdr_ptr->next_ptr = cur->next_ptr;
            } else {
                prev->next_ptr = cur->next_ptr;
            }
            cur->next_ptr = NULL;
            return true;
        }
        prev = cur;
        cur = cur->next_ptr;
    }
    return false;
}

void vos_initKernel(void)
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

/* Select the next task.  The actual register save/restore is deliberately
 * kept in vosPendSVHandler on Cortex-M; this function only prepares it. */
void vosTaskDispatch(vosTaskHandle_t handle)
{
    if (handle == NULL || handle == (vosTaskHandle_t)VOS_END_PTR) {
        return;
    }
    g_vosDispatchTask = handle;
}

void vosPendSVHandler(void)
{
    vosTaskCB_t *next = g_vosDispatchTask;

    if (next == NULL) {
        return;
    }
    if (g_vosKernelCB.run_task.next_ptr != NULL) {
        vosTaskCB_t *current = g_vosKernelCB.run_task.next_ptr;
        g_vosKernelCB.run_task.next_ptr = NULL;
        vosTaskEnque(&g_vosKernelCB.ready_que, current);
    }
    g_vosKernelCB.run_task.next_ptr = next;
    g_vosDispatchTask = NULL;
}

void vosSysTickHandler(void)
{
    vosTaskCB_t *current = g_vosKernelCB.run_task.next_ptr;
    vosTaskCB_t *ready = g_vosKernelCB.ready_que.next_ptr;

    if (!g_vosKernelCB.start_kernel || ready == NULL) {
        return;
    }
#if (VOS_DISPATCH == VOS_TIME_SLICE)
    vosTaskDispatch(ready);
    (void)vosTaskDeque(&g_vosKernelCB.ready_que);
    vosPendSVHandler();
#else
    if (current == NULL || ready->task_pri > current->task_pri) {
        vosTaskDispatch(ready);
        (void)vosTaskDeque(&g_vosKernelCB.ready_que);
        vosPendSVHandler();
    }
#endif
}

/* Cooperative fallback used by the C implementation.  On Cortex-M a task
 * normally returns through the PendSV context restore path. */
vosError_e vosKernelStart(void)
{
    g_vosKernelCB.start_kernel = true;

    while (g_vosKernelCB.ready_que.next_ptr != NULL ||
           g_vosKernelCB.run_task.next_ptr != NULL) {
        if (g_vosKernelCB.run_task.next_ptr == NULL) {
            vosTaskDispatch(vosTaskDeque(&g_vosKernelCB.ready_que));
            vosPendSVHandler();
        }
        if (g_vosKernelCB.run_task.next_ptr != NULL &&
            g_vosKernelCB.run_task.next_ptr->task_func != NULL) {
            void (*task)(int32_t, char **) =
                (void (*)(int32_t, char **))g_vosKernelCB.run_task.next_ptr->task_func;
            task(0, NULL);
            /* A returning task has implicitly exited. */
            g_vosKernelCB.run_task.next_ptr->task_func = NULL;
            g_vosKernelCB.run_task.next_ptr = NULL;
        } else if (g_vosKernelCB.ready_que.next_ptr == NULL) {
            break;
        }
    }
    g_vosKernelCB.start_kernel = false;
    return VOS_OK;
}
