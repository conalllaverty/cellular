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

#include "esp_timer.h" // For esp_timer_get_time()

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
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Start the platform.
int32_t cellularPortPlatformStart(void (*pEntryPoint)(void *),
                                  void *pParameter,
                                  size_t stackSizeBytes,
                                  int32_t priority)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;

    (void) stackSizeBytes;
    (void) priority;

    // RTOS is already running, just call pEntryPoint
    if (pEntryPoint != NULL) {
        errorCode = CELLULAR_PORT_PLATFORM_ERROR;
        pEntryPoint(pParameter);
    }

    return errorCode;
}

// Initialise the porting layer.
int32_t cellularPortInit()
{
    // Nothing to do
    return CELLULAR_PORT_SUCCESS;
}

// Deinitialise the porting layer.
void cellularPortDeinit()
{
    // Nothing to do
}

// Get the current tick converted to a time in milliseconds.
int64_t cellularPortGetTickTimeMs()
{
    return esp_timer_get_time() / 1000;
}

// End of file
