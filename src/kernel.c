#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "vos_config.h"
#include "vos.h"

/*
 * Kernel service.
 *
 * The kernel owns the scheduling state declared in vos.h.  Task queues are
 * manipulated through the public internal task helpers implemented in
 * task.c; this keeps queue ordering in one place.
 */

static vosTaskHandle_t g_vosDispatchTask;

/* task.c internal interfaces */
extern void vosInitTask(void);
extern void vosTaskEnque(vosTaskQueHdr_t *queue, vosTaskCB_t *task);
extern vosTaskCB_t *vosTaskDeque(vosTaskQueHdr_t *queue);
extern bool vosTargetTaskDeque(vosTaskQueHdr_t *queue, vosTaskCB_t *task);

static bool vosKernelTaskValid(vosTaskHandle_t task)
{
    uint32_t i;

    if (task == NULL || task == (vosTaskHandle_t)VOS_END_PTR) {
        return false;
    }

    for (i = 0U; i < VOS_TASK_NUM; ++i) {
        if (task == &g_vosTaskCB[i]) {
            return task->task_func != NULL;
        }
    }
    return false;
}

static void vosKernelQueueInit(vosTaskQueHdr_t *queue)
{
    if (queue != NULL) {
        queue->next_ptr = NULL;
    }
}

static vosTaskHandle_t vosKernelCurrentTask(void)
{
    vosTaskHandle_t task = g_vosKernelCB.run_task.next_ptr;
    return vosKernelTaskValid(task) ? task : NULL;
}

static vosTaskHandle_t vosKernelSelectReady(void)
{
    vosTaskHandle_t task = g_vosKernelCB.ready_que.next_ptr;
    return vosKernelTaskValid(task) ? task : NULL;
}

static void vosKernelMoveRunTo(vosTaskQueHdr_t *destination)
{
    vosTaskHandle_t current = vosKernelCurrentTask();

    if (current == NULL || destination == NULL) {
        return;
    }

    (void)vosTargetTaskDeque(&g_vosKernelCB.run_task, current);
    vosTaskEnque(destination, current);
}

static void vosKernelSchedule(void)
{
    vosTaskHandle_t current = vosKernelCurrentTask();
    vosTaskHandle_t next = vosKernelSelectReady();

    if (next == NULL) {
        return;
    }

    /* A higher-priority ready task preempts the current task. */
    if (current != NULL && current->task_pri > next->task_pri) {
        return;
    }

    if (current != NULL) {
        vosKernelMoveRunTo(&g_vosKernelCB.ready_que);
    }

    next = vosTaskDeque(&g_vosKernelCB.ready_que);
    if (next != NULL) {
        vosTaskEnque(&g_vosKernelCB.run_task, next);
        g_vosDispatchTask = next;
    }
}

void vos_initKernel(void)
{
    g_vosKernelCB.start_kernel = false;
    vosKernelQueueInit(&g_vosKernelCB.run_task);
    vosKernelQueueInit(&g_vosKernelCB.ready_que);
    vosKernelQueueInit(&g_vosKernelCB.wait_que);
    vosKernelQueueInit(&g_vosKernelCB.dormant_que);
    g_vosDispatchTask = NULL;
    vosInitTask();
}

/* Compatibility spelling used by the design document. */
void vosKernelInit(void)
{
    vos_initKernel();
}

void vosTaskDispatch(vosTaskHandle_t handle)
{
    if (!g_vosKernelCB.start_kernel || !vosKernelTaskValid(handle)) {
        return;
    }

    if (vosKernelCurrentTask() != handle) {
        (void)vosTargetTaskDeque(&g_vosKernelCB.ready_que, handle);
        vosKernelMoveRunTo(&g_vosKernelCB.ready_que);
        vosTaskEnque(&g_vosKernelCB.run_task, handle);
    }
    g_vosDispatchTask = handle;

    /* A target port may replace this function with a PendSV trigger. */
    vosPendSVHandler();
}

void vosPendSVHandler(void)
{
    /*
     * The register save/restore part is architecture-specific and belongs in
     * the Cortex-M assembly port.  The C scheduler has already prepared the
     * RUN queue and g_vosDispatchTask.  Keeping this handler callable on a
     * host also makes queue behavior testable without ARM instructions.
     */
    if (!g_vosKernelCB.start_kernel || !vosKernelTaskValid(g_vosDispatchTask)) {
        return;
    }
}

void vosSysTickHandler(void)
{
    if (!g_vosKernelCB.start_kernel) {
        return;
    }

#if (VOS_DISPATCH == VOS_TIME_SLICE)
    vosKernelSchedule();
#else
    /* Event-driven mode only schedules when an API requests dispatch. */
    (void)0;
#endif
}

vosError_e vosKernelStart(void)
{
    if (g_vosKernelCB.start_kernel) {
        return VOS_INVALID_PARAM;
    }

    g_vosKernelCB.start_kernel = true;
    vosKernelSchedule();

    /*
     * This is the portable scheduler loop.  On Cortex-M, the task entry is
     * normally reached by the PendSV port; the direct call below is useful
     * for the non-assembly/host implementation and returns when no task is
     * runnable.  A task can call vosTaskExit() to leave RUN state.
     */
    while (g_vosKernelCB.start_kernel) {
        vosTaskHandle_t task = vosKernelCurrentTask();
        if (task == NULL) {
            vosKernelSchedule();
            task = vosKernelCurrentTask();
            if (task == NULL) {
                g_vosKernelCB.start_kernel = false;
                break;
            }
        }

        ((int32_t (*)(int32_t, char **))(uintptr_t)task->task_func)(0, NULL);

        /* A returning task is treated as exited. */
        if (vosKernelCurrentTask() == task) {
            (void)vosTargetTaskDeque(&g_vosKernelCB.run_task, task);
            task->next_ptr = NULL;
            task->task_func = NULL;
            task->stacK_top = NULL;
            task->stack_size = 0U;
            task->task_pri = 0U;
        }

        vosKernelSchedule();
        if (g_vosKernelCB.run_task.next_ptr == NULL &&
            g_vosKernelCB.ready_que.next_ptr == NULL) {
            g_vosKernelCB.start_kernel = false;
        }
    }

    return VOS_OK;
}
