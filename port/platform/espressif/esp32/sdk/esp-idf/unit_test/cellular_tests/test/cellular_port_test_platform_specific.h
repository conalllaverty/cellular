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

#ifndef _CELLULAR_PORT_TEST_PLATFORM_SPECIFIC_H_
#define _CELLULAR_PORT_TEST_PLATFORM_SPECIFIC_H_

/* Only bring in #includes specifically related to the test framework */

#include "unity.h"

/** Porting layer for test execution for the Espressif platform.
 * Since test execution is often macro-ised rather than
 * function-calling this header file forms part of the platform
 * test source code rather than pretending to be a generic API.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: UNITY RELATED
 * -------------------------------------------------------------- */

/** Macro to wrap the definition of a test function and
 * map it to unity.
 */
#define CELLULAR_PORT_TEST_FUNCTION(func, name, group) TEST_CASE(name, \
                                                                 group)

/** Macro to wrap a test assertion and map it to unity.
 */
#define CELLULAR_PORT_TEST_ASSERT(condition) TEST_ASSERT(condition)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: OS RELATED
 * -------------------------------------------------------------- */

/** The stack size to use for the test task created during OS testing.
 */
#define CELLULAR_PORT_TEST_OS_TASK_STACK_SIZE_BYTES 2048

/** The task priority to use for the task created during OS
 * testing: make sure that the priority of the task RUNNING
 * the tests is lower than this.  In FreeRTOS, as used on this
 * platform, low numbers indicate lower priority.
 */
#define CELLULAR_PORT_TEST_OS_TASK_PRIORITY (CELLULAR_PORT_OS_PRIORITY_MIN + 5)

/** The stack size to use for the test task created during sockets testing.
 */
#define CELLULAR_PORT_TEST_SOCK_TASK_STACK_SIZE_BYTES (1024 * 5)

/** The priority to use for the test task created during sockets testing;
 * lower priority than the URC handler.  In FreeRTOS, as used on this
 * platform, low numbers indicate lower priority.
 */
#define CELLULAR_PORT_TEST_SOCK_TASK_PRIORITY (CELLULAR_CTRL_AT_TASK_URC_PRIORITY - 1)

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: HW RELATED
 * -------------------------------------------------------------- */

/** Pin A for GPIO testing: will be used as an output and
 * must be connected to pin B via a 1k resistor.
 */
#ifndef CELLULAR_PORT_TEST_PIN_A
# define CELLULAR_PORT_TEST_PIN_A         33
#endif

/** Pin B for GPIO testing: will be used as both an input and
 * and open drain output and must be connected both to pin A via
 * a 1k resistor and directly to pin C.
 */
#ifndef CELLULAR_PORT_TEST_PIN_B
# define CELLULAR_PORT_TEST_PIN_B         32
#endif

/** Pin C for GPIO testing: must be connected to pin B,
 * will be used as an input only.
 */
#ifndef CELLULAR_PORT_TEST_PIN_C
# define CELLULAR_PORT_TEST_PIN_C         35
#endif

/** UART HW block for UART driver testing.
 */
#ifndef CELLULAR_PORT_TEST_UART
# define CELLULAR_PORT_TEST_UART          2
#endif

/** Handshake threshold for UART testing.
 */
#ifndef CELLULAR_PORT_TEST_UART_RTS_THRESHOLD
# define CELLULAR_PORT_TEST_UART_RTS_THRESHOLD 100
#endif

/** Tx pin for UART testing: should be connected to the Rx UART pin.
 */
#ifndef CELLULAR_PORT_TEST_PIN_UART_TXD
# define CELLULAR_PORT_TEST_PIN_UART_TXD   13
#endif

/** Rx pin for UART testing: should be connected to the Tx UART pin.
 */
#ifndef CELLULAR_PORT_TEST_PIN_UART_RXD
# define CELLULAR_PORT_TEST_PIN_UART_RXD   14
#endif

/** CTS pin for UART testing: should be connected to the RTS UART pin.
 */
#ifndef CELLULAR_PORT_TEST_PIN_UART_CTS
# define CELLULAR_PORT_TEST_PIN_UART_CTS   26
#endif

/** RTS pin for UART testing: should be connected to the CTS UART pin.
 */
#ifndef CELLULAR_PORT_TEST_PIN_UART_RTS
# define CELLULAR_PORT_TEST_PIN_UART_RTS   27
#endif

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

#endif // _CELLULAR_PORT_TEST_PLATFORM_SPECIFIC_H_

// End of file
