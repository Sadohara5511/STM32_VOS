#include "vos.h"

#if (VOS_EVT_NUM != 0)

uint32_t	g_vosEvtCB_Counter;
vosEvtCB_t  g_vosEvtCB[VOS_EVT_NUM];

/**
 * [API] Event flag create
 * @param[in] evtflag_ptr	=!NULL:Event flag address/=NULL:Event flag auto set
 * @return	Event flag handle
 */
vosEvtHandle_t vosEvtFlagCreate(uint32_t *evtflag_ptr)
{
	vosEvtCB_t*	ecb = NUL;
    if (g_vosEvtCB_Counter < VOS_EVT_NUM) {
    	ecb = &g_vosEvtCB[g_vosEvtCB_Counter];
    	if (evtflag_ptr == NUL) {
    		ecb->evt_flg_ptr = &ecb->evt_flg;
    	}else{
        	ecb->evt_flg_ptr = evtflag_ptr;
        	*evtflag_ptr = 0U;
    	}
    	g_vosEvtCB_Counter++;
		g_vosKernelCB.run_task.next_ptr->api_err = VOS_OK;
    }else{
		g_vosKernelCB.run_task.next_ptr->api_err = VOS_NO_RESOURCE;
    }
    return ecb;
}

uint32_t vosEvtFlagWait(vosEvtHandle_t handle, uint32_t wait_bit)
{
    vosTaskCB_t *next;
#if VOS_API_PARAM_CHECK
    if (handle == NUL) {
		g_vosKernelCB.run_task.next_ptr->api_err = VOS_INVALID_HANDLE;
        return 0U;
    }
#endif
	if (handle->wait_task.next_ptr != NUL) {
		g_vosKernelCB.run_task.next_ptr->api_err = VOS_INVALID_API;
		return 0U;
	}
	g_vosKernelCB.run_task.next_ptr->api_err = VOS_OK;
    if (!(handle->evt_flg & wait_bit)) {
    	handle->wait_task.next_ptr = g_vosKernelCB.run_task.next_ptr;
        /* 実行中タスクは、次状態をWAIT状態にセットアップする */
    	g_vosKernelCB.run_task.next_ptr->next_state = VOS_WAIT;
        /* READYキューのタスクにディスパッチする */
    	next = vosTaskDeque(&g_vosKernelCB.ready_que);
        if (next == NUL) {
    		g_vosKernelCB.run_task.next_ptr->api_err = VOS_NOTHING_TASK;
            return 0U;
        }
        vosTaskDispatch(next);
    	handle->wait_task.next_ptr = NUL;
    }
    return handle->evt_flg;
}

bool vosEvtFlagPost(vosEvtHandle_t handle, uint32_t post_bit)
{
    vosTaskCB_t *next;
#if VOS_API_PARAM_CHECK
    if (handle == NUL) {
		g_vosKernelCB.run_task.next_ptr->api_err = VOS_INVALID_HANDLE;
        return false;
    }
#endif
    handle->evt_flg |= post_bit;
    /* 待ちタスクがあれば RUNタスクにディスパッチ、或いはREADYキューへつなぐ */
	next = handle->wait_task.next_ptr;
	if (next != NUL) {
        if (vosTargetTaskDeque(&g_vosKernelCB.wait_que, next)) {
        	if (g_vosKernelCB.run_task.next_ptr->task_pri < next->task_pri) {
        		g_vosKernelCB.run_task.next_ptr->next_state = VOS_READY;
			    vosTaskDispatch(next);
        	}else{
				vosTaskEnque(&g_vosKernelCB.ready_que, next);
        	}
        }
	}
	g_vosKernelCB.run_task.next_ptr->api_err = VOS_OK;
    return true;
}

bool vosEvtFlagClear(vosEvtHandle_t handle, uint32_t clear_bit)
{
#if VOS_API_PARAM_CHECK
    if (handle == NUL) {
		g_vosKernelCB.run_task.next_ptr->api_err = VOS_INVALID_HANDLE;
        return false;
    }
#endif
    handle->evt_flg &= ~clear_bit;
	g_vosKernelCB.run_task.next_ptr->api_err = VOS_OK;
    return true;
}

#endif /*(VOS_EVT_NUM != 0)*/
