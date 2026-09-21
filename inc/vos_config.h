/**
 * マルチタスクOS(VOS) 
 * ユーザーコンフィグレーション・ファイル
 */
/**
#ifndef _VOS_CONFIG_H_
#define _VOS_CONFIG_H_

/* VOSユーザーが決定する定数（リソース定数） */
#define VOS_TASK_NUM            (3)     /* ユーザータスク数 */
#define VOS_MSGQUE_NUM          (3)     /* メッセージキュー数 */
#define USER_QUE1_MSGBUFF_NUM   (2)     /* メッセージキュー１のメッセージバッファ数 */
#define USER_QUE2_MSGBUFF_NUM   (2)     /* メッセージキュー２のメッセージバッファ数 */
#define USER_QUE3_MSGBUFF_NUM   (2)     /* メッセージキュー３のメッセージバッファ数 */
#define VOS_TOTAL_MSG_NUM       (USER_QUE1_MSGBUFF_NUM
                                +USER_AUE2_MSGBUFF_NUM
                                +USER_QUE3_MSGBUFF_NUM)
#define VOS_EVT_NUM             (1)     /* イベントフラグ数 */
#define VOS_SEM_NUM             (1)     /* セマフォ数 */

/* VOSユーザーが決定するタスク優先度範囲 */
enum {
    TASK_PRI_LO = 1,                    /* 最低優先度 */
    TASK_PRI_MID = 4,
    TASK_PRI_HI = 7                     /* 最高優先度 */
};

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
