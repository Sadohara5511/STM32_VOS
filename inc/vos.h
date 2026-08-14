/**
 * マルチタスクOS(VOS) ヘッダーファイル
 */

/* VOS基本定数・データ型 */
#define VOS_END_PTR     (void*)(-1)     /* 端点(番人)ポインタ値 */
#define NUL             (NULL)          /* 初期ポインタ値 */

/* VOS APIのエラーリターンコード */
typedef enum {
    VOS_ERR_PARAM = -1,                 /* パラメータエラー */
    VOS_ERR_RESOURCE = -2,              /* リソース不足 */
    VOS_ERR_OVER_RES = -3,              /* リソース超過指定 */
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


/* プロトタイプ */
void vos_initKernel(void);
vosTaskHandle_t  vosCreateTask(int32_t (*task)(int32_t, char**), uint32_t pri, uint32_t stack_size, uint32_t *stack);
vosMsgHandle_t   vosCreateMsgQue(uint32_t msg_num, uint32_t msg_size, uint32_t *msg_pool);
