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

/* Only #includes of cellular_* are allowed here, no C lib,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/C library/OS must be brought in through
 * cellular_port* to maintain portability.
 */

#ifdef CELLULAR_CFG_OVERRIDE
# include "cellular_cfg_override.h" // For a customer's configuration override
#endif
#include "cellular_cfg_hw_platform_specific.h"
#include "cellular_cfg_sw.h"
#include "cellular_cfg_module.h"
#include "cellular_port_clib.h"
#include "cellular_port.h"
#include "cellular_port_debug.h"
#include "cellular_port_os.h"
#include "cellular_port_uart.h"
#include "cellular_port_test_platform_specific.h"
#include "cellular_ctrl.h"
#include "cellular_cfg_test.h"

/** Note: some of these tests use cellularPort_rand() but they
 * deliberately don't attempt any seeding of the random number
 * generator, (a) because, if an error occurs, I will want to repeat
 * it and (b) because no seed is available anyway.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// The amount of time to allow for cellular power off in milliseconds.
#define CELLULAR_CTRL_TEST_POWER_OFF_TIME_MS 10000

// The number of consecutive AT timeouts that might
// normally be expected from the module.
#define CELLULAR_CTRL_AT_CONSECUTIVE_TIMEOUTS_LIMIT 0

// The time in seconds allowed for a connection to complete.
#define CELLULAR_CFG_TEST_CONNECT_TIMEOUT_SECONDS 240

// Figure out if NB1 is supported
#ifndef CELLULAR_CTRL_TEST_NB1_IS_SUPPORTED
# ifdef CELLULAR_CFG_MODULE_SARA_R4
#  define CELLULAR_CTRL_TEST_NB1_IS_SUPPORTED 1
# else
#  define CELLULAR_CTRL_TEST_NB1_IS_SUPPORTED 0
# endif
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Used for keepGoingCallback() timeout.
static int64_t gStopTimeMS;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the cellular connect process
static bool keepGoingCallback()
{
    bool keepGoing = true;

    if (cellularPortGetTickTimeMs() > gStopTimeMS) {
        keepGoing = false;
    }

    return keepGoing;
}

// Test power on/off and aliveness, parameterised with the VInt pin.
// Note: no checking of cellularCtrlGetConsecutiveAtTimeouts() here as
// we're deliberately doing things that should cause timeouts.
static void cellularCtrlTestPowerAliveVInt(int32_t pinVint)
{
    CellularPortQueueHandle_t queueHandle;
    bool (*pKeepGoingCallback) (void) = NULL;
    bool trulyHardPowerOff = false;

#if CELLULAR_CFG_PIN_ENABLE_POWER >= 0
    trulyHardPowerOff = true;
#endif

    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);

    if (pinVint >= 0) {
        cellularPortLog("CELLULAR_CTRL_TEST: running power-on and alive tests with VInt on pin %d.\n",
                        pinVint);
    } else {
        cellularPortLog("CELLULAR_CTRL_TEST: running power-on and alive tests without VInt.\n");
    }

    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &queueHandle) == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: testing power-on and alive calls before initialisation...\n");
#if (CELLULAR_CFG_PIN_ENABLE_POWER) == -1
    // Should always return true if there isn't a power enable pin
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlIsPowered());
#endif
    // Should return false before initialisation
    CELLULAR_PORT_TEST_ASSERT(!cellularCtrlIsAlive());
    // Should fail before initialisation
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) < 0);
    // Should still return false
    CELLULAR_PORT_TEST_ASSERT(!cellularCtrlIsAlive());

    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               pinVint,
                                               false,
                                               CELLULAR_CFG_UART,
                                               queueHandle) == 0);

    // Do this twice so as to check transiting from
    // a call to cellularCtrlPowerOff() to a call to
    // cellularCtrlPowerOn().
    for (size_t x = 0; x < 2; x++) {
        cellularPortLog("CELLULAR_CTRL_TEST: testing power-on and alive calls");
        if (x > 0) {
           cellularPortLog(" with a callback passed to cellularCtrlPowerOff() and a %d second power-off timer, iteration %d.\n",
                           CELLULAR_CTRL_TEST_POWER_OFF_TIME_MS / 1000, x + 1);
        } else {
           cellularPortLog(" with cellularCtrlPowerOff(NULL), iteration %d.\n",
                           x + 1);
        }
        CELLULAR_PORT_TEST_ASSERT(!cellularCtrlIsAlive());
#if (CELLULAR_CFG_PIN_ENABLE_POWER) != -1
        CELLULAR_PORT_TEST_ASSERT(!cellularCtrlIsPowered());
#endif
        // TODO Note: only use a NULL pin as we don't support anything
        // else at least that's the case on SARA-R4 when you want to
        // have power saving
        cellularPortLog("CELLULAR_CTRL_TEST: powering on...\n");
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);
        cellularPortLog("CELLULAR_CTRL_TEST: checking that modem is alive...\n");
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlIsAlive());
        // Test with and without a keep going callback
        if (x > 0) {
            // Note: can't check if keepGoingCallback is being
            // called here as we've no control over how long the
            // module takes to power off.
            pKeepGoingCallback = keepGoingCallback;
            gStopTimeMS = cellularPortGetTickTimeMs() + CELLULAR_CTRL_TEST_POWER_OFF_TIME_MS;
        }
        cellularPortLog("CELLULAR_CTRL_TEST: powering off...\n");
        cellularCtrlPowerOff(pKeepGoingCallback);
        cellularPortLog("CELLULAR_CTRL_TEST: power off completed.\n");
    }

    // Do this twice so as to check transiting from
    // a call to cellularCtrlHardOff() to a call to
    // cellularCtrlPowerOn().
    for (size_t x = 0; x < 2; x++) {
        cellularPortLog("CELLULAR_CTRL_TEST: testing power-on and alive calls with cellularCtrlHardPowerOff()");
        if (trulyHardPowerOff) {
            cellularPortLog(" and truly hard power off");
        }
        cellularPortLog(", iteration %d.\n", x + 1);
        CELLULAR_PORT_TEST_ASSERT(!cellularCtrlIsAlive());
#if (CELLULAR_CFG_PIN_ENABLE_POWER) != -1
        CELLULAR_PORT_TEST_ASSERT(!cellularCtrlIsPowered());
#endif
        cellularPortLog("CELLULAR_CTRL_TEST: powering on...\n");
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);
        cellularPortLog("CELLULAR_CTRL_TEST: checking that modem is alive...\n");
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlIsAlive());
#ifdef CELLULAR_CFG_MODULE_SARA_R5
        if (!trulyHardPowerOff) {
            cellularPortLog("CELLULAR_CTRL_TEST: waiting 10 seconds 'cos SARA-R5 needs that...\n");
            // The current version of SARA-R5 will not power
            // off based on PWR_ON for 10 second after powering on
            // TODO: remove this when KM_SW-3250 is resolved
            cellularPortTaskBlock(10000);
        }
#endif
        cellularPortLog("CELLULAR_CTRL_TEST: hard powering off...\n");
        cellularCtrlHardPowerOff(trulyHardPowerOff, NULL);
        cellularPortLog("CELLULAR_CTRL_TEST: hard power off completed.\n");
    }

    cellularPortLog("CELLULAR_CTRL_TEST: testing power-on and alive calls after hard power off.\n");
    CELLULAR_PORT_TEST_ASSERT(!cellularCtrlIsAlive());
#if (CELLULAR_CFG_PIN_ENABLE_POWER) != -1
    CELLULAR_PORT_TEST_ASSERT(!cellularCtrlIsPowered());
#endif

    cellularCtrlDeinit();

    cellularPortLog("CELLULAR_CTRL_TEST: testing power-on and alive calls after deinitialisation.\n");
#if (CELLULAR_CFG_PIN_ENABLE_POWER) == -1
    // Should always return true if there isn't a power enable pin
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlIsPowered());
#endif
    // Should fail after deinitialisation
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) < 0);
    // Should return false after deinitialisation
    CELLULAR_PORT_TEST_ASSERT(!cellularCtrlIsAlive());

    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);

    cellularPortDeinit();
}

// Do a connect/disconnect test on the specified RAT.
static void connectDisconnect(CellularCtrlRat_t rat)
{
    CellularPortQueueHandle_t queueHandle;
    CellularCtrlRat_t originalRats[CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS];
    uint64_t originalMask1;
    uint64_t originalMask2;
    char buffer[64];
    int32_t mcc;
    int32_t mnc;
    int32_t bytesRead;
    bool screwy = false;
    int32_t y;
    const char *pApn = CELLULAR_CFG_TEST_APN;
    const char *pUsername = CELLULAR_CFG_TEST_USERNAME;
    const char *pPassword = CELLULAR_CFG_TEST_PASSWORD;

    for (size_t x = 0; x < sizeof (originalRats) / sizeof (originalRats[0]); x++) {
        originalRats[x] = CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED;
    }

    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               CELLULAR_CFG_PIN_VINT,
                                               false,
                                               CELLULAR_CFG_UART,
                                               queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);

    // Purely for diagnostics
    cellularCtrlGetMnoProfile();

    cellularPortLog("CELLULAR_CTRL_TEST: preparing for test...\n");
    // First, read out the existing RATs so that we can put them back
    for (size_t x = 0; x < sizeof (originalRats) / sizeof (originalRats[0]); x++) {
        originalRats[x] = cellularCtrlGetRat(x);
    }
    // Then read out the existing band masks
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetBandMask(rat, &originalMask1, &originalMask2) == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: setting sole RAT to %d...\n", rat);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetRat(rat) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetBandMask(rat, CELLULAR_CFG_TEST_BANDMASK1,
                                                           CELLULAR_CFG_TEST_BANDMASK2) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlReboot() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetNetworkStatus() != CELLULAR_CTRL_NETWORK_STATUS_REGISTERED);
    for (size_t x = 0; x < CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS; x++) {
        if (x == 0) {
            CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRat(x) == rat);
        } else {
            CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRat(x) == CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED);
        }
    }

    cellularPortLog("CELLULAR_CTRL_TEST: set a very short connect time-out to achieve a fail...\n");
    gStopTimeMS = cellularPortGetTickTimeMs() + 0;

    CELLULAR_PORT_TEST_ASSERT(cellularCtrlConnect(keepGoingCallback, NULL, NULL, NULL) != 0);
    // It is possible that, underneath us, the module has autonomously connected
    // so make sure it is disconnected here
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlDisconnect() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetNetworkStatus() != CELLULAR_CTRL_NETWORK_STATUS_REGISTERED);

    cellularPortLog("CELLULAR_CTRL_TEST: waiting %d second(s) to connect with all NULL parameters...\n",
                    CELLULAR_CFG_TEST_CONNECT_TIMEOUT_SECONDS);
    gStopTimeMS = cellularPortGetTickTimeMs()  + (CELLULAR_CFG_TEST_CONNECT_TIMEOUT_SECONDS * 1000);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlConnect(keepGoingCallback, NULL, NULL, NULL) == 0);
    cellularPortLog("CELLULAR_CTRL_TEST: RAT %d, cellularCtrlGetNetworkStatus() %d.\n",
                    rat, cellularCtrlGetNetworkStatus());
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetNetworkStatus() == CELLULAR_CTRL_NETWORK_STATUS_REGISTERED);

    cellularPortLog("CELLULAR_CTRL_TEST: reading the operator name...\n");
    // First use an unrealistically short buffer and check
    // that there is no overrun
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    bytesRead = cellularCtrlGetOperatorStr(buffer, 1);
    CELLULAR_PORT_TEST_ASSERT(bytesRead == 0);
    for (size_t x = bytesRead; x < sizeof(buffer); x++) {
        CELLULAR_PORT_TEST_ASSERT(buffer[x] == 0);
    }
    // Now read it properly
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    bytesRead = cellularCtrlGetOperatorStr(buffer, sizeof(buffer));
    CELLULAR_PORT_TEST_ASSERT((bytesRead > 0) && (bytesRead < sizeof(buffer) - 1) &&
                              (bytesRead == cellularPort_strlen(buffer)));
    cellularPortLog(" CELLULAR_CTRL_TEST: operator name is \"%s\"...\n", buffer);

    // Read the MCC/MNC
    cellularPortLog("CELLULAR_CTRL_TEST: reading the mcc/mnc...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetMccMnc(&mcc, &mnc) == 0);
    cellularPortLog("CELLULAR_CTRL_TEST: mcc: %d, mnc %d.\n", mcc, mnc);

    cellularPortLog("CELLULAR_CTRL_TEST: reading the APN...\n");
    // First use an unrealistically short buffer and check
    // that there is no overrun
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    bytesRead = cellularCtrlGetApnStr(buffer, 1);
    CELLULAR_PORT_TEST_ASSERT(bytesRead == 0);
    for (size_t x = bytesRead; x < sizeof(buffer); x++) {
        CELLULAR_PORT_TEST_ASSERT(buffer[x] == 0);
    }
    // Now read it properly
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    bytesRead = cellularCtrlGetApnStr(buffer, sizeof(buffer));
    CELLULAR_PORT_TEST_ASSERT((bytesRead > 0) && (bytesRead < sizeof(buffer) - 1) &&
                              (bytesRead == cellularPort_strlen(buffer)));
    cellularPortLog("CELLULAR_CTRL_TEST: APN is \"%s\"...\n", buffer);

    // Read the IP address
    cellularPortLog("CELLULAR_CTRL_TEST: check if there is an IP address...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetIpAddressStr(NULL) >= 0);
    cellularPortLog("CELLULAR_CTRL_TEST: reading the IP address...\n");
    bytesRead = cellularCtrlGetIpAddressStr(buffer);
    CELLULAR_PORT_TEST_ASSERT((bytesRead > 0) && (bytesRead < sizeof(buffer) - 1) &&
                              (bytesRead == cellularPort_strlen(buffer)));
    cellularPortLog("CELLULAR_CTRL_TEST: IP address \"%s\".\n", buffer);

    // Read the time
    cellularPortLog("CELLULAR_CTRL_TEST: reading network time...\n");
    y = cellularCtrlGetTimeUtc();
    CELLULAR_PORT_TEST_ASSERT(y >= 0);
    cellularPortLog("CELLULAR_CTRL_TEST: time is %d.\n", y);

    cellularPortLog("CELLULAR_CTRL_TEST: disconnecting...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlDisconnect() == 0);

    if (pApn != NULL) {
        cellularPortLog("CELLULAR_CTRL_TEST: waiting %d second(s) to connect to APN \"%s\"...\n",
                        CELLULAR_CFG_TEST_CONNECT_TIMEOUT_SECONDS, pApn);
        gStopTimeMS = cellularPortGetTickTimeMs()  + (CELLULAR_CFG_TEST_CONNECT_TIMEOUT_SECONDS * 1000);
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlConnect(keepGoingCallback, pApn, NULL, NULL) == 0);
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetNetworkStatus() == CELLULAR_CTRL_NETWORK_STATUS_REGISTERED);

        cellularPortLog("CELLULAR_CTRL_TEST: disconnecting...\n");
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlDisconnect() == 0);
    } else {
        cellularPortLog("CELLULAR_CTRL_TEST: not testing with APN as none is specified.\n");
    }

    if ((pUsername != NULL) && (pPassword != NULL)) {
        cellularPortLog("CELLULAR_CTRL_TEST: waiting %d second(s) to connect to given username and password...\n",
                        CELLULAR_CFG_TEST_CONNECT_TIMEOUT_SECONDS);
        gStopTimeMS = cellularPortGetTickTimeMs()  + (CELLULAR_CFG_TEST_CONNECT_TIMEOUT_SECONDS * 1000);
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlConnect(keepGoingCallback, pApn,
                                                      pUsername, pPassword) == 0);
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetNetworkStatus() == CELLULAR_CTRL_NETWORK_STATUS_REGISTERED);

        cellularPortLog("CELLULAR_CTRL_TEST: disconnecting...\n");
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlDisconnect() == 0);
    } else {
        cellularPortLog("CELLULAR_CTRL_TEST: not testing with username/password as none are specified.\n");
    }

    cellularPortLog("CELLULAR_CTRL_TEST: completed, tidying up...\n");
    // No asserts here, we need it to plough on and succeed
    if (cellularCtrlSetBandMask(rat, originalMask1, originalMask2) != 0) {
        cellularPortLog("CELLULAR_CTRL_TEST: !!! ATTENTION: the band mask for RAT %d on the module under test may have been left screwy, please check!!!\n", rat);
    }
    for (size_t x = 0; x < sizeof (originalRats) / sizeof (originalRats[0]); x++) {
        cellularCtrlSetRatRank(originalRats[x], x);
    }
    cellularCtrlReboot();
    for (size_t x = 0; x < sizeof (originalRats) / sizeof (originalRats[0]); x++) {
        if (cellularCtrlGetRat(x) != originalRats[x]) {
            screwy = true;
        }
    }
    if (screwy) {
        cellularPortLog("CELLULAR_CTRL_TEST: !!! ATTENTION: the RAT settings of the module under test may have been left screwy, please check!!!\n");
    }

    cellularCtrlPowerOff(NULL);

    // Check the number of consecutive AT timeouts
    y = cellularCtrlGetConsecutiveAtTimeouts();
    cellularPortLog("CELLULAR_CTRL_TEST: there have been %d consecutive AT timeouts.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y <= CELLULAR_CTRL_AT_CONSECUTIVE_TIMEOUTS_LIMIT);

    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Basic test: initialise and then deinitialise everything.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularCtrlTestInitialisation(),
                            "ctrl_initialisation",
                            "ctrl")
{
    CellularPortQueueHandle_t queueHandle;

    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               CELLULAR_CFG_PIN_VINT,
                                               false,
                                               CELLULAR_CFG_UART,
                                               queueHandle) == 0);

    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

/** Get bandmasks.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularCtrlTestGetBandMask(),
                            "getBandMask",
                            "ctrl")
{
    CellularPortQueueHandle_t queueHandle;
    uint64_t mask1;
    uint64_t mask2;
    int32_t y;

    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               CELLULAR_CFG_PIN_VINT,
                                               false,
                                               CELLULAR_CFG_UART,
                                               queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: getting band mask...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetBandMask(CELLULAR_CTRL_RAT_CATM1,
                                                      &mask1, &mask2) == 0);
#if CELLULAR_CTRL_TEST_NB1_IS_SUPPORTED
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetBandMask(CELLULAR_CTRL_RAT_NB1,
                                                      &mask1, &mask2) == 0);
#endif

    cellularCtrlPowerOff(NULL);

    // Check the number of consecutive AT timeouts
    y = cellularCtrlGetConsecutiveAtTimeouts();
    cellularPortLog("CELLULAR_CTRL_TEST: there have been %d consecutive AT timeouts.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y <= CELLULAR_CTRL_AT_CONSECUTIVE_TIMEOUTS_LIMIT);

    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

/** Set bandmasks.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularCtrlTestSetBandMask(),
                            "setBandMask",
                            "ctrl")
{
    CellularPortQueueHandle_t queueHandle;
    int32_t y;
    uint64_t originalMask1CatM1;
    uint64_t originalMask2CatM1;
#if CELLULAR_CTRL_TEST_NB1_IS_SUPPORTED
    uint64_t originalMask1Nb1;
    uint64_t originalMask2Nb1;
#endif
    uint64_t newMask1CatM1;
    uint64_t newMask2CatM1;
#if CELLULAR_CTRL_TEST_NB1_IS_SUPPORTED
    uint64_t newMask1Nb1;
    uint64_t newMask2Nb1;
#endif

    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               CELLULAR_CFG_PIN_VINT,
                                               false,
                                               CELLULAR_CFG_UART,
                                               queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: reading original band mask...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetBandMask(CELLULAR_CTRL_RAT_CATM1,
                                                      &originalMask1CatM1,
                                                      &originalMask2CatM1) == 0);
#if CELLULAR_CTRL_TEST_NB1_IS_SUPPORTED
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetBandMask(CELLULAR_CTRL_RAT_NB1,
                                                      &originalMask1Nb1,
                                                      &originalMask2Nb1) == 0);
#endif
    cellularPortLog("CELLULAR_CTRL_TEST: setting cat-M1 band mask to 0x%016llx %016llx...\n",
                    originalMask2CatM1 & 0xaaaaaaaaaaaaaaaaULL,
                    originalMask1CatM1 & 0xaaaaaaaaaaaaaaaaULL);
    // Take the existing values and mask off every other bit
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetBandMask(CELLULAR_CTRL_RAT_CATM1,
                                                      originalMask1CatM1 &
                                                      0xaaaaaaaaaaaaaaaaULL,
                                                      originalMask2CatM1 &
                                                      0xaaaaaaaaaaaaaaaaULL) == 0);
#if CELLULAR_CTRL_TEST_NB1_IS_SUPPORTED
    cellularPortLog("CELLULAR_CTRL_TEST: setting NB1 band mask to 0x%016llx %016llx...\n",
                    originalMask2Nb1 & 0xaaaaaaaaaaaaaaaaULL,
                    originalMask1Nb1 & 0xaaaaaaaaaaaaaaaaULL);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetBandMask(CELLULAR_CTRL_RAT_NB1,
                                                      originalMask1Nb1 &
                                                      0xaaaaaaaaaaaaaaaaULL,
                                                      originalMask2Nb1 &
                                                      0xaaaaaaaaaaaaaaaaULL) == 0);
#endif
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlReboot() == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: reading new cat-M1 band mask...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetBandMask(CELLULAR_CTRL_RAT_CATM1,
                                                      &newMask1CatM1,
                                                      &newMask2CatM1) == 0);
    cellularPortLog("CELLULAR_CTRL_TEST: new cat-M1 band mask is 0x%016llx %016llx...\n",
                    newMask2CatM1, newMask1CatM1);
    CELLULAR_PORT_TEST_ASSERT(newMask1CatM1 == (originalMask1CatM1 &
                                                0xaaaaaaaaaaaaaaaaULL));
    CELLULAR_PORT_TEST_ASSERT(newMask2CatM1 == (originalMask2CatM1 &
                                                0xaaaaaaaaaaaaaaaaULL));
#if CELLULAR_CTRL_TEST_NB1_IS_SUPPORTED
    cellularPortLog("CELLULAR_CTRL_TEST: reading new NB1 band mask...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetBandMask(CELLULAR_CTRL_RAT_NB1,
                                                      &originalMask1Nb1,
                                                      &originalMask2Nb1) == 0);
    cellularPortLog("CELLULAR_CTRL_TEST: new NB1 band mask is 0x%016llx %016llx...\n",
                    newMask2Nb1, newMask1Nb1);
    CELLULAR_PORT_TEST_ASSERT(newMask1Nb1 == (originalMask1Nb1 &
                                              0xaaaaaaaaaaaaaaaaULL));
    CELLULAR_PORT_TEST_ASSERT(newMask2Nb1 == (originalMask2Nb1 &
                                              0xaaaaaaaaaaaaaaaaULL));
#endif

    // Put things back as they were if we can, or if not,
    // then a sensible default
    cellularPortLog("CELLULAR_CTRL_TEST: completed, tidying up...\n");
    // No asserts here, we need it to plough on and succeed
    if (cellularCtrlSetBandMask(CELLULAR_CTRL_RAT_CATM1,
                                originalMask1CatM1,
                                originalMask2CatM1) != 0) {
        if (cellularCtrlSetBandMask(CELLULAR_CTRL_RAT_CATM1,
                                    CELLULAR_CTRL_BAND_MASK_1_NORTH_AMERICA_CATM1_DEFAULT,
                                    CELLULAR_CTRL_BAND_MASK_2_NORTH_AMERICA_CATM1_DEFAULT) != 0) {
            cellularPortLog("CELLULAR_CTRL_TEST: !!! ATTENTION: the band mask for cat-M1 on the module under test may have been left screwy, please check!!!\n");
        }
    }
#if CELLULAR_CTRL_TEST_NB1_IS_SUPPORTED
    if (cellularCtrlSetBandMask(CELLULAR_CTRL_RAT_NB1,
                                originalMask1Nb1,
                                originalMask2Nb1) != 0) {
        if (cellularCtrlSetBandMask(CELLULAR_CTRL_RAT_NB1,
                                    CELLULAR_CTRL_BAND_MASK_1_EUROPE_NB1_DEFAULT,
                                    CELLULAR_CTRL_BAND_MASK_2_EUROPE_NB1_DEFAULT) != 0) {
            cellularPortLog("CELLULAR_CTRL_TEST: !!! ATTENTION: the band mask for NB1 on the module under test may have been left screwy, please check!!!\n");
        }
    }
#endif

    cellularCtrlPowerOff(NULL);

    // Check the number of consecutive AT timeouts
    y = cellularCtrlGetConsecutiveAtTimeouts();
    cellularPortLog("CELLULAR_CTRL_TEST: there have been %d consecutive AT timeouts.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y <= CELLULAR_CTRL_AT_CONSECUTIVE_TIMEOUTS_LIMIT);

    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

/** Test power on/off and aliveness.
 * Note: it may seem more logical to put this test early on, however
 * in that case that the previous test run failed, the
 * modem may be left on and this would cause these tests to
 * fail as a consequence (since they check that the module
 * is off at the start).  The bandmask tests, on the other hand,
 * are pretty solid so putting this test here produces fewer annoying
 * consequential failures.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularCtrlTestPowerAlive(),
                            "powerAndAliveness",
                            "ctrl")
{
    // Should work with and without a VInt pin connected
    cellularCtrlTestPowerAliveVInt(-1);
#if (CELLULAR_CFG_PIN_VINT) != -1
    cellularCtrlTestPowerAliveVInt(CELLULAR_CFG_PIN_VINT);
#endif
}

/** Test set/get RAT.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularCtrlTestSetGetRat(),
                            "setGetRat",
                            "ctrl")
{
    CellularPortQueueHandle_t queueHandle;
    int32_t y;
    CellularCtrlRat_t originalRats[CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS];
    bool screwy = false;

    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               CELLULAR_CFG_PIN_VINT,
                                               false,
                                               CELLULAR_CFG_UART,
                                               queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: reading original RATs...\n");
    // First, read out the existing RATs so that we can put them back
    for (size_t rank = 0; rank < sizeof (originalRats) / sizeof (originalRats[0]); rank++) {
        originalRats[rank] = cellularCtrlGetRat(rank);
    }
    for (size_t rat = CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED + 1; rat < CELLULAR_CTRL_MAX_NUM_RATS; rat++) {
        cellularPortLog("CELLULAR_CTRL_TEST: setting sole RAT to %d...\n", rat);
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetRat(rat) == 0);
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlReboot() == 0);

        for (size_t rank = 0; rank < CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS; rank++) {
            if (rank == 0) {
                cellularPortLog("CELLULAR_CTRL_TEST: checking that the RAT at rank 0 is %d...\n", rat);
                CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRat(rank) == rat);
            } else {
                cellularPortLog("CELLULAR_CTRL_TEST: checking that the there is no RAT at rank %d...\n", rank);
                CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRat(rank) == CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED);
            }
        }
    }

    // Check the number of consecutive AT timeouts
    y = cellularCtrlGetConsecutiveAtTimeouts();
    cellularPortLog("CELLULAR_CTRL_TEST: there have been %d consecutive AT timeouts.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y <= CELLULAR_CTRL_AT_CONSECUTIVE_TIMEOUTS_LIMIT);

    cellularPortLog("CELLULAR_CTRL_TEST: completed, tidying up...\n");
    // No asserts here, we need it to plough on and succeed
    for (size_t rank = 0; rank < sizeof (originalRats) / sizeof (originalRats[0]); rank++) {
        cellularCtrlSetRatRank(originalRats[rank], rank);
    }
    cellularCtrlReboot();
    for (size_t rank = 0; rank < sizeof (originalRats) / sizeof (originalRats[0]); rank++) {
        if (cellularCtrlGetRat(rank) != originalRats[rank]) {
            screwy = true;
        }
    }
    if (screwy) {
        cellularPortLog("CELLULAR_CTRL_TEST:  !!! ATTENTION: the RAT settings of the module under test may have been left screwy, please check!!!\n");
    }
    cellularCtrlPowerOff(NULL);

    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

#if CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS > 1

/** Test set/get RAT rank.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularCtrlTestSetGetRatRank(),
                            "setGetRatRank",
                            "ctrl")
{
    CellularPortQueueHandle_t queueHandle;
    CellularCtrlRat_t originalRats[CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS];
    CellularCtrlRat_t setRats[CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS];
    CellularCtrlRat_t allRats[CELLULAR_CTRL_MAX_NUM_RATS];
    CellularCtrlRat_t rat;
    CellularCtrlRat_t ratTmp;
    size_t count;
    int32_t rank;
    int32_t found;
    int32_t numRats;
    int32_t w;
    bool screwy = false;

    // Fill the array up with cellular RATs and
    // unused values
    for (rank = 0 ; rank < sizeof (allRats) / sizeof (allRats[0]); rank++) {
        allRats[rank] = CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED;
    }
    for (rank = 0, rat = CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED + 1;
         (rank < sizeof (allRats) / sizeof (allRats[0])) && (rat < CELLULAR_CTRL_MAX_NUM_RATS);
         rank++, rat++) {
        allRats[rank] = rat;
    }

    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               CELLULAR_CFG_PIN_VINT,
                                               false,
                                               CELLULAR_CFG_UART,
                                               queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);

    // Before starting, read out the existing RATs so that we can put them back
    cellularPortLog("CELLULAR_CTRL_TEST: reading original RATs...\n");
    for (rank = 0; rank < sizeof (originalRats) / sizeof (originalRats[0]); rank++) {
        originalRats[rank] = cellularCtrlGetRat(rank);
    }
    cellularPortLog("CELLULAR_CTRL_TEST: first, set the sole RAT to %d.\n", allRats[0]);
    // First get the module into a known single-RAT state
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetRat(allRats[0]) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlReboot() == 0);
    //  Note the code below deliberately checks an out of range value
    for (rank = 0; (rank <= sizeof (allRats) / sizeof (allRats[0])) && 
                   (rank < CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS); rank++) {
        rat = cellularCtrlGetRat(rank);
        if (rank == 0) {
            cellularPortLog("CELLULAR_CTRL_TEST: RAT at rank %d is expected to be %d and is %d.\n",
                            rank, allRats[rank], rat);
            CELLULAR_PORT_TEST_ASSERT(rat == allRats[rank]);
        } else {
            if (rank < CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS) {
                cellularPortLog("CELLULAR_CTRL_TEST: RAT at rank %d is expected to be %d.\n",
                                rank, CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED);
                CELLULAR_PORT_TEST_ASSERT(rat == CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED);
            } else {
                cellularPortLog("CELLULAR_CTRL_TEST: asking for the RAT at rank %d is expected to fail and is %d.\n",
                                rank, rat);
                CELLULAR_PORT_TEST_ASSERT(rat < 0);
            }
        }
    }
    // Now set up the maximum number of supported RATs
    // deliberately checking out of range values
    cellularPortLog("CELLULAR_CTRL_TEST: now set a RAT at all %d possible ranks.\n",
                    sizeof (setRats) / sizeof (setRats[0]));
    for (rank = 0; rank <= sizeof (allRats) / sizeof (allRats[0]); rank++) {
        if (rank < sizeof (setRats) / sizeof (setRats[0])) {
            cellularPortLog("CELLULAR_CTRL_TEST: setting RAT at rank %d to %d.\n", rank, allRats[rank]);
            CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetRatRank(allRats[rank], rank) == 0);
        } else {
            cellularPortLog("CELLULAR_CTRL_TEST: try to set RAT at rank %d to %d, should fail.\n", rank, allRats[0]);
            CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetRatRank(allRats[0], rank) < 0);
        }
    }
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlReboot() == 0);
    // Check that worked and remember what was set
    for (rank = 0; rank < sizeof (allRats) / sizeof (allRats[0]); rank++) {
        rat = cellularCtrlGetRat(rank);
        if (rank < sizeof(setRats) / sizeof(setRats[0])) {
            cellularPortLog("CELLULAR_CTRL_TEST: RAT at rank %d is expected to be %d and is %d.\n",
                            rank, allRats[rank], rat);
            CELLULAR_PORT_TEST_ASSERT(rat == allRats[rank]);
            setRats[rank] = allRats[rank];
        } else {
            cellularPortLog("CELLULAR_CTRL_TEST: asking for the RAT at rank %d is expected to fail and is %d.\n",
                            rank, rat);
            CELLULAR_PORT_TEST_ASSERT(rat < 0);
        }
    }
    cellularPortLog("CELLULAR_CTRL_TEST: expected RAT list is now:\n");
    for (size_t rank = 0; rank < sizeof (setRats) / sizeof (setRats[0]); rank++) {
        cellularPortLog("  rank %d: %d.\n", rank, setRats[rank]);
    }
    // Now randomly pick a rank to change and check, in each case,
    // that only the RAT at that rank has changed
    cellularPortLog("CELLULAR_CTRL_TEST: randomly removing RATs at ranks.\n");
    w = 0;
    while (w < 10) {
        // Find a rat to change that leaves us with a non-zero number of RATs
        numRats = 0;
        while (numRats == 0) {
            rank = cellularPort_rand() % (sizeof (setRats) / sizeof (setRats[0]));
            // Find a RAT that isn't the one that is already set at this rank
            // ('cos that would be a pointless test)
            do {
                rat = allRats[cellularPort_rand() % (sizeof (allRats) / sizeof (allRats[0]))];
            }
            while (rat == setRats[rank]);
            
            // Count the number of RATs left
            for (size_t y = 0; y < sizeof(setRats) / sizeof(setRats[0]); y++) {
                ratTmp = setRats[y];
                if (y == rank) {
                    ratTmp = rat;
                }
                if (ratTmp != CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED) {
                    numRats++;
                }
            }
        }
        setRats[rank] = rat;

        w++;
        cellularPortLog("CELLULAR_CTRL_TEST: changing RAT at rank %d to %d.\n", rank, setRats[rank]);
        // Do the setting
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetRatRank(setRats[rank], rank) == 0);
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlReboot() == 0);
        // Remove duplicates from the set RAT list
        for (size_t y = 0; y < sizeof(setRats) / sizeof(setRats[0]); y++) {
            for (size_t z = y + 1; z < sizeof(setRats) / sizeof(setRats[0]); z++) {
                if ((setRats[y] > CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED) && (setRats[y] == setRats[z])) {
                    setRats[z] = CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED;
                }
            }
        }
        // Sort empty values to the end as the driver does
        count = 0;
        for (size_t y = 0; y < sizeof(setRats) / sizeof(setRats[0]); y++) {
            if (setRats[y] != CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED) {
                setRats[count] = setRats[y];
                count++;
            }
        }
        for (; count < sizeof(setRats) / sizeof(setRats[0]); count++) {
            setRats[count] = CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED;
        }
        cellularPortLog("CELLULAR_CTRL_TEST: new expected RAT list is:\n");
        for (size_t y = 0; y < sizeof (setRats) / sizeof (setRats[0]); y++) {
            cellularPortLog("  rank %d: %d.\n", y, setRats[y]);
        }
        // Check that the RATs are as expected
        cellularPortLog("CELLULAR_CTRL_TEST: checking that the module agrees...\n");
        for (size_t y = 0; y < sizeof (setRats) / sizeof (setRats[0]); y++) {
            rat = cellularCtrlGetRat(y);
            cellularPortLog("  RAT at rank %d is expected to be %d and is %d.\n",
                            y, setRats[y], rat);
            CELLULAR_PORT_TEST_ASSERT(rat == setRats[y]);
        }
        for (size_t y = 0 ; y < sizeof (allRats) / sizeof (allRats[0]); y++) {
            if (allRats[y] != CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED) {
                found = -1;
                for (size_t z = 0; (found < 0) &&
                                   (z < sizeof (setRats) / sizeof (setRats[0])); z++) {
                    if (setRats[z] == allRats[y]) {
                        found = z;
                    }
                }
                rank = cellularCtrlGetRatRank(allRats[y]);
                if (found < 0) {
                    if (rank >= 0) {
                        cellularPortLog("  RAT %d is expected to be not ranked but is ranked at %d.\n",
                                        allRats[y], rank);
                        CELLULAR_PORT_TEST_ASSERT(false);
                    }
                } else {
                    cellularPortLog("  rank of RAT %d is expected to be %d and is %d.\n",
                                    allRats[y], found, rank);
                    CELLULAR_PORT_TEST_ASSERT(found == rank);
                }
            }
        }
    }

    // Check the number of consecutive AT timeouts
    w = cellularCtrlGetConsecutiveAtTimeouts();
    cellularPortLog("CELLULAR_CTRL_TEST: there have been %d consecutive AT timeouts.\n", w);
    CELLULAR_PORT_TEST_ASSERT(w <= CELLULAR_CTRL_AT_CONSECUTIVE_TIMEOUTS_LIMIT);

    cellularPortLog("CELLULAR_CTRL_TEST: completed, tidying up...\n");
    // No asserts here, we need it to plough on and succeed
    for (size_t rank = 0; rank < sizeof (originalRats) / sizeof (originalRats[0]); rank++) {
        cellularCtrlSetRatRank(originalRats[rank], rank);
    }
    cellularCtrlReboot();
    for (size_t rank = 0; rank < sizeof (originalRats) / sizeof (originalRats[0]); rank++) {
        if (cellularCtrlGetRat(rank) != originalRats[rank]) {
            screwy = true;
        }
    }
    if (screwy) {
        cellularPortLog("CELLULAR_CTRL_TEST: !!! ATTENTION: the RAT settings of the module under test may have been left screwy, please check!!!\n");
    }

    cellularCtrlPowerOff(NULL);
    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

#endif

/** Test connected things on the default test RAT.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularCtrlTestConnectedThings(),
                            "connectedThings",
                            "ctrl")
{
    connectDisconnect(CELLULAR_CFG_TEST_RAT);
}

/** Test get/set MNO profile.  Note that this test requires the
 * ability to connect with a network in order to check that
 * setting of an MNO profile is not allowed when connected.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularCtrlTestMnoProfile(),
                            "getSetMnoProfile",
                            "ctrl")
{
    CellularPortQueueHandle_t queueHandle;
    CellularCtrlRat_t originalRats[CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS];
    uint64_t originalMask1;
    uint64_t originalMask2;
    int32_t originalMnoProfile;
    int32_t mnoProfile;
    bool screwy = false;
    int32_t y;

    for (size_t x = 0; x < sizeof (originalRats) / sizeof (originalRats[0]); x++) {
        originalRats[x] = CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED;
    }

    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               CELLULAR_CFG_PIN_VINT,
                                               false,
                                               CELLULAR_CFG_UART,
                                               queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: preparing for test...\n");
    // First, read out the existing RATs so that we can put them back
    for (size_t x = 0; x < sizeof (originalRats) / sizeof (originalRats[0]); x++) {
        originalRats[x] = cellularCtrlGetRat(x);
    }
    // Read out the original MNO profile
    originalMnoProfile = cellularCtrlGetMnoProfile();
    // Then read out the existing band mask
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetBandMask(CELLULAR_CFG_TEST_RAT,
                                                      &originalMask1,
                                                      &originalMask2) == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: setting sole RAT to %d and bandmask to 0x%016llx %016llx so that we can register with a network...\n",
                    CELLULAR_CFG_TEST_RAT,
                    CELLULAR_CFG_TEST_BANDMASK2,
                    CELLULAR_CFG_TEST_BANDMASK1);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetRat(CELLULAR_CFG_TEST_RAT) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetBandMask(CELLULAR_CFG_TEST_RAT,
                                                      CELLULAR_CFG_TEST_BANDMASK1,
                                                      CELLULAR_CFG_TEST_BANDMASK2) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlReboot() == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: getting MNO profile...\n");
    CELLULAR_PORT_TEST_ASSERT(originalMnoProfile >= 0);
    // Need to be careful here as changing the
    // MNO profile changes the RAT and the BAND
    // as well.  0 is the default one, which should
    // work and 100 is Europe.
    if (originalMnoProfile != 100) {
        mnoProfile = 100;
    } else {
        mnoProfile = 0;
    }

    cellularPortLog("CELLULAR_CTRL_TEST: trying to set MNO profile while connected...\n");
    gStopTimeMS = cellularPortGetTickTimeMs()  + (CELLULAR_CFG_TEST_CONNECT_TIMEOUT_SECONDS * 1000);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlConnect(keepGoingCallback, NULL, NULL, NULL) == 0);
    cellularPortLog("CELLULAR_CTRL_TEST: cellularCtrlGetNetworkStatus() %d.\n",
                    cellularCtrlGetNetworkStatus());
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetNetworkStatus() == CELLULAR_CTRL_NETWORK_STATUS_REGISTERED);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetMnoProfile(mnoProfile) == CELLULAR_CTRL_CONNECTED);

    cellularPortLog("CELLULAR_CTRL_TEST: disconnecting to really set MNO profile...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlDisconnect() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetMnoProfile(mnoProfile) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlReboot() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetMnoProfile() == mnoProfile);

    cellularPortLog("CELLULAR_CTRL_TEST: completed, tidying up...\n");
    // No asserts here, we need it to plough on and succeed
    cellularCtrlSetMnoProfile(originalMnoProfile);
    cellularCtrlReboot();
    if (cellularCtrlGetMnoProfile() != originalMnoProfile) {
        cellularPortLog("CELLULAR_CTRL_TEST: !!! ATTENTION: the MNO profile of the module under test may have been left screwy, please check!!!\n");
    }
    cellularCtrlReboot();
    if (cellularCtrlSetBandMask(CELLULAR_CFG_TEST_RAT, originalMask1,
                                                       originalMask2) != 0) {
        cellularPortLog("CELLULAR_CTRL_TEST: !!! ATTENTION: the band mask for RAT %d on the module under test may have been left screwy, please check!!!\n",
                        CELLULAR_CFG_TEST_RAT);
    }
    cellularCtrlReboot();
    for (size_t x = 0; x < sizeof (originalRats) / sizeof (originalRats[0]); x++) {
        cellularCtrlSetRatRank(originalRats[x], x);
    }
    cellularCtrlReboot();

    for (size_t x = 0; x < sizeof (originalRats) / sizeof (originalRats[0]); x++) {
        if (cellularCtrlGetRat(x) != originalRats[x]) {
            screwy = true;
        }
    }
    if (screwy) {
        cellularPortLog("CELLULAR_CTRL_TEST: !!! ATTENTION: the RAT settings of the module under test may have been left screwy, please check!!!\n");
    }

    cellularCtrlPowerOff(NULL);

    // Check the number of consecutive AT timeouts
    y = cellularCtrlGetConsecutiveAtTimeouts();
    cellularPortLog("CELLULAR_CTRL_TEST: there have been %d consecutive AT timeouts.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y <= CELLULAR_CTRL_AT_CONSECUTIVE_TIMEOUTS_LIMIT);

    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

/** Test reading the radio parameters.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularCtrlTestReadRadioParameters(),
                            "readRadioParameters",
                            "ctrl")
{
    CellularPortQueueHandle_t queueHandle;
    CellularCtrlRat_t originalRats[CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS];
    uint64_t originalMask1;
    uint64_t originalMask2;
    int32_t count;
    int32_t snrDb;
    bool screwy = false;
    int32_t y;

    for (size_t x = 0; x < sizeof (originalRats) / sizeof (originalRats[0]); x++) {
        originalRats[x] = CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED;
    }

    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               CELLULAR_CFG_PIN_VINT,
                                               false,
                                               CELLULAR_CFG_UART,
                                               queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: preparing for test...\n");
    // First, read out the existing RATs so that we can put them back
    for (size_t x = 0; x < sizeof (originalRats) / sizeof (originalRats[0]); x++) {
        originalRats[x] = cellularCtrlGetRat(x);
    }
    // Read out the existing band mask
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetBandMask(CELLULAR_CFG_TEST_RAT,
                                                      &originalMask1,
                                                      &originalMask2) == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: setting sole RAT to %d and bandmask to 0x%016llx %016llx so that we can register with a network...\n",
                    CELLULAR_CFG_TEST_RAT,
                    CELLULAR_CFG_TEST_BANDMASK2,
                    CELLULAR_CFG_TEST_BANDMASK1);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetRat(CELLULAR_CFG_TEST_RAT) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetBandMask(CELLULAR_CFG_TEST_RAT,
                                                      CELLULAR_CFG_TEST_BANDMASK1,
                                                      CELLULAR_CFG_TEST_BANDMASK2) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlReboot() == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: checking values before a refresh (should return errors)...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRssiDbm() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRsrpDbm() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRsrqDbm() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetSnrDb(&snrDb) != 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetCellId() == -1);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetEarfcn() == -1);

    cellularPortLog("CELLULAR_CTRL_TEST: checking values after a refresh but before network registration (should return errors)...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlRefreshRadioParameters() != 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRssiDbm() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRsrpDbm() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRsrqDbm() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetSnrDb(&snrDb) != 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetCellId() == -1);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetEarfcn() == -1);

    cellularPortLog("CELLULAR_CTRL_TEST: checking values after registration...\n");
    gStopTimeMS = cellularPortGetTickTimeMs()  + (CELLULAR_CFG_TEST_CONNECT_TIMEOUT_SECONDS * 1000);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlConnect(keepGoingCallback, NULL, NULL, NULL) == 0);
    cellularPortLog("CELLULAR_CTRL_TEST: cellularCtrlGetNetworkStatus() %d.\n",
                    cellularCtrlGetNetworkStatus());
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetNetworkStatus() == CELLULAR_CTRL_NETWORK_STATUS_REGISTERED);
    // Attempt this a number of times as it can return a temporary "operation not allowed" error
    for (count = 10; (cellularCtrlRefreshRadioParameters() != 0) && (count > 0); count--) {
        cellularPortTaskBlock(1000);
    }
    CELLULAR_PORT_TEST_ASSERT(count > 0);
    // Should now have everything
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRsrpDbm() < 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRsrqDbm() < 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetCellId() >= 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetEarfcn() >= 0);
    // ...however RSSI can take a long time to
    // get so keep trying if it's not arrived
    for (count = 30; (cellularCtrlGetRssiDbm() == 0) && (count > 0); count--) {
        cellularCtrlRefreshRadioParameters();
        cellularPortTaskBlock(1000);
    }
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetRssiDbm() < 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetSnrDb(&snrDb) == 0);
    cellularPortLog("CELLULAR_CTRL_TEST: SNR is %d dB.\n", snrDb);

    CELLULAR_PORT_TEST_ASSERT(cellularCtrlDisconnect() == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: completed, tidying up...\n");
    // No asserts here, we need it to plough on and succeed
    if (cellularCtrlSetBandMask(CELLULAR_CFG_TEST_RAT, originalMask1,
                                                       originalMask2) != 0) {
        cellularPortLog("CELLULAR_CTRL_TEST: !!! ATTENTION: the band mask for cat-M1 on the module under test may have been left screwy, please check!!!\n");
    }
    for (size_t x = 0; x < sizeof (originalRats) / sizeof (originalRats[0]); x++) {
        cellularCtrlSetRatRank(originalRats[x], x);
    }
    cellularCtrlReboot();
    for (size_t x = 0; x < sizeof (originalRats) / sizeof (originalRats[0]); x++) {
        if (cellularCtrlGetRat(x) != originalRats[x]) {
            screwy = true;
        }
    }
    if (screwy) {
        cellularPortLog("CELLULAR_CTRL_TEST:  !!! ATTENTION: the RAT settings of the module under test may have been left screwy, please check!!!\n");
    }
    cellularCtrlPowerOff(NULL);

    // Check the number of consecutive AT timeouts
    y = cellularCtrlGetConsecutiveAtTimeouts();
    cellularPortLog("CELLULAR_CTRL_TEST: there have been %d consecutive AT timeouts.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y <= CELLULAR_CTRL_AT_CONSECUTIVE_TIMEOUTS_LIMIT);

    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

/** Get IMEI etc.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularCtrlTestReadImeiEtc(),
                            "readImeiEtc",
                            "ctrl")
{
    CellularPortQueueHandle_t queueHandle;
    char buffer[64];
    int32_t bytesRead;
    int32_t y;

    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               CELLULAR_CFG_PIN_VINT,
                                               false,
                                               CELLULAR_CFG_UART,
                                               queueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);

    cellularPortLog("CELLULAR_CTRL_TEST: getting and checking IMEI...\n");
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetImei(buffer) >= 0);
    for (size_t x = 0; x < sizeof(buffer); x++) {
        if (x < CELLULAR_CTRL_IMEI_SIZE) {
            CELLULAR_PORT_TEST_ASSERT((buffer[x] >= '0') && (buffer[x] <= '9'));
        } else {
            CELLULAR_PORT_TEST_ASSERT(buffer[x] == 0);
        }
    }
    cellularPortLog("CELLULAR_CTRL_TEST: getting and checking IMSI...\n");
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetImsi(buffer) >= 0);
    for (size_t x = 0; x < sizeof(buffer); x++) {
        if (x < CELLULAR_CTRL_IMSI_SIZE) {
            CELLULAR_PORT_TEST_ASSERT((buffer[x] >= '0') && (buffer[x] <= '9'));
        } else {
            CELLULAR_PORT_TEST_ASSERT(buffer[x] == 0);
        }
    }
    cellularPortLog("CELLULAR_CTRL_TEST: getting and checking ICCID...\n");
    // First use an unrealistically short buffer and check
    // that there is no overrun
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    bytesRead = cellularCtrlGetIccidStr(buffer, 1);
    CELLULAR_PORT_TEST_ASSERT(bytesRead == 0);
    for (size_t x = bytesRead; x < sizeof(buffer); x++) {
        CELLULAR_PORT_TEST_ASSERT(buffer[x] == 0);
    }
    // Now read it properly
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetIccidStr(buffer, sizeof(buffer)) >= 0);
    // Can't really do a check here as the number of digits
    // in an ICCID can vary
    cellularPortLog("CELLULAR_CTRL_TEST: getting and checking manufacturer string...\n");
    // First use an unrealistically short buffer and check
    // that there is no overrun
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    bytesRead = cellularCtrlGetManufacturerStr(buffer, 1);
    CELLULAR_PORT_TEST_ASSERT(bytesRead == 0);
    for (size_t x = bytesRead; x < sizeof(buffer); x++) {
        CELLULAR_PORT_TEST_ASSERT(buffer[x] == 0);
    }
    // Now read it properly
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    bytesRead = cellularCtrlGetManufacturerStr(buffer, sizeof(buffer));
    CELLULAR_PORT_TEST_ASSERT((bytesRead > 0) && (bytesRead < sizeof(buffer) - 1) && (bytesRead == cellularPort_strlen(buffer)));
    cellularPortLog("CELLULAR_CTRL_TEST: getting and checking model string...\n");
    // First use an unrealistically short buffer and check
    // that there is no overrun
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    bytesRead = cellularCtrlGetModelStr(buffer, 1);
    CELLULAR_PORT_TEST_ASSERT(bytesRead == 0);
    for (size_t x = bytesRead; x < sizeof(buffer); x++) {
        CELLULAR_PORT_TEST_ASSERT(buffer[x] == 0);
    }
    // Now read it properly
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    bytesRead = cellularCtrlGetModelStr(buffer, sizeof(buffer));
    CELLULAR_PORT_TEST_ASSERT((bytesRead > 0) && (bytesRead < sizeof(buffer) - 1) && (bytesRead == cellularPort_strlen(buffer)));
    cellularPortLog("CELLULAR_CTRL_TEST: getting and checking firmware version string...\n");
    // First use an unrealistically short buffer and check
    // that there is no overrun
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    bytesRead = cellularCtrlGetFirmwareVersionStr(buffer, 1);
    CELLULAR_PORT_TEST_ASSERT(bytesRead == 0);
    for (size_t x = bytesRead; x < sizeof(buffer); x++) {
        CELLULAR_PORT_TEST_ASSERT(buffer[x] == 0);
    }
    // Now read it properly
    pCellularPort_memset(buffer, 0, sizeof(buffer));
    bytesRead = cellularCtrlGetFirmwareVersionStr(buffer, sizeof(buffer));
    CELLULAR_PORT_TEST_ASSERT((bytesRead > 0) && (bytesRead < sizeof(buffer) - 1) && (bytesRead == cellularPort_strlen(buffer)));

    cellularCtrlPowerOff(NULL);

    // Check the number of consecutive AT timeouts
    y = cellularCtrlGetConsecutiveAtTimeouts();
    cellularPortLog("CELLULAR_CTRL_TEST: there have been %d consecutive AT timeouts.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y <= CELLULAR_CTRL_AT_CONSECUTIVE_TIMEOUTS_LIMIT);

    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

// End of file
