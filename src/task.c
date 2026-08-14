#include <stdio.h>
#include <stdint.h>
//#include <stddef.h>
#include <stdbool.h>
#include "vos_config.h"
#include "vos.h"

/* タスクコントロールブロック */
struct tag_vosTaskCB {
    vosTaskCB_t *   next_ptr;           /* タスクコントロールブロック・リストポインタ */
    union {
        vosMsgCB_t * msg_cb;            /* メッセージキュー */
        vosEvtCB_t * evt_cb;            /* イベントフラグ */
        vosSemCB_t * sem_cd;            /* セマフォ */
    }wait_svc;                          /* 受信待ちサービス */
    uint32_t        task_pri;           /* タスク優先度 */
    uint32_t        stack_size;         /* スタック領域サイズ(単位:32bit) */
    uint32_t *      stacK_top;          /* スタック領域先頭アドレス */
    uint32_t *      task_func;          /* タスク実行アドレス */
};

/* タスク状態キュー */
typedef struct {
    vosTaskCB_t *   next_ptr;    
} vosTaskQueHdr_t;

/* カーネルコントロールブロック */
typedef struct {
    bool            start_kernel;       /* カーネルStart/Stop */
    vosTaskQueHdr_t run_task;           /* RUNタスク */
    vosTaskQueHdr_t ready_que;          /* READYキュー */
    vosTaskQueHdr_t wait_que;           /* WAITキュー */
    vosTaskQueHdr_t stop_que;           /* STOPキュー */
} vosKernelCB_t;

/* カーネルグローバル */
static vosKernelCB_t g_vosKernelCB;
/* タスク機能グローバル */
static vosTaskCB_t   g_vosTaskCB[VOS_TASK_NUM];
/* 内部管理変数 */
static vosError_e   g_vosTaskError;


/**
 * VOSリソース初期化 */
 */
void vosInitKernel(void)
{
    vosMemset(&g_vosKernelCB, 0, sizeof(g_vosKernelCB));
    vosInitTask();
    vosInitMsgQue();
}

/**
 *
 */
void vosInitTask(void)
{
    vosMemset(&g_vosTaskCB[0], 0, sizeof(g_vosTaskCB));
    g_vosTaskError = 0;
}
/**
 *
 */
vosTaskHandle_t vosCreateTask(int32_t (*task)(int32_t, char**), uint32_t pri, uint32_t stack_size, uint32_t *stack)
{
    vosTaskHandle_t handle = NULL;  /* 空きなし */
#if(VOS_API_PARAM_CHECK)
    if (task == NULL || stack == NULL || stack_size == 0) {
        return (vosTaskHandle_t)0; /* 失敗: パラメータ不正 */
    }
#endif

    /* 空きTCBを探索 */
    for (uint32_t i = 0; i < VOS_TASK_NUM; i++) {
        if (g_vosTaskCB[i].task == NULL) {
            /* 初期化 */
            handle = &g_vosTaskCB[i];
            handle.next_ptr = NUL;
            handle.wait_svc = NUL;
            handle.task_pri = pri;
            handle.stack_size = stack_size;
            handle.stack_top = stack;
            handle.task = task;

            /* READY キューに挿入する */
            vosTaskEnque(&g_vosKernelCB.ready_que, handle);
            break;
        }
    }
    return handle;
}
