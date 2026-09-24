/**
 * マルチタスクOS(VOS) 
 * ユーザーコンフィグレーション・ファイル
 */
#ifndef _VOS_CONFIG_H_
#define _VOS_CONFIG_H_

/* VOSユーザーが決定する定数（リソース定数） */
#define VOS_TASK_NUM            (2)     /* ユーザータスク数 */
#define VOS_MSGQUE_NUM          (2)     /* メッセージキュー数 */
#define VOS_EVT_NUM             (0)     /* イベントフラグ数 */
#define VOS_SEM_NUM             (0)     /* セマフォ数 */
#define VOS_QUE1_MSGBUFF_NUM    (2)     /* メッセージキュー１のメッセージバッファ数 */
#define VOS_QUE2_MSGBUFF_NUM    (2)     /* メッセージキュー２のメッセージバッファ数 */
#define VOS_QUE3_MSGBUFF_NUM    (0)     /* メッセージキュー３のメッセージバッファ数 */
#define VOS_TOTAL_MSG_NUM       (VOS_QUE1_MSGBUFF_NUM \
								+ VOS_QUE2_MSGBUFF_NUM \
                                + VOS_QUE3_MSGBUFF_NUM)

/* VOSユーザーが決定するディスパッチ方式 */
#define VOS_EVENT_DRIVEN        (1)
#define VOS_TIME_SLICE          (2)
#define VOS_DISPATCH            VOS_EVENT_DRIVEN

/* VOSタスク優先度 */
typedef enum {
    VOS_IDLE_TASK_PRI = 0,              /* IDLEタスク優先度 */
    /* VOSユーザーが決定するタスク優先度 */
    VOS_TASK_PRI_LO = 1,                /* 最低優先度 */
    VOS_TASK_PRI_MID = 4,
    VOS_TASK_PRI_HI = 7                 /* 最高優先度 */
} VOS_TASKPRI_e;

/* VOS ユーザへ提供するデバッグ機能 */
#define VOS_STACK_OVF_CHECK     true    /* スタック・オバーフロー・チェックの実施有無 */
#define VOS_API_PARAM_CHECK     true    /* APIのパラメータチェックの実施有無*/

/**
 * スタック・オバーフロー・チェックの実施有時は、
 * ユーザーはスタックオーバー検知領域サイズを含むスタックサイズを指定すること
 */
/* VOSにて認識するタスクスタック領域構造 */
typedef struct {
#if(VOS_STACK_OVF_CHECK)
    uint32_t        safety[4];      /* スタックオーバー検知領域 */
#endif
    uint32_t        stack[1];       /* スタック領域 */
} VOS_STACK_t;

#endif /*_VOS_CONFIG_H_*/
