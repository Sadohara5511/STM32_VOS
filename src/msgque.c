#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "vos_config.h"
#include "vos.h"

/* メッセージコントロールブロック */
typedef struct tag_vosMsgHdr vosMsgHdr_t;
struct tag_vosMsgHdr {
    vosMsgHdr_t*    next_ptr;           /* 送受信メッセージチェイン */
    uint32_t*       msg_ptr;            /* メッセージバッファ・ポインタ(生成時にセットアップ) */
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

/* メッセージキュー機能グローバル */
static vosMsgHdr_t  g_vosMsgBuff_t[VOS_TOTAL_MSG_NUM];
static vosMsgCB_t   g_vosMsgCB[VOS_MSGQUE_NUM];
/* 内部管理変数 */
static uint32_t     g_vosMsgBuffAllocIdx;       /* g_vosMsgBuff_t の割当インデックス */
static vosError_e   g_vosMsgQueError;

/**
 *
 */
void vosInitMsgQue(void)
{
    vosMemset(&g_vosMsgBuff_t[0], 0, sizeof(g_vosMsgBuff_t));
    vosMemset(&g_vosMsgCB[0], 0, sizeof(g_vosMsgCB));
    g_vosMsgBuffAllocIdx = 0;
    g_vosMsgQueError = 0;
}

/**
 *
 */
vosMsgHandle_t vosCreateMsgQue(uint32_t msg_num, uint32_t msg_size, uint32_t *msg_pool)
{
    vosMsgHandle_t handle = NUL;  /* 空きなし */
    vosMsgHdr_t* hdr;
    uint32_t* buff;

#if(VOS_API_PARAM_CHECK)
    g_vosMsgQueError = 0;
    if (msg_num == 0 || msg_size == 0 || msg_pool == NUL) {
        g_vosMsgQueError = VOS_ERR_PARAM;
        return handle;
    }

    /* メッセージヘッダ領域の確保をチェック */
    if (g_vosMsgBuffAllocIdx + msg_num > VOS_TOTAL_MSG_NUM) {
        g_vosMsgQueError = VOS_ERR_OVER_RES;
        return handle;      /* VOS_TOTAL_MSG_NUM不足 */
    }
#endif

    /* 空きの MsgCB を見つける (msg_num == 0 を未使用フラグとする) */
    for (uint32_t i = 0; i < VOS_MSGQUE_NUM; i++) {
        if (g_vosMsgCB[i].msg_num == 0) {
            /* 初期化 */
            handle = &g_vosMsgCB[i];
            handle.wait_task.next_ptr = NUL;
            handle.msg_num = msg_num;
            handle.msg_size = msg_size;
            handle.msg_pool = msg_pool;
            handle.msg_buff = &g_vosMsgBuff_t[g_vosMsgBuffAllocIdx];
            handle.free_idx = 0;
            handle.msg_que.next_ptr = NUL;

            /* メッセージバッファ管理ヘッダーと対となるmsg_poolアドレス(メッセージバッファアドレス)をセット */
            hdr = &g_vosMsgBuff_t[g_vosMsgBuffAllocIdx];
            buff = msg_pool;
            for (uint32_t num = 0; num < msg_num; num++) {
                hdr->next_ptr = NUL;    /* NUL=バッファ未使用をセット */
                hdr->msg_ptr  = buff;
                hdr ++;
                buff += msg_size;
            }

            /* 次回割当のためインデックスを進める */
            g_vosMsgBuffAllocIdx += msg_num;
            return handle;
        }
    }

    g_vosMsgQueError = VOS_ERR_RESOURCE;
    return handle;
}
