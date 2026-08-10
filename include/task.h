#include <stdint.h>

/**
 * @function vos_createTask
 * @param[I] task タスク関数
 * @param[I] pri タスク優先度
 * @param[I] stack_size タスクスタックサイズ
 * @param[I] stack タスクスタック領域
 * @retval 1以上：生成したタスクID
 * @retval 0以下：生成失敗APIリターンコード `vos.h`参照
 * @brief 新規ユーザタスクを生成して実行可能状態にする。
 * ユーザタスクの引数は、C言語のmain関数と同様の引数を渡すことが可能。
 * スタック領域は、他のタスクのスタック領域と重複あるいは領域が重なり合ってはならない。
 * 生成されたタスクは、タスク管理によりTCBにエントリーされ管理される。
 * 生成されたタスクは、タスク優先度順にREADYキューにエントリーされる。
 * @note タスクIDは、0：VOSアイドルタスク、1以上：ユーザタスクである。
 */
extern int32_t vos_createTask(int32_t (*task)(int32_t, char**), uint32_t pri, uint32_t stack_size, uint32_t *stack);

/**
 * @function vos_startKernel
 * @brief VOSを起動する。
 * READYキュー先頭のタスクを実行状態にして、実行する。
 */
extern void vos_startKernel(void);
