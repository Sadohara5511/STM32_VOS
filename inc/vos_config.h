/**
 * マルチタスクOS(VOS) 
 * ユーザーコンフィグレーション・ファイル
 */

/* VOSユーザーが決定する定数（リソース定数） */
#define VOS_TASK_NUM            (3)     /* ユーザータスク数 */
#define VOS_MSGQUE_NUM          (3)     /* メッセージキュー数 */
#define USER_QUE1_MSGBUFF_NUM   (2)     /* メッセージキュー１のメッセージバッファ数 */
#define USER_AUE2_MSGBUFF_NUM   (2)     /* メッセージキュー２のメッセージバッファ数 */
#define USER_QUE3_MSGBUFF_NUM   (2)     /* メッセージキュー３のメッセージバッファ数 */
#define VOS_TOTAL_MSG_NUM       (USER_QUE1_MSGBUFF_NUM
                                +USER_AUE2_MSGBUFF_NUM
                                +USER_QUE3_MSGBUFF_NUM)

/* VOS ユーザへ提供するデバッグ機能 */
#define VOS_STACK_OVF_CHECK     true    /* スタック・オバーフロー・チェックの実施有無 */
#define VOS_API_PARAM_CHECK     true    /* APIのパラメータチェックの実施有無*/
