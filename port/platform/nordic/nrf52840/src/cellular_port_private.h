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

#ifndef _CELLULAR_PORT_PRIVATE_H_
#define _CELLULAR_PORT_PRIVATE_H_

/** Stuff private to the NRF52840 porting layer.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// The frequency to run the timer at: nice 'n slow.
// IMPORTANT: if you change this value then you also
// need to change the calculation in cellularPortGetTickTimeMs()
// and you need to consider the effect it has on the Rx timeout
// of the UART since it is also used there.  Best not to change it.
#define CELLULAR_PORT_TICK_TIMER_FREQUENCY_HZ NRF_TIMER_FREQ_31250Hz;

// The bit-width of the timer.
#define CELLULAR_PORT_TICK_TIMER_BIT_WIDTH NRF_TIMER_BIT_WIDTH_24;

// The limit of the timer in normal mode.  With a frequency
// of 31250 Hz this results in an overflow every 9 minutes.
// IMPORTANT: if you change this value then you also
// need to change the calculation in cellularPortGetTickTimeMs().
#define CELLULAR_PORT_TICK_TIMER_LIMIT_NORMAL_MODE 0xFFFFFF

// The number of bits represented by
// CELLULAR_PORT_TICK_TIMER_LIMIT_NORMAL_MODE.
#define CELLULAR_PORT_TICK_TIMER_LIMIT_NORMAL_MODE_BITS 24

// The limit of the timer in UART mode.  With a frequency
// of 31250 Hz this results in an overflow every 130 milliseconds.  The
// overflow count is a 64 bit variable so that's still rather a large
// number of years.
// IMPORTANT: if you change this value then you also
// need to change the calculation in cellularPortGetTickTimeMs()
// and you need to consider the effect it has on the Rx timeout
// of the UART since it is also used there.  Best not to change it.
#define CELLULAR_PORT_TICK_TIMER_LIMIT_UART_MODE 0xFFF

// The number of bits represented by
// CELLULAR_PORT_TICK_TIMER_LIMIT_UART_MODE.
#define CELLULAR_PORT_TICK_TIMER_LIMIT_UART_MODE_BITS 12

// The difference between the two limits above as a bit shift.
#define CELLULAR_PORT_TICK_TIMER_LIMIT_DIFF CELLULAR_PORT_TICK_TIMER_LIMIT_NORMAL_MODE_BITS - \
                                            CELLULAR_PORT_TICK_TIMER_LIMIT_UART_MODE_BITS

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the private stuff.
 *
 * @return zero on success else negative error code.
 */
int32_t cellularPortPrivateInit();

/** Deinitialise the private stuff.
 */
void cellularPortPrivateDeinit();

/** Get the current OS tick converted to a time in milliseconds.
 */
int64_t cellularPortPrivateGetTickTimeMs();

/** Register a callback to be called when tick timer
 * overflow interrupt occurs.
 *
 * @param pCb          the callback, use NULL to deregister a
 *                     previous callback.  This will be
 *                     called from interrupt context and
 *                     so must do virtually nothing!
 * @param pCbParameter a parameter which will be passed to
 *                     pCb when it is called, may be NULL.
 */
void cellularPortPrivateTickTimeSetInterruptCb(void (*pCb) (void *),
                                               void *pCbParameter);

/** Set the tick time into a mode where it can used
 * as a relatively rapid ticker for UART Rx timeouts.
 * This function fiddles with the timer values and so
 * should only be called when there is no possibility that
 * we might also be calling cellularPortGetTickTimeMs()
 * or cellularPortPrivateGetTickTimeMs().
 */
void cellularPortPrivateTickTimeUartMode();

/** Set the tick time back into normal slow mode.
 * This function fiddles with the timer values and so
 * should only be called when there is no possibility that
 * we might also be calling cellularPortGetTickTimeMs()
 * or cellularPortPrivateGetTickTimeMs().
 */
void cellularPortPrivateTickTimeNormalMode();

#ifdef __cplusplus
}
#endif

#endif // _CELLULAR_PORT_PRIVATE_H_

// End of file
