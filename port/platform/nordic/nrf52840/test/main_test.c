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
#include "cellular_port_test_platform_specific.h"
#include "assert.h"
#include "FreeRTOS.h"
#include "task.h"
#include "nrf_drv_clock.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// How much stack the task running all the tests needs in bytes.
#define CELLULAR_PORT_TEST_RUNNER_TASK_STACK_SIZE_BYTES (1024 * 4)

// The priority of the task running all the tests.
#define CELLULAR_PORT_TEST_RUNNER_TASK_PRIORITY 14

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
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Unity setUp() function.
void setUp(void)
{
    // Nothing to do
}

// Unity tearDown() function.
void tearDown(void)
{
    // Nothing to do
}

void testFail(void)
{
    
}

// The task within which testing runs.
void testTask(void *pParam)
{
    (void) pParam;

    NRF_LOG_RAW_INFO("\n\nCELLULAR_TEST: Test task started.\n");

    UNITY_BEGIN();

    NRF_LOG_RAW_INFO("CELLULAR_TEST: Tests available:\n\n");
    cellularPortUnityPrintAll("CELLULAR_TEST: ");
    NRF_LOG_RAW_INFO("CELLULAR_TEST: Running all tests.\n");
    cellularPortUnityRunAll("CELLULAR_TEST: ");

    UNITY_END();

    NRF_LOG_RAW_INFO("\n\nCELLULAR_TEST: Test task ended.\n");
    NRF_LOG_FLUSH();

    while(1){}
}

// Entry point
int main(void)
{
    TaskHandle_t taskHandle = NULL;

    NRF_LOG_INIT(NULL);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

#if configTICK_SOURCE == FREERTOS_USE_RTC
    // If the clock has not already been started, start it
    nrf_drv_clock_init();
#endif

   // Create the test task and have it running
   // at a low priority
    assert(xTaskCreate(testTask, "TestTask",
                       CELLULAR_PORT_TEST_RUNNER_TASK_STACK_SIZE_BYTES / 4,
                       NULL,
                       CELLULAR_PORT_TEST_RUNNER_TASK_PRIORITY,
                       &taskHandle) == pdPASS);

    // Activate deep sleep mode.
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    // Start the scheduler.
    vTaskStartScheduler();

    // Should never get here
    assert(false);

    return 0;
}