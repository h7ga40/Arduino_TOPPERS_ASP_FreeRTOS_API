
#ifndef TOPPERSASP_FREERTOS_API_H
#define TOPPERSASP_FREERTOS_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOSConfig.h"
#include "projdefs.h"
#include "portmacro.h"

#define PRIVILEGED_FUNCTION

typedef struct tskTaskControlBlock* TaskHandle_t;
typedef struct QueueDefinition* QueueHandle_t;
#define configSTACK_DEPTH_TYPE uint16_t

void *pvPortMalloc( size_t xSize ) PRIVILEGED_FUNCTION;
void vPortFree( void *pv ) PRIVILEGED_FUNCTION;

void vTaskStartScheduler(void) PRIVILEGED_FUNCTION;

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,
                       const char* const pcName,
                       const configSTACK_DEPTH_TYPE usStackDepth,
                       void* const pvParameters,
                       UBaseType_t uxPriority,
                       TaskHandle_t* const pxCreatedTask) PRIVILEGED_FUNCTION;

void vTaskDelete(TaskHandle_t xTaskToDelete) PRIVILEGED_FUNCTION;
void vTaskDelay(const TickType_t xTicksToDelay) PRIVILEGED_FUNCTION;
TaskHandle_t xTaskGetCurrentTaskHandle(void) PRIVILEGED_FUNCTION;
void vTaskSuspend(TaskHandle_t xTaskToSuspend) PRIVILEGED_FUNCTION;
TickType_t xTaskGetTickCount(void) PRIVILEGED_FUNCTION;

#define taskENTER_CRITICAL()		vPortEnterCritical()
#define taskEXIT_CRITICAL()			vPortExitCritical()
#define taskYIELD()					portYIELD()

struct QueueDefinition;
typedef struct QueueDefinition* QueueHandle_t;
typedef QueueHandle_t SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateMutex(void) PRIVILEGED_FUNCTION;
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t xMutex, TickType_t xTicksToWait) PRIVILEGED_FUNCTION;
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t xMutex) PRIVILEGED_FUNCTION;

SemaphoreHandle_t xSemaphoreCreateCounting(const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount) PRIVILEGED_FUNCTION;
void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) PRIVILEGED_FUNCTION;
BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) PRIVILEGED_FUNCTION;
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) PRIVILEGED_FUNCTION;
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore,  BaseType_t * const pxHigherPriorityTaskWoken ) PRIVILEGED_FUNCTION;

UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue ) PRIVILEGED_FUNCTION;

SemaphoreHandle_t xSemaphoreCreateBinary( void ) PRIVILEGED_FUNCTION;

struct EventGroupDef_t;
typedef struct EventGroupDef_t* EventGroupHandle_t;

typedef unsigned int uint_t;
typedef uint_t FLGPTN;
typedef FLGPTN EventBits_t;

EventGroupHandle_t xEventGroupCreate(void) PRIVILEGED_FUNCTION;
EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet) PRIVILEGED_FUNCTION;
EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear) PRIVILEGED_FUNCTION;
EventBits_t xEventGroupGetBits(EventGroupHandle_t xEventGroup) PRIVILEGED_FUNCTION;
EventBits_t xEventGroupWaitBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor,
const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait) PRIVILEGED_FUNCTION;

#ifdef __cplusplus
};
#endif

#endif /* TOPPERSASP_FREERTOS_API_H */