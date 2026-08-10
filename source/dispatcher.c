/**
 * Cortex-Mの標準的な設計に倣い、ディスパッチャ（PendSVの起動制御）をC言語関数としてモジュール化し、
 * 実際のコンテキスト切り替えをPendSVハンドラ（アセンブリ）で行う構成にしています。
 * また、浮動小数点演算（FPU）は非使用の前提としています。
 */
#include <stdint.h>
#include "dispatcher.h"


/* Cortex-M システム制御レジスタの定義 */
#define NVIC_INT_CTRL_REG      (*(volatile uint32_t *)0xE000ED04)
#define NVIC_PENDSVSET_BIT     (1UL << 28)

/**
 * @brief タスク切り替え（ディスパッチ）を要求するモジュール
 * OSの各API（タスク生成やウェイトなど）の最後や、タイマー割り込みからこの関数を呼び出すことで、タスクの再スケジューリング（切り替え）を要求します。
 * @note この関数を呼ぶとPendSV例外がペンディングされ、
 *       他の割込み処理などがすべて終わった安全なタイミングでPendSV_Handlerが起動します。
 */
void vos_dispatch(void)
{
    /* PendSV例外を発行（セット）する */
    NVIC_INT_CTRL_REG = NVIC_PENDSVSET_BIT;

    /* データ同期バリア（確実に発行を完了させる） */
    __asm volatile ("dsb" : : : "memory");
    __asm volatile ("isb" : : : "memory");
}
