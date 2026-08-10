/**
 * Cortex-Mにおけるスタック初期化のポイントCortex-M（Cortex-M3/M4/M7など）は、関数呼び出しや例外発生時に特定のレジスタをスタックへ自動・手動で退避します。
 * タスクが初めて起動する際、あたかも「ディスパッチャ（割り込み）から復帰した」ように見せるため、
 * スタックの末尾（Cortex-Mは降順スタックのため高位アドレス側）に初期レジスタの値をあらかじめ偽装して配置（スタックの初期化）しておく必要があります。
 * ハードウェア自動スタック（8レジスタ）: xPSR, PC, LR, R12, R3, R2, R1, R0
 * ソフトウェア手動スタック（8レジスタ）: R11, R10, R9, R8, R7, R6, R5, R4
 * 引数の渡し方: 第一引数 argc は R0、第二引数 argv は R1 に配置してます。
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "vos_config.h"
#include "task.h"


/* OS全体のタスク数(ユーザータスク+IDLEタスク) */
#define VOS_TASK_NUM            (VOS_USER_TASK_NUM +1)

/* タスク状態の定義 */
typedef enum {
    VOS_STATE_DORMANT = 0,          /* 休止状態 */
    VOS_STATE_READY,                /* 実行可能状態 */
    VOS_STATE_RUN,                  /* 実行状態 */
    VOS_STATE_WAIT                  /* 待ち状態 */
} VOS_STATE_e;

/**
 * タスクコントロールテーブル(TCB)
 */
/* TCB構造体 */
/* 構造体の前方宣言 */
typedef struct tag_VOS_TCB_t VOS_TCB_t;

struct tag_VOS_TCB_t {
    VOS_TCB_t       *tcb_ptr;       /* リストポインタ */
    VOS_STATE_e      task_state;    /* タスク状態 */
    uint32_t         task_pri;      /* タスク優先度 */
    uint32_t         stack_size;    /* スタック領域サイズ(単位:32bit) */
    uint32_t        *stacK_top;     /* スタック領域先頭アドレス（配列の先頭メモリ） */
    uint32_t        *task_ptr;      /* タスク実行アドレス */
    uint32_t         reg[16];       /* ディスパッチ前のCPUレジスタ値 (R0-R15相当、またはSP保持用) */
    uint32_t        *sp;            /* 現在のスタックポインタ（独自追加：コンテキスト切り替え用） */
};


/**
 * OS制御
 */
typedef struct {
    bool             start_kernel;  /* カーネルStart/Stop状態 */
    VOS_TCB_t       *run_que;       /* RUNタスク */
    VOS_TCB_t       *ready_que;     /* READYキュー */
    VOS_TCB_t       *wait_que;      /* WAITキュー */
    VOS_TCB_t       *stop_que;      /* 休止状態キュー */
} VOS_OS_CTRL_t;


/**
 * グローバル変数定義
 */
VOS_TASK_CTRL_t     g_vos_os_ctrl;
VOS_TCB_t           g_vos_tcb[VOS_TASK_NUM];


/* エラーコードの定義 */
#define VOS_ERR_FULL     (-1)
#define VOS_ERR_PARAM    (-2)

/* クリティカルセクション（割り込み制御用API） */
extern void vos_disable_dispatch(void);
extern void vos_enable_dispatch(void);
/* タスク終了時に呼び出されるべきシステム関数 */
extern void vos_exitTask(void);

/**
 * @brief 新規ユーザタスクを生成して実行可能状態にする (ARM Cortex-M対応版)
 * 
 * @param[I] task タスク関数
 * @param[I] pri タスク優先度 (値が小さいほど高優先)
 * @param[I] stack_size タスクスタックサイズ (単位: 32bitワード数)
 * @param[I] stack タスクスタック領域の先頭アドレス
 * @param[I] argc タスクへ渡す引数の数
 * @param[I] argv タスクへ渡す引数文字列配列のポインタ
 * @retval 1以上：生成したタスクID、0以下：エラーコード
 */
int32_t vos_createTask(int32_t (*task)(int32_t, char**), uint32_t pri, uint32_t stack_size, uint32_t *stack, int32_t argc, char **argv)
{
    int32_t task_id = -1;
    VOS_TCB_t *p_tcb = NULL;
    uint32_t *p_stk = NULL;

    /* 1. 引数チェック */
    if ((task == NULL) || (stack == NULL) || (stack_size < 16)) { /* 最低限レジスタ分(16ワード)は必要 */
        return VOS_ERR_PARAM;
    }

    /* 2. 排他開始 */
    vos_disable_dispatch();

    /* 3. 空きTCBの検索（0番はアイドルタスク用としてスキップ） */
    for (int32_t i = 1; i < VOS_TASK_NUM; i++) {
        if (g_vos_tcb[i].task_state == VOS_STATE_DORMANT) {
            task_id = i;
            p_tcb = &g_vos_tcb[i];
            break;
        }
    }

    if (p_tcb == NULL) {
        vos_enable_dispatch();
        return VOS_ERR_FULL;
    }

    /* 4. TCBの基本情報設定 */
    p_tcb->tcb_ptr    = NULL;
    p_tcb->task_state = VOS_STATE_READY;
    p_tcb->task_pri   = pri;
    p_tcb->stack_size = stack_size;
    p_tcb->stacK_top  = stack;
    p_tcb->task_ptr   = (uint32_t *)task;

    /* 5. ARM Cortex-M 用のスタック初期化 (擬似コンテキスト作成) */
    /* Cortex-Mのスタックは「減少型(Full Descending)」のため、メモリの末尾（高位アドレス）から使う */
    p_stk = &stack[stack_size];

    /* --- ハードウェアが自動復帰するレジスタ領域 (8ワード) --- */
    p_stk--; *p_stk = 0x01000000UL;      /* xPSR : Thumbモードを有効にするため24ビット目を1にする */
    p_stk--; *p_stk = (uint32_t)task;    /* PC   : タスクの実行開始アドレス */
    p_stk--; *p_stk = (uint32_t)vos_exitTask; /* LR : タスクがreturnした際の戻り先（終了処理） */
    p_stk--; *p_stk = 0;                 /* R12  : 初期値0 */
    p_stk--; *p_stk = 0;                 /* R3   : 初期値0 */
    p_stk--; *p_stk = 0;                 /* R2   : 初期値0 */
    p_stk--; *p_stk = (uint32_t)argv;    /* R1   : 第2引数 (char**) */
    p_stk--; *p_stk = (uint32_t)argc;    /* R0   : 第1引数 (int32_t) */

    /* --- ソフトウェア(ディスパッチャ)が手動で復帰させるレジスタ領域 (8ワード) --- */
    p_stk--; *p_stk = 0;                 /* R11  : 初期値0 */
    p_stk--; *p_stk = 0;                 /* R10  : 初期値0 */
    p_stk--; *p_stk = 0;                 /* R9   : 初期値0 */
    p_stk--; *p_stk = 0;                 /* R8   : 初期値0 */
    p_stk--; *p_stk = 0;                 /* R7   : 初期値0 */
    p_stk--; *p_stk = 0;                 /* R6   : 初期値0 */
    p_stk--; *p_stk = 0;                 /* R5   : 初期値0 */
    p_stk--; *p_stk = 0;                 /* R4   : 初期値0 */

    /* 構築したスタックの現在地(SP)をTCBに保存 */
    p_tcb->sp = p_stk;

    /* 提示された構造体の要件を満たすため、reg配列にも値をコピー（任意） */
    for (int i = 0; i < 16; i++) {
        p_tcb->reg[i] = 0;
    }
    p_tcb->reg[0] = (uint32_t)argc;
    p_tcb->reg[1] = (uint32_t)argv;

    /* 6. READYキューへ優先度順（値が小さいほど高優先）に挿入 */
    if (g_vos_os_ctrl.ready_que == NULL) {
        g_vos_os_ctrl.ready_que = p_tcb;
    } else {
        VOS_TCB_t *prev = NULL;
        VOS_TCB_t *curr = g_vos_os_ctrl.ready_que;

        /* 自分より優先度が低い（値が大きい）タスクの手前を探す */
        /* 同じ優先度の場合は、既存のタスクの後ろ（FIFO）になるよう「>」判定 */
        while ((curr != NULL) && (curr->task_pri <= pri)) {
            prev = curr;
            curr = curr->tcb_ptr;
        }

        if (prev == NULL) {
            /* 先頭へ挿入（キュー内で最も高い優先度） */
            p_tcb->tcb_ptr = g_vos_os_ctrl.ready_que;
            g_vos_os_ctrl.ready_que = p_tcb;
        } else {
            /* 中間または末尾へ挿入 */
            p_tcb->tcb_ptr = curr;
            prev->tcb_ptr = p_tcb;
        }
    }

    /* 7. 排他解除 */
    vos_enable_dispatch();

    return task_id;
}

/**
 * @brief タスクを状態キューの末尾に追加する (FIFO)
 * @param p_que 対象のキュー構造体へのポインタ
 * @param p_tcb 追加するTCBへのポインタ
 * @return true: 成功, false: 失敗（引数不正など）
 */
bool vos_enque(VOS_TCB_t *p_que, VOS_TCB_t *p_tcb) {
    // 引数チェック
    if ((p_que == NULL) || (p_tcb == NULL)) {
        return false;
    }

    // 追加するTCBの次ポインタを初期化
    p_tcb->tcb_ptr = NULL;

    // キューが空の場合
    if (p_que->tcb_ptr == NULL) {
        p_que->tcb_ptr = p_tcb;
    } 
    // キューに既に要素がある場合（末尾まで線形探索）
    //[修正]同一優先度の最後尾に追加
    else {
        VOS_TCB_t *current = p_que->tcb_ptr;
        while (current->tcb_ptr != NULL) {
            current = current->tcb_ptr;
        }
        current->tcb_ptr = p_tcb;
    }
    return true;
}

/**
 * @brief 状態キューの先頭からタスクを取り出す (FIFO)
 * @param p_que 対象のキュー構造体へのポインタ
 * @return 取り出したTCBへのポインタ（キューが空の場合はNULL）
 */
void *vos_deque(VOS_TCB_t *p_que) {
    // 引数チェック
    if (p_que == NULL) {
        return NULL;
    }
    // 先頭のTCBを取得
    VOS_TCB_t *p_tcb = p_que->tcb_ptr;
    if (p_tcb == NULL) {
        return NULL;
    }

    // キューの先頭を次の要素に更新
    p_que->tcb_ptr = p_tcb->tcb_ptr;

    // 取り出したTCBのリンクを切り離す
    p_tcb->tcb_ptr = NULL;
    return (void *)p_tcb;
}

/**
 * @function vos_startKernel
 * @brief VOSを起動する。
 * READYキュー先頭のタスクを実行状態にして、実行する。
 */
bool vos_startKernel(void)
{
    VOS_TCB_t *run_que;
    g_vos_os_ctrl.start_kernel = true;
    run_que = vos_deque(g_vos_os_ctrl.ready_que);
    g_vos_os_ctrl.run_que = run_que;
    return (run_que != NULL);
}
