/*
 * src/vos_create.c
 * vosCreateTask / vosCreateMsgQue の実装例（生成時に自動で READY キューに入れる）
 *
 * 仮定と注記:
 * - README の仕様を元に最小限の構造体/グローバル変数をここで定義しています。
 * - msg_size は uint32_t 単位のサイズ（1 = 32bit）として扱っています。
 * - タスク未使用判定: task_func == VOS_INIT_PTR (NULL) を未登録とみなします。
 * - メッセージヘッダ配列 g_vosMsgBuff_t は固定長で、キュー生成時に連続領域を割当てます。
 * - エラーコードはポインタ戻り値 NULL で表現します（README: 0=失敗）。
 * - 生成時に自動で READY キューに挿入します。優先度の比較は「数値が大きいほど優先度が高い」と仮定しています。
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* README と合わせた定数・型定義 (簡略化) */
#define VOS_END_PTR     (void*)(-1)
#define VOS_INIT_PTR    (NULL)

#define VOS_ERR_PARAM           (-1)
#define VOS_ERR_RESOURCE        (-2)

#include "include/vos_config.h" /* VOS_TASK_NUM, VOS_MSGQUE_NUM, VOS_TOTAL_MSG_NUM */
#include "include/vos.h"        /* 型エイリアス (vosTaskHandle_t / vosMsgHandle_t) */

/* --- データ構造 (README から抜粋/簡略化) --- */

typedef struct tag_vosMsgHdr vosMsgHdr_t;
struct tag_vosMsgHdr {
    vosMsgHdr_t *   next_ptr;   /* 送受信メッセージチェイン */
    uint32_t *      msg_ptr;    /* メッセージバッファ・ポインタ */
};

typedef struct {
    vosTaskHandle_t next_ptr;   /* タスク待ちキュー ヘッダ互換のため */
} vosTaskQueHdr_t;

typedef struct tag_vosMsgCB {
    vosTaskQueHdr_t wait_task;  /* 受信待ちタスクコントロールブロック・ポインタ */
    vosMsgHdr_t     msg_que;    /* 送受信メッセージキュー (ヘッダ) */
    vosMsgHdr_t *   msg_buff;   /* メッセージバッファ先頭ポインタ (管理配列) */
    uint32_t        free_idx;   /* メッセージバッファの空きインデックス(未使用実装では0) */

    /* メッセージプール */
    uint32_t        msg_num;    /* メッセージバッファ数 */
    uint32_t        msg_size;   /* メッセージバッファ１つのサイズ (uint32_t ��位) */
    uint32_t *      msg_pool;   /* メッセージバッファ領域 (ユーザー提供) */
} vosMsgCB_t;

typedef struct tag_vosTaskCB {
    struct tag_vosTaskCB * next_ptr; /* タスクコントロールブロック・リストポインタ */
    union {
        vosMsgCB_t * msg_cb;     /* メッセージキュー */
        void *       evt_cb;
        void *       sem_cd;
    } wait_svc;
    uint32_t        task_pri;
    uint32_t        stack_size;     /* 単位: uint32_t 個数 */
    uint32_t *      stack_top;
    uint32_t *      task_func;      /* タスク実行アドレス (関数ポインタを uint32_t* として格納) */
} vosTaskCB_t;

/* Kernel control block (最小) */
typedef struct {
    bool            start_kernel;
    vosTaskQueHdr_t run_task;
    vosTaskQueHdr_t ready_que;
    vosTaskQueHdr_t wait_que;
    vosTaskQueHdr_t stop_que;
} vosKernelCB_t;

/* README に記載のグローバル配列 */
static vosTaskCB_t   g_vosTaskCB[VOS_TASK_NUM];
static vosKernelCB_t g_vosKernelCB;

/* メッセージ関連グローバル */
static vosMsgHdr_t  g_vosMsgBuff_t[VOS_TOTAL_MSG_NUM];
static vosMsgCB_t   g_vosMsgCB[VOS_MSGQUE_NUM];

/* 内部管理変数: g_vosMsgBuff_t の割当インデックス */
static uint32_t g_vosMsgBuffAllocIdx = 0;

/* プロトタイプ (README 仕様に合わせる) */
vosTaskHandle_t  vosCreateTask(int32_t (*task)(int32_t, char**), uint32_t pri, uint32_t stack_size, uint32_t *stack);
vosMsgHandle_t   vosCreateMsgQue(uint32_t msg_num, uint32_t msg_size, uint32_t *msg_pool);

/* ヘルパ: READY キューに優先度順に挿入 (数値が大きいほど優先度が高い、と仮定) */
static void insert_ready_queue(vosTaskCB_t *new_tcb)
{
    if (new_tcb == NULL) return;

    /* ヘッダの next_ptr が NULL なら空キュー */
    vosTaskCB_t *prev = NULL;
    vosTaskCB_t *cur = (vosTaskCB_t*)g_vosKernelCB.ready_que.next_ptr;

    /* 挿入位置を探す: cur の優先度が新規より大きい/等しい場合は先へ進む (FIFO for same pri)
       つまり first cur with cur->task_pri < new_tcb->task_pri の直前に挿入 */
    while (cur != (vosTaskCB_t*)VOS_INIT_PTR) {
        if (cur->task_pri < new_tcb->task_pri) {
            break; /* この cur の前に new を挿入 */
        }
        prev = cur;
        cur = cur->next_ptr;
    }

    if (prev == NULL) {
        /* 先頭に挿入 */
        new_tcb->next_ptr = (vosTaskCB_t*)g_vosKernelCB.ready_que.next_ptr;
        g_vosKernelCB.ready_que.next_ptr = (vosTaskHandle_t)new_tcb;
    } else {
        /* prev の次に挿入 */
        new_tcb->next_ptr = prev->next_ptr;
        prev->next_ptr = new_tcb;
    }
}

/* 実装 */

vosTaskHandle_t vosCreateTask(int32_t (*task)(int32_t, char**), uint32_t pri, uint32_t stack_size, uint32_t *stack)
{
    if (task == NULL || stack == NULL || stack_size == 0) {
        return (vosTaskHandle_t)0; /* 失敗: パラメータ不正 */
    }

    /* 空きTCBを探索 (task_func == VOS_INIT_PTR を未登録とみなす) */
    for (uint32_t i = 0; i < VOS_TASK_NUM; ++i) {
        if (g_vosTaskCB[i].task_func == (uint32_t*)VOS_INIT_PTR) {
            /* 初期化 */
            g_vosTaskCB[i].next_ptr = (vosTaskCB_t*)VOS_INIT_PTR;
            g_vosTaskCB[i].wait_svc.msg_cb = (vosMsgCB_t*)VOS_INIT_PTR;
            g_vosTaskCB[i].task_pri = pri;
            g_vosTaskCB[i].stack_size = stack_size;
            g_vosTaskCB[i].stack_top = stack;
            /* C の関数ポインタを uint32_t* に格納するためキャスト (README のデータ設計に準拠) */
            g_vosTaskCB[i].task_func = (uint32_t*)task;

            /* 生成時に自動で READY キューに挿入する */
            insert_ready_queue(&g_vosTaskCB[i]);

            return &g_vosTaskCB[i];
        }
    }

    /* 空きなし */
    return (vosTaskHandle_t)0;
}

vosMsgHandle_t vosCreateMsgQue(uint32_t msg_num, uint32_t msg_size, uint32_t *msg_pool)
{
    if (msg_num == 0 || msg_size == 0 || msg_pool == NULL) {
        return (vosMsgHandle_t)0;
    }

    /* メッセージヘッダ領域の確保をチェック */
    if (g_vosMsgBuffAllocIdx + msg_num > VOS_TOTAL_MSG_NUM) {
        return (vosMsgHandle_t)0; /* メッセージヘッダ不足 */
    }

    /* 空きの MsgCB を見つける (msg_num == 0 を未使用フラグとする) */
    for (uint32_t i = 0; i < VOS_MSGQUE_NUM; ++i) {
        if (g_vosMsgCB[i].msg_num == 0) {
            vosMsgCB_t *cb = &g_vosMsgCB[i];

            /* 初期化 */
            cb->wait_task.next_ptr = (vosTaskHandle_t)VOS_INIT_PTR;
            cb->msg_que.next_ptr = (vosMsgHdr_t*)VOS_END_PTR; /* 空キュー */
            cb->msg_que.msg_ptr = (uint32_t*)VOS_INIT_PTR;
            cb->msg_buff = &g_vosMsgBuff_t[g_vosMsgBuffAllocIdx];
            cb->free_idx = 0;

            /* msg_pool をユーザーが提供する連続領域と仮定して、各ヘッダに対応するポインタをセット */
            for (uint32_t j = 0; j < msg_num; ++j) {
                vosMsgHdr_t *hdr = &g_vosMsgBuff_t[g_vosMsgBuffAllocIdx + j];
                hdr->next_ptr = (j + 1 < msg_num)
                    ? &g_vosMsgBuff_t[g_vosMsgBuffAllocIdx + j + 1]
                    : (vosMsgHdr_t*)VOS_END_PTR; /* 最後は終端 */
                /* msg_size を uint32_t 単位として、msg_pool の該当オフセットをセット */
                hdr->msg_ptr = &msg_pool[j * msg_size];
            }

            /* 管理情報 */
            cb->msg_num = msg_num;
            cb->msg_size = msg_size;
            cb->msg_pool = msg_pool;

            /* 次回割当のためインデックスを進める */
            g_vosMsgBuffAllocIdx += msg_num;

            return cb;
        }
    }

    /* 空きMsgCBなし */
    return (vosMsgHandle_t)0;
}

/* ライブラリ初期化ヘルパ (任意): TCB の task_func を初期化しておく */
void vos_init_globals(void)
{
    for (uint32_t i = 0; i < VOS_TASK_NUM; ++i) {
        g_vosTaskCB[i].task_func = (uint32_t*)VOS_INIT_PTR;
        g_vosTaskCB[i].next_ptr = (vosTaskCB_t*)VOS_INIT_PTR;
        g_vosTaskCB[i].wait_svc.msg_cb = (vosMsgCB_t*)VOS_INIT_PTR;
    }
    for (uint32_t i = 0; i < VOS_MSGQUE_NUM; ++i) {
        g_vosMsgCB[i].msg_num = 0;
        g_vosMsgCB[i].msg_buff = (vosMsgHdr_t*)VOS_INIT_PTR;
        g_vosMsgCB[i].msg_pool = (uint32_t*)VOS_INIT_PTR;
    }
    g_vosMsgBuffAllocIdx = 0;
    g_vosKernelCB.start_kernel = false;
    /* READY キューを空にする */
    g_vosKernelCB.ready_que.next_ptr = (vosTaskHandle_t)VOS_INIT_PTR;
}
