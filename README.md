<div style="font-size: 22pt;">マルチタスクOS(VOS) 設計書</div>

# 目次
- [概要](#1概要)
    - [目的](#1-1目的)
- [システム構成](#2システム構成)
- [機能](#3機能)
- [データ構造](#4データ構造)

---
# 1.概要
## 1-1.目的
軽量リアルタイムOSを実装する。
- [先の予定] :メモリ不足などによりRTOSがポーティングできない場合でも、マルチタスク風のプログラム構成の実装設計にしたい場合やマルチタスク駆動方式を取り入れたい等の要求を実現する。

## 1-2.実装方針
- 一般的なRTOS同様、main関数にてコアプログラム及びユーザタスクを繰り返し実行可能なループ実装にする。
- RTOSにて利用されてるシステムTICK割り込みを使用して、mainループのイベント待ち処理を解除する実装にする。

## 1-3.用語説明
タスクの状態について、以下の状態がある。機能APIコールやディスパッチャによりタスクの状態が遷移する。
- `未登録状態`:NON-EXIST
- `休止状態`:DORMANT
- `実行可能状態`:READY
- `実行状態`:RUN
- `待ち状態`:WAIT

---
# 2.システム構成
```marmaidまたはplantumlによる作図
```

---
# 3.機能
## 3-1.タスク管理機能
タスク管理機能は、タスクの生成・削除・起動・終了、タスク状態の設定・参照する機能である。
タスク管理は、タスクコントロールブロック(TCB)により管理する。

### 3-1-1.データ構造

## 3-2.機能 API
### 3-2-1.タスクの生成
**プロトタイプ**
```
int32_t vos_createTask(int32_t (*task)(int32_t, char**), uint32_t pri, uint32_t stack_size, uint32_t *stack);
```

**パラメータ**
```
int32_t (*task)(int32_t argc, char **argv):タスクの関数アドレス
uint32_t pri:タスクの優先度
uint32_t stack_size:タスクのスタックサイズ
uint32_t *stack:タスクのスタック領域
```

**リターン**
```
1以上:生成成功。生成したタスクID。
[要検討] 0:生成失敗。失敗要因は `vos_getError()`関数にて取得可能。
[要検討] -1以下:生成失敗。失敗要因は `vos_getError()`関数のリターン値参照。
```

**機能説明**
タスクの状態が`未登録状態`から`休止状態`あるいは`実行可能状態`に遷移する。vos_startKernel()が実行されてない状態では、`未登録状態`から`休止状態`に遷移するだけですが、vos_startKernel()コール前(未実行)状態から実行状態に遷移した場合、vos_startKernel()により`休止状態`から`実行可能状態`に自動遷移する。

**補足説明**

---
# 4.データ構造
# 4-1.TCB
タスクコントロールブロック
```
/* OS全体のタスク数(ユーザータスク+IDLEタスク) */
#define VOS_TASK_NUM            (VOS_USER_TASK_NUM +1)

/* TCB構造体 */
typedef struct tag_VOS_TCB_t VOS_TCB_t;
struct VOS_TCB_t {
    VOS_TCB_t      *tcb_ptr;        /* TCBリストポインタ */
    VOS_STATE_e     task_state;     /* タスク状態:タスク状態キューにて管理するので無くてもいいかも */
    uint32_t        task_pri;       /* タスク優先度 */
    uint32_t        stack_size;     /* スタック領域サイズ(単位:32bit) */
    uint32_t       *stacK_top;      /* スタック領域先頭アドレス */
    uint32_t       *task_ptr;       /* タスク実行アドレス */
    register        reg[16];        /* ディスパッチ前のCPUレジスタ値 */
};
VOS_TCB_t       g_vos_tcb[VOS_TASK_NUM];    /* [0]はIDLEタスク、[1~N]ユーザータスク */
```

OS制御
```
typedef struct {
    bool             start_kernel;  /* カーネルStart/Stop */
    VOS_TCB_t       *run_ptr;       /* RUNタスク */
    VOS_TCB_t       *ready_que;     /* READYキュー */
    VOS_TCB_t       *wait_que;      /* WAITキュー */
    VOS_TCB_t       *stop_que;      /* 休止状態キュー */
} VOS_OS_CTRL_t;
VOS_OS_CTRL_t       g_vos_os_ctrl;
```

# 4-2.SCB
サービスコントロールブロック
```
typedef enum {
    VOS_MESSAGE_QUE,
    VOS_EVENT_FLAG,
    VOS_SEMAPH,
    VOS_SERVICE_MAX
} VOS_SERVICE_e;
typedef struct {
    VOS_SERVICE_e   service;
    ...
} VOS_SCB_t;
VOS_SCB_t       g_vos_scb[VOS_SERVICE_MAX];
```

# 4-3.スタック領域サイズ
タスクのスタック領域構造
```
typedef struct {
    uint32_t        safety[4];      /* スタックオーバー検知領域 */
    uint32_t        stack[1];       /* スタック領域 */
} VOS_STACK_t;
```

# 5.機能詳細
## 5-1.タスク管理 詳細
### タスク管理 動作の流れ
- 1.vos_dispatch() が呼ばれると、CPUに「処理が一段落したらPendSV例外を発行して」と合図が送られます。
- 2.他の優先度の高い割り込み（タイマーなど）がなければ、即座に PendSV_Handler が起動します。
- 3.ハンドラ起動時、Cortex-Mのハードウェア構造によって、自動的にその時点の R0-R3, R12, LR, PC, xPSR がスタック（PSP）に自動保存されます。
- 4.アセンブリ内で、残り半分の R4-R11 を手動で同じスタックに保存します。これで前回の vos_createTask で作った初期状態と全く同じ形の綺麗な「レジスタの塊」がスタック上にできあがります。
- 5.あとは、動かしたい次のタスクのスタックから逆の手順でレジスタを復元し、bx lr で元のタスク空間へジャンプします。

このディスパッチャをシステムに組み込むにあたり、以下の点で追加の実装や解説は必要になるだろう。仕様執筆してから実装する見込み。
- 最初のタスクを安全に起動するための「OS起動開始関数 vos_startKernel()」のコード
- レジスタの保存位置（オフセット値など）をC言語の構造体と自動で同期・一致させるための注意点
- タイマー割り込み（SysTick）を用いて一定時間ごとにタスクを切り替える（ラウンドロビン・タイムスライス）ための仕組みの追加
