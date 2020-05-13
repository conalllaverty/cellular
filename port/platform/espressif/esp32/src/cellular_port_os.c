/*
 * Copyright 2020 u-blox Cambourne Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef CELLULAR_CFG_OVERRIDE
# include "cellular_cfg_override.h" // For a customer's configuration override
#endif
#include "cellular_port_clib.h"
#include "cellular_port.h"
#include "cellular_port_os.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TASKS
 * -------------------------------------------------------------- */

// Create a task.
int32_t cellularPortTaskCreate(void (*pFunction)(void *),
                               const char *pName,
                               size_t stackSizeBytes,
                               void *pParameter,
                               int32_t priority,
                               CellularPortTaskHandle_t *pTaskHandle)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    if ((pFunction != NULL) && (pTaskHandle != NULL)) {
        if (xTaskCreate(pFunction, pName, stackSizeBytes,
                        pParameter, priority,
                        (TaskHandle_t *) pTaskHandle) == pdPASS) {
            errorCode = CELLULAR_PORT_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Delete the given task.
int32_t cellularPortTaskDelete(const CellularPortTaskHandle_t taskHandle)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    // Can only delete oneself in freeRTOS
    if (taskHandle == NULL) {
        vTaskDelete((TaskHandle_t) taskHandle);
        errorCode = CELLULAR_PORT_SUCCESS;
    }

    return (int32_t) errorCode;
}

// Check if the current task handle is equal to the given task handle.
bool cellularPortTaskIsThis(const CellularPortTaskHandle_t taskHandle)
{
    return xTaskGetCurrentTaskHandle() == (TaskHandle_t) taskHandle;
}

// Block the current task for a time.
void cellularPortTaskBlock(int32_t delayMs)
{
    vTaskDelay(delayMs / portTICK_PERIOD_MS);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: QUEUES
 * -------------------------------------------------------------- */

// Create a queue.
int32_t cellularPortQueueCreate(size_t queueLength,
                                size_t itemSizeBytes,
                                CellularPortQueueHandle_t *pQueueHandle)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    if (pQueueHandle != NULL) {
        errorCode = CELLULAR_PORT_PLATFORM_ERROR;
        // Actually create the queue
        *pQueueHandle = (CellularPortQueueHandle_t) xQueueCreate(queueLength,
                                                                 itemSizeBytes);
        if (*pQueueHandle != NULL) {
            errorCode = CELLULAR_PORT_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Delete the given queue.
int32_t cellularPortQueueDelete(const CellularPortQueueHandle_t queueHandle)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    if (queueHandle != NULL) {
        vQueueDelete((QueueHandle_t) queueHandle);
        errorCode = CELLULAR_PORT_SUCCESS;
    }

    return (int32_t) errorCode;
}

// Send to the given queue.
int32_t cellularPortQueueSend(const CellularPortQueueHandle_t queueHandle,
                              const void *pEventData)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = CELLULAR_PORT_PLATFORM_ERROR;
        if (xQueueSend((QueueHandle_t) queueHandle,
                       pEventData,
                       (portTickType) portMAX_DELAY) == pdTRUE) {
            errorCode = CELLULAR_PORT_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Receive from the given queue, blocking.
int32_t cellularPortQueueReceive(const CellularPortQueueHandle_t queueHandle,
                                 void *pEventData)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = CELLULAR_PORT_PLATFORM_ERROR;
        if (xQueueReceive((QueueHandle_t) queueHandle,
                          pEventData,
                          (portTickType) portMAX_DELAY) == pdTRUE) {
            errorCode = CELLULAR_PORT_SUCCESS;
       }
    }

    return (int32_t) errorCode;
}

// Receive from the given queue, with a wait time.
int32_t cellularPortQueueTryReceive(const CellularPortQueueHandle_t queueHandle,
                                    int32_t waitMs, void *pEventData)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    if ((queueHandle != NULL) && (pEventData != NULL)) {
        errorCode = CELLULAR_PORT_PLATFORM_ERROR;
        if (xQueueReceive((QueueHandle_t) queueHandle,
                          pEventData,
                          waitMs / portTICK_PERIOD_MS) == pdTRUE) {
            errorCode = CELLULAR_PORT_SUCCESS;
       }
    }

    return (int32_t) errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MUTEXES
 * -------------------------------------------------------------- */

// Create a mutex.
int32_t cellularPortMutexCreate(CellularPortMutexHandle_t *pMutexHandle)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    if (pMutexHandle != NULL) {
        errorCode = CELLULAR_PORT_PLATFORM_ERROR;
        // Actually create the mutex
        *pMutexHandle = (CellularPortMutexHandle_t) xSemaphoreCreateMutex();
        if (*pMutexHandle != NULL) {
            errorCode = CELLULAR_PORT_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Destroy a mutex.
int32_t cellularPortMutexDelete(const CellularPortMutexHandle_t mutexHandle)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    if (mutexHandle != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t) mutexHandle);
        errorCode = CELLULAR_PORT_SUCCESS;
    }

    return (int32_t) errorCode;
}

// Lock the given mutex.
int32_t cellularPortMutexLock(const CellularPortMutexHandle_t mutexHandle)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    if (mutexHandle != NULL) {
        errorCode = CELLULAR_PORT_PLATFORM_ERROR;
        if (xSemaphoreTake((SemaphoreHandle_t) mutexHandle,
                           (portTickType) portMAX_DELAY) == pdTRUE) {
            errorCode = CELLULAR_PORT_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Try to lock the given mutex.
int32_t cellularPortMutexTryLock(const CellularPortMutexHandle_t mutexHandle,
                                 int32_t delayMs)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    if (mutexHandle != NULL) {
        errorCode = CELLULAR_PORT_TIMEOUT;
        if (xSemaphoreTake((SemaphoreHandle_t) mutexHandle,
                           delayMs / portTICK_PERIOD_MS) == pdTRUE) {
            errorCode = CELLULAR_PORT_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Unlock the given mutex.
int32_t cellularPortMutexUnlock(const CellularPortMutexHandle_t mutexHandle)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    if (mutexHandle != NULL) {
        xSemaphoreGive((SemaphoreHandle_t) mutexHandle);
        errorCode = CELLULAR_PORT_SUCCESS;
    }

    return (int32_t) errorCode;
}

// End of file
