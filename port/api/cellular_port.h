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

#ifndef _CELLULAR_PORT_H_
#define _CELLULAR_PORT_H_

/* No #includes allowed here */

/** Common stuff for porting layer.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** On some platforms we need to run a timer to get our 64-bit
 * tick timer.  This specifies the timer instance to use for that.
 */
#ifndef CELLULAR_PORT_TICK_TIMER_INSTANCE
# define CELLULAR_PORT_TICK_TIMER_INSTANCE 0
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Error codes.
 */
typedef enum {
    CELLULAR_PORT_SUCCESS = 0,
    CELLULAR_PORT_UNKNOWN_ERROR = -1,
    CELLULAR_PORT_NOT_INITIALISED = -2,
    CELLULAR_PORT_NOT_IMPLEMENTED = -3,
    CELLULAR_PORT_INVALID_PARAMETER = -4,
    CELLULAR_PORT_OUT_OF_MEMORY = -5,
    CELLULAR_PORT_TIMEOUT = -6,
    CELLULAR_PORT_PLATFORM_ERROR = -7,
    CELLULAR_PORT_FORCE_32_BIT = 0x7FFFFFFF // Force this enum to be 32 bit
                                            // as it can be used as a size
                                            // also
} CellularPortErrorCode_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Start the platform.  This should be called before
 * anything else: it configures clocks, resources, etc. and,
 * then calls the entry point function.
 * Where there is code already in the system that does these
 * things, that code should be adapted to include the
 * necessary portions of this function.
 * This function only returns if there is an error;
 * code execution ends up in pEntryPoint, which should 
 * never return.
 *
 * @param pEntryPoint    the function to run.
 * @param pParameter     a pointer that will be passed to pEntryPoint
 *                       when it is called. Usually NULL.
 * @param stackSizeBytes the number of bytes of memory to dynamically
 *                       allocate for stack; ignored if an RTOS
 *                       task is already running.
 * @param priority       the priority at which to run a task that
 *                       is pEntryPoint; ignored if an RTOS
 *                       task is already running.
 * @return               negative error code.
 */
int32_t cellularPortPlatformStart(void (*pEntryPoint)(void *),
                                  void *pParameter,
                                  size_t stackSizeBytes,
                                  int32_t priority);

/** Initialise the porting layer.
 *
 * @return zero on success else negative error code.
 */
int32_t cellularPortInit();

/** Deinitialise the porting layer.
 */
void cellularPortDeinit();

/** Get the current OS tick converted to a time in milliseconds.
 * This is guaranteed to be unaffected by any time setting activity
 * It is NOT maintained while the processor is sleeping; port
 * initialisation must be called on return from sleep and
 * that will restart this time from zero once more.
 *
 * @return the current OS tick converted to milliseconds.
 */
int64_t cellularPortGetTickTimeMs();

#endif // _CELLULAR_PORT_H_

// End of file
