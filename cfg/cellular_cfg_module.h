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

#ifndef _CELLULAR_CFG_MODULE_H_
#define _CELLULAR_CFG_MODULE_H_

/* No #includes allowed here */

/* This header file contains configuration information for all the
 * possible cellular module types.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACRO CROSS CHECKING
 * -------------------------------------------------------------- */

/* #define cross-checking.
 */
#if defined(CELLULAR_CFG_MODULE_SARA_R4) && defined(CELLULAR_CFG_MODULE_SARA_R5)
# error More than one module type defined.
#endif

#if !defined(CELLULAR_CFG_MODULE_SARA_R4) && !defined(CELLULAR_CFG_MODULE_SARA_R5)
# error Must define a module type (e.g. CELLULAR_CFG_MODULE_SARA_R4 or CELLULAR_CFG_MODULE_SARA_R5).
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS COMMON TO ALL MODULES
 * -------------------------------------------------------------- */

#ifndef CELLULAR_CFG_BAUD_RATE
/** The baud rate to use on the UART interface
 */
# define CELLULAR_CFG_BAUD_RATE                      115200
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR SARA-R5
 * -------------------------------------------------------------- */

#ifdef CELLULAR_CFG_MODULE_SARA_R5

/** The delay between AT commands, allowing internal cellular module
 * comms to complete before another is sent.
 */
# define CELLULAR_CTRL_COMMAND_DELAY_MS 20

/** The minimum reponse time one can expect with cellular module.
 * This is quite large since, if there is a URC about to come through,
 * it can delay what are normally immediate responses.
 */
# define CELLULAR_CTRL_COMMAND_MINIMUM_RESPONSE_TIME_MS 5000

/** The time to wait before the cellular module is ready at boot.
 */
# define CELLULAR_CTRL_BOOT_WAIT_TIME_MS 3000

/** The time to wait for an organised power off.
 */
# define CELLULAR_CTRL_POWER_DOWN_WAIT_SECONDS 20

/** The maximum number of simultaneous radio access technologies
 *  supported by the cellular module.
 */
# define CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS 1

#endif // CELLULAR_CFG_MODULE_SARA_R5

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR SARA-R4
 * -------------------------------------------------------------- */

#ifdef CELLULAR_CFG_MODULE_SARA_R4

/** The delay between AT commands, allowing internal cellular module
 * comms to complete before another is sent.
 */
# define CELLULAR_CTRL_COMMAND_DELAY_MS 100

/** The minimum reponse time one can expect with cellular module.
 * This is quite large since, if there is a URC about to come through,
 * it can delay what are normally immediate responses.
 */
# define CELLULAR_CTRL_COMMAND_MINIMUM_RESPONSE_TIME_MS 2000

/** The time to wait before the cellular module is ready at boot.
 */
# define CELLULAR_CTRL_BOOT_WAIT_TIME_MS 5000

/** The time to wait for an organised power off.
 */
# define CELLULAR_CTRL_POWER_DOWN_WAIT_SECONDS 10

/** The maximum number of simultaneous radio access technologies
 *  supported by the cellular module.
 */
# define CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS 2

#endif // CELLULAR_CFG_MODULE_SARA_R4

#endif // _CELLULAR_CFG_MODULE_H_

// End of file
