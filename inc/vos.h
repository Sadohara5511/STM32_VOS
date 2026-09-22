/**
 * マルチタスクOS(VOS) ヘッダーファイル
 */
#ifndef _VOS_H_
#define _VOS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "vos_config.h"

/* VOS基本定数・データ型 */
#define VOS_END_PTR     (void*)(-1)     /* リストの端点(番人)ポインタ値 */
#define NUL             (NULL)          /* 初期ポインタ値 */

/* VOS APIのエラーリターンコード */
typedef enum {
    VOS_OK = 0,
    VOS_INVALID_PARAM = -1,             /* APIパラメータ不正 */
    VOS_INVALID_HANDLE = -2,            /* 無効なハンドル */
    VOS_INVALID_API = -3,               /* 無効なAPIコール */
    VOS_MSG_QUEUE_FULL = -4,            /* メッセージキューがFULL */
    VOS_MSG_BUFF_EMPTY = -5,            /* メッセージバッファがEMPTY */
    VOS_NO_RESOURCE = -6,               /* リソース不足 */
    VOS_OVER_RESOURCE = -7,             /* リソース超過 */
} vosError_e;

/* VOS基本データ型(ビルド時の構造体前方宣言) */
typedef struct tag_vosMsgCB vosMsgCB_t;
typedef struct tag_vosEvtCB vosEvtCB_t;
typedef struct tag_vosSemCB vosSemCB_t;
typedef struct tag_vosTaskCB vosTaskCB_t;
typedef vosMsgCB_t* vosMsgHandle_t;     /* メッセージキューハンドル */
typedef vosEvtCB_t* vosEvtHandle_t;     /* イベントフラグハンドル */
typedef vosSemCB_t* vosSemHandle_t;     /* セマフォハンドル */
typedef vosTaskCB_t* vosTaskHandle_t;   /* タスクハンドル */

/* カーネルコントロールブロック */
typedef struct {
    bool            start_kernel;       /* カーネルStart/Stop */
    vosTaskQueHdr_t run_task;           /* RUNタスク */
    vosTaskQueHdr_t ready_que;          /* READYキュー */
    vosTaskQueHdr_t wait_que;           /* WAITキュー */
    vosTaskQueHdr_t dormant_que;        /* DORMANTキュー */
} vosKernelCB_t;

/* カーネルコントロールブロック変数宣言 */
vosKernelCB_t       g_vosKernelCB;

/* タスクコントロールブロック */
struct tag_vosTaskCB {
    vosTaskCB_t*    next_ptr;           /* タスクコントロールブロック・リストポインタ */
    union {
        vosMsgCB_t* msg_cb;             /* メッセージキュー */
        vosEvtCB_t* evt_cb;             /* イベントフラグ */
        vosSemCB_t* sem_cd;             /* セマフォ */
    }wait_svc;                          /* 受信待ちサービス */
    uint32_t        task_pri;           /* タスク優先度 */
    uint32_t        stack_size;         /* スタック領域サイズ(単位:32bit) */
    uint32_t*       stacK_top;          /* スタック領域先頭アドレス */
    uint32_t*       task_func;          /* タスク実行アドレス */
};

/* タスク状態キュー */
typedef struct {
    vosTaskCB_t *   next_ptr;
} vosTaskQueHdr_t;

/* タスクコントロールブロック変数宣言 */
vosTaskCB_t         g_vosTaskCB[VOS_TASK_NUM];

int32_t     g_vosCriticalCounter;         /* 割り込み抑止解除カウンタ */


#if(VOS_MSGQUE_NUM != 0)                   /* メッセージ数≠0 */
/* メッセージコントロールブロック */
typedef struct tag_vosMsgHdr vosMsgHdr_t;
struct tag_vosMsgHdr {
    vosMsgHdr_t *   next_ptr;           /* 送受信メッセージチェイン */
    uint32_t *      msg_ptr;            /* メッセージバッファ・ポインタ(生成時にセットアップ) */
};

typedef struct {
    vosTaskQueHdr_t wait_task;          /* 受信待ちタスクコントロールブロック・ポインタ */
    /* メッセージプール */
    uint32_t        msg_num;            /* メッセージバッファ数 */
    uint32_t        msg_size;           /* メッセージバッファ１つのサイズ */
    uint32_t *      msg_pool;           /* メッセージバッファ領域 */
    vosMsgHdr_t *   msg_buff;           /* メッセージバッファ先頭ポインタ */
    uint32_t        free_idx;           /* メッセージバッファ空きインデックス[0~msg_num-1] */
    vosMsgHdr_t     msg_que;            /* 送受信メッセージキュー */
} vosMsgCB_t;

/* メッセージコントロールブロック変数宣言 */
vosMsgHdr_t         g_vosMsgBuff_t[VOS_TOTAL_MSG_NUM]
vosMsgCB_t          g_vosMsgCB[VOS_MSGQUE_NUM];
#endif  /*(VOS_MSGQUE_NUM != 0)*/


#if(VOS_EVT_NUM != 0)                   /* イベントフラグ数≠0 */
/* イベントフラグコントロールブロック */
struct tag_vosEvtCB {
    uint32_t        evt_flg;            /* イベントフラグ */
    vosTaskQueHdr_t wait_task;          /* 待ち状態タスクコントロールブロック・ポインタ */
};

/* イベントフラグコントロールブロック変数宣言 */
vosEvtCB_t          g_vosEvtCB[VOS_EVT_NUM];
#endif  /*(VOS_EVT_NUM != 0)*/


#if(VOS_SEM_NUM != 0)                   /* セマフォ数≠0 */
/* セマフォコントロールブロック */
struct tag_vosSemCB {
    int32_t         sem_cnt;            /* セマフォカウンタ */
    int32_t         init_cnt;           /* セマフォカウンタ初期値 */
    vosTaskQueHdr_t wait_task;          /* 待ち状態タスクコントロールブロック・ポインタ */
};

/* セマフォコントロールブロック変数宣言 */
vosSemCB_t          g_vosSemCB[VOS_SEM_NUM];
#endif  /*(VOS_SEM_NUM != 0)*/


/* プロトタイプ */
void vos_initKernel(void);
vosTaskHandle_t  vosCreateTask(int32_t (*task)(int32_t, char**), uint32_t pri, uint32_t stack_size, uint32_t *stack);
vosMsgHandle_t   vosCreateMsgQue(uint32_t msg_num, uint32_t msg_size, uint32_t *msg_pool);

#endif /*_VOS_H_*/
