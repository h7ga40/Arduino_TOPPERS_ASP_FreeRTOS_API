
#include <ToppersASP_FreeRTOS_API.h>
#include <kernel.h>
#include "kernel_id.h"
#include <stdlib.h>
#include <t_syslog.h>

extern "C"
void *pvPortMalloc(size_t xSize)
{
	return malloc(xSize);
}

extern "C"
void vPortFree(void *pv)
{
	free(pv);
}

uint_t lock_count;

extern "C"
void vPortEnterCritical(void)
{
	if (lock_count == 0)
		loc_cpu();

	lock_count++;
}

extern "C"
void vPortExitCritical(void)
{
	lock_count--;

	if (lock_count == 0)
		unl_cpu();
}

extern void StartToppersASP(void);

extern "C"
void vTaskStartScheduler(void)
{
	StartToppersASP();
}

typedef struct tskTaskControlBlock
{
	ID tskid;
	const char *pcName;
} TCB_t;

TCB_t FreeRTOS_TCB[10];

extern "C"
BaseType_t xTaskCreate(TaskFunction_t pxTaskCode, const char *const pcName,
	const configSTACK_DEPTH_TYPE usStackDepth, void *const pvParameters,
	UBaseType_t uxPriority, TaskHandle_t *const pxCreatedTask)
{
	T_CTSK  ctsk;
	ER    ercd;

	TCB_t *pTCB = nullptr;
	ID tskid = TASK1;
	TCB_t *end = &FreeRTOS_TCB[sizeof(FreeRTOS_TCB) / sizeof(FreeRTOS_TCB[0])];
	for (TCB_t *pos = FreeRTOS_TCB; pos < end; pos++, tskid++) {
		if (pos->tskid == 0) {
			pTCB = pos;
			pTCB->tskid = tskid;
			pTCB->pcName = pcName;
			break;
		}
	}

	if (pTCB == nullptr)
		return pdFAIL;

	ctsk.tskatr = TA_ACT;
	ctsk.exinf = (intptr_t)pvParameters;
	ctsk.task = (TASK)pxTaskCode;
	if (TMIN_TPRI > TMAX_TPRI - uxPriority)
		ctsk.itskpri = TMIN_TPRI;
	else
		ctsk.itskpri = TMAX_TPRI - uxPriority;
	ctsk.stksz = usStackDepth * sizeof(StackType_t);
	ctsk.stk = NULL;
	ercd = cre_tsk(pTCB->tskid, &ctsk);
	if (ercd != E_OK) {
		syslog(LOG_ERROR, "cre_tsk error %d", ercd);
		pTCB->tskid = 0;
		pTCB->pcName = nullptr;
		return pdFAIL;
	}

	*pxCreatedTask = pTCB;

	return pdTRUE;
}

extern "C"
void vTaskSuspend(TaskHandle_t xTaskToSuspend)
{
	TCB_t *pTCB = nullptr;
	ER    ercd;

	int index = ((int)xTaskToSuspend - (int)FreeRTOS_TCB) / sizeof(FreeRTOS_TCB[0]);
	if ((index < 0) || (index >= (int)FreeRTOS_TCB / sizeof(FreeRTOS_TCB[0])))
		return;

	pTCB = &FreeRTOS_TCB[index];
	ercd = sus_tsk(pTCB->tskid);
	if (ercd != E_OK) {
		syslog(LOG_ERROR, "sus_tsk error %d", ercd);
	}
}

extern void ToppersASPDelayMs(uint32_t ms);

extern "C"
void vTaskDelay(const TickType_t xTicksToDelay)
{
	ER ercd = dly_tsk(xTicksToDelay * portTICK_PERIOD_MS);
	if (ercd != E_OK) {
		ToppersASPDelayMs(xTicksToDelay * portTICK_PERIOD_MS);
	}
}

extern "C"
TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
	ID tid;
	ER ercd = get_tid(&tid);
	if (ercd != E_OK) {
		syslog(LOG_ERROR, "get_tid error %d", ercd);
		return nullptr;
	}

	if ((tid >= TASK1) && (tid <= TASK10)) {
		TCB_t *pTCB = &FreeRTOS_TCB[tid - TASK1];
		if (pTCB->tskid == tid) {
			return pTCB;
		}
	}

	syslog(LOG_ERROR, "%d is not FreeRTOS task", ercd);
	return nullptr;
}

extern "C"
TickType_t xTaskGetTickCount(void)
{
	SYSTIM now = 0;
	ER ercd;

	ercd = get_tim(&now);
	if (ercd != E_OK) {
		return 0;
	}

	return (TickType_t)now;
}

typedef struct QueueDefinition {
	ID semid;
	uint8_t ucQueueType;
	ID xMutexHolder;
	UBaseType_t uxRecursiveCallCount;
} Queue_t;

Queue_t FreeRTOS_SEM[10];

#define QUEUE_TYPE_MUTEX 				( ( uint8_t ) 1U )
#define QUEUE_TYPE_COUNTING_SEMAPHORE	( ( uint8_t ) 2U )
#define QUEUE_TYPE_BINARY_SEMAPHORE		( ( uint8_t ) 3U )

extern "C"
SemaphoreHandle_t xSemaphoreGenericCreate(const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount)
{
	T_CSEM  csem;
	ER    ercd;

	Queue_t *pSEM = nullptr;
	ID semid = SEM1;
	Queue_t *end = &FreeRTOS_SEM[sizeof(FreeRTOS_SEM) / sizeof(FreeRTOS_SEM[0])];
	for (Queue_t *pos = FreeRTOS_SEM; pos < end; pos++, semid++) {
		if (pos->semid == 0) {
			pSEM = pos;
			pSEM->semid = semid;
			break;
		}
	}

	if (pSEM == nullptr)
		return nullptr;

	csem.sematr = TA_TPRI;
	csem.isemcnt = uxInitialCount;
	csem.maxsem = uxMaxCount;
	ercd = cre_sem(pSEM->semid, &csem);
	if (ercd != E_OK) {
		syslog(LOG_ERROR, "cre_sem error %d", ercd);
		pSEM->semid = 0;
		return nullptr;
	}

	return pSEM;
}

extern "C"
void vSemaphoreDelete(SemaphoreHandle_t xSemaphore)
{
}

extern "C"
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
	SemaphoreHandle_t xNewMutex;
	const UBaseType_t uxMaxCount = 1, uxInitialCount = 1;

	xNewMutex = xSemaphoreGenericCreate(uxMaxCount, uxInitialCount);
	if (xNewMutex != NULL) {
		xNewMutex->ucQueueType = QUEUE_TYPE_MUTEX;
		xNewMutex->xMutexHolder = NULL;
		xNewMutex->uxRecursiveCallCount = 0;
	}

	return xNewMutex;
}

extern "C"
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t xMutex, TickType_t xTicksToWait)
{
	BaseType_t xReturn;
	ID tid;
	ER ercd = get_tid(&tid);
	if (ercd != E_OK) {
		syslog(LOG_ERROR, "get_tid error %d", ercd);
		return pdFAIL;
	}

	if (xMutex->xMutexHolder == tid) {
		xMutex->uxRecursiveCallCount++;
		xReturn = pdPASS;
	}
	else {
		xReturn = xSemaphoreTake(xMutex, xTicksToWait);
		if (xReturn != pdFAIL) {
			xMutex->uxRecursiveCallCount++;
		}
	}

	return xReturn;
}

extern "C"
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t xMutex)
{
	BaseType_t xReturn;
	ID tid;
	ER ercd = get_tid(&tid);
	if (ercd != E_OK) {
		syslog(LOG_ERROR, "get_tid error %d", ercd);
		return pdFAIL;
	}

	if (xMutex->xMutexHolder == tid) {
		xMutex->uxRecursiveCallCount--;
		if (xMutex->uxRecursiveCallCount == 0) {
			(void)xSemaphoreGive(xMutex);
		}

		xReturn = pdPASS;
	}
	else {
		xReturn = pdFAIL;
	}

	return xReturn;
}

extern "C"
SemaphoreHandle_t xSemaphoreCreateCounting(const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount)
{
	SemaphoreHandle_t xHandle;

	xHandle = xSemaphoreGenericCreate(uxMaxCount, uxInitialCount);
	if (xHandle != NULL) {
		xHandle->ucQueueType = QUEUE_TYPE_COUNTING_SEMAPHORE;
	}

	return xHandle;
}

SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
	SemaphoreHandle_t xHandle;

	xHandle = xSemaphoreGenericCreate(1, 0);
	if (xHandle != NULL) {
		xHandle->ucQueueType = QUEUE_TYPE_BINARY_SEMAPHORE;
	}

	return xHandle;
}

extern "C"
BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime)
{
	ER ercd;
	TMO tmo;

	if (xBlockTime == portMAX_DELAY) {
		ercd = wai_sem(xSemaphore->semid);
	}
	else {
		tmo = (TMO)xBlockTime;
		ercd = twai_sem(xSemaphore->semid, tmo);
	}

	switch (ercd) {
	case E_OK:
		if (xSemaphore->ucQueueType == QUEUE_TYPE_MUTEX) {
			ID tid;
			ercd = get_tid(&tid);
			if (ercd != E_OK) {
				syslog(LOG_ERROR, "get_tid error %d", ercd);
				tid = 0;
			}
			xSemaphore->xMutexHolder = tid;
		}
		return pdPASS;
	case E_TMOUT:
		return errQUEUE_EMPTY;
	default:
		syslog(LOG_ERROR, "wai_sem(%d) error %d", xSemaphore->semid, ercd);
		break;
	}

	return pdFAIL;
}

extern "C"
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
	ER ercd;

	if (xSemaphore->ucQueueType == QUEUE_TYPE_MUTEX) {
		xSemaphore->xMutexHolder = NULL;
	}

	ercd = sig_sem(xSemaphore->semid);

	switch (ercd) {
	case E_OK:
		return pdPASS;
	case E_TMOUT:
		return errQUEUE_EMPTY;
	default:
		syslog(LOG_ERROR, "sig_sem(%d) error %d", xSemaphore->semid, ercd);
		break;
	}

	return pdFAIL;
}

typedef struct EventGroupDef_t
{
	ID flgid;
} EventGroup_t;

EventGroup_t FreeRTOS_FLG[10];

extern "C"
EventGroupHandle_t xEventGroupCreate(void)
{
	T_CFLG  cflg;
	ER    ercd;

	EventGroup_t *pFLG = nullptr;
	ID flgid = FLG1;
	EventGroup_t *end = &FreeRTOS_FLG[sizeof(FreeRTOS_FLG) / sizeof(FreeRTOS_FLG[0])];
	for (EventGroup_t *pos = FreeRTOS_FLG; pos < end; pos++, flgid++) {
		if (pos->flgid == 0) {
			pFLG = pos;
			pFLG->flgid = flgid;
			break;
		}
	}

	if (pFLG == nullptr)
		return nullptr;

	cflg.flgatr = TA_TPRI;
	cflg.iflgptn = 0;
	ercd = cre_flg(pFLG->flgid, &cflg);
	if (ercd != E_OK) {
		pFLG->flgid = 0;
		syslog(LOG_ERROR, "cre_flg error %d", ercd);
		return nullptr;
	}

	return pFLG;
}

extern "C"
EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet)
{
	T_RFLG rflg = { 0 };
	ER ercd;

	ercd = set_flg(xEventGroup->flgid, uxBitsToSet);
	if (ercd != E_OK) {
		syslog(LOG_ERROR, "set_flg(%d) error %d", xEventGroup->flgid, ercd);
	}

	ercd = ref_flg(xEventGroup->flgid, &rflg);
	if (ercd != E_OK) {
		syslog(LOG_ERROR, "ref_flg(%d) error %d", xEventGroup->flgid, ercd);
	}

	return rflg.flgptn;
}

extern "C"
EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear)
{
	T_RFLG rflg = { 0 };
	ER ercd;

	ercd = ref_flg(xEventGroup->flgid, &rflg);
	if (ercd != E_OK) {
		syslog(LOG_ERROR, "ref_flg(%d) error %d", xEventGroup->flgid, ercd);
	}

	ercd = clr_flg(xEventGroup->flgid, ~uxBitsToClear);
	if (ercd != E_OK) {
		syslog(LOG_ERROR, "clr_flg(%d) error %d", xEventGroup->flgid, ercd);
	}

	return rflg.flgptn;
}

extern "C"
EventBits_t xEventGroupGetBits(EventGroupHandle_t xEventGroup)
{
	T_RFLG rflg = { 0 };
	ER ercd;

	ercd = ref_flg(xEventGroup->flgid, &rflg);
	if (ercd != E_OK) {
		syslog(LOG_ERROR, "ref_flg(%d) error %d", xEventGroup->flgid, ercd);
	}

	return rflg.flgptn;
}

extern "C"
EventBits_t xEventGroupWaitBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait)
{
	ER ercd;
	MODE wfmode;
	TMO tmo;
	FLGPTN flgptn = 0;

	if (xWaitForAllBits) {
		wfmode = TWF_ANDW;
	}
	else {
		wfmode = TWF_ORW;
	}

	if (xTicksToWait == portMAX_DELAY) {
		ercd = wai_flg(xEventGroup->flgid, uxBitsToWaitFor, wfmode, &flgptn);
	}
	else {
		tmo = (TMO)xTicksToWait;
		ercd = twai_flg(xEventGroup->flgid, uxBitsToWaitFor, wfmode, &flgptn, tmo);
	}

	if (ercd != E_OK) {
		syslog(LOG_ERROR, "wai_flg(%d) error %d", xEventGroup->flgid, ercd);
	}

	if (xClearOnExit && ercd == E_OK) {
		ercd = clr_flg(xEventGroup->flgid, ~uxBitsToWaitFor);
		if (ercd != E_OK) {
			syslog(LOG_ERROR, "wai_flg(%d) error %d", xEventGroup->flgid, ercd);
		}
	}

	return flgptn;
}

__attribute__((weak))
void user_inirtn(void)
{
}

extern "C"
void
init_idleloop(void) {
}
