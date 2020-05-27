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
#include "cellular_mqtt.h"
#include "cellular_cfg_test.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Used for keepGoingCallback() timeout.
static int64_t gStopTimeMs;

// The UART queue handle: kept as a global variable
// because if a test fails init will have run but
// deinit will have been skipped.  With this as a global,
// when the inits skip doing their thing because
// things are already init'ed, the subsequent
// functions will continue to use this valid queue
// handle.
static CellularPortQueueHandle_t gUartQueueHandle = NULL;

// Place to store the original RAT settings of the module.
static CellularCtrlRat_t gOriginalRats[CELLULAR_CTRL_MAX_NUM_SIMULTANEOUS_RATS];

// Place to store the original band mask settings of the module.
static uint64_t gOriginalMask1;
static uint64_t gOriginalMask2;

// A string of all possible characters, including strings
// that might appear as terminators in the AT interface
static const char gAllChars[] = "the quick brown fox jumps over the lazy dog "
                                "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789 "
                                "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                                "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c"
                                "\x1d\x1e!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\x7f"
                                "\r\nOK\r\n \r\nERROR\r\n";

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the cellular networkConnect process.
static bool keepGoingCallback()
{
    bool keepGoing = true;

    if (cellularPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

// Connect to the network, saving existing settings first.
static void networkConnect(const char *pApn,
                           const char *pUsername,
                           const char *pPassword)
{
    for (size_t x = 0; x < sizeof (gOriginalRats) / sizeof (gOriginalRats[0]); x++) {
        gOriginalRats[x] = CELLULAR_CTRL_RAT_UNKNOWN_OR_NOT_USED;
    }

    cellularPortLog("CELLULAR_MQTT_TEST: saving existing settings...\n");
    // First, read out the existing RATs so that we can put them back
    for (size_t x = 0; x < sizeof (gOriginalRats) / sizeof (gOriginalRats[0]); x++) {
        gOriginalRats[x] = cellularCtrlGetRat(x);
    }
    if ((CELLULAR_CFG_TEST_RAT == CELLULAR_CTRL_RAT_CATM1) || (CELLULAR_CFG_TEST_RAT == CELLULAR_CTRL_RAT_NB1)) {
        // Then read out the existing band masks
       CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetBandMask(CELLULAR_CFG_TEST_RAT,
                                                         &gOriginalMask1, &gOriginalMask2) == 0);
    }
    cellularPortLog("CELLULAR_MQTT_TEST: setting sole RAT to %d...\n", CELLULAR_CFG_TEST_RAT);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetRat(CELLULAR_CFG_TEST_RAT) == 0);
    if ((CELLULAR_CFG_TEST_RAT == CELLULAR_CTRL_RAT_CATM1) || (CELLULAR_CFG_TEST_RAT == CELLULAR_CTRL_RAT_NB1)) {
        cellularPortLog("CELLULAR_CTRL_TEST: setting bandmask to 0x%08x%08x %08x%08x...\n",
                        (uint32_t) (CELLULAR_CFG_TEST_BANDMASK2 >> 32),
                        (uint32_t) CELLULAR_CFG_TEST_BANDMASK2,
                        (uint32_t) (CELLULAR_CFG_TEST_BANDMASK1 >> 32),
                        (uint32_t) CELLULAR_CFG_TEST_BANDMASK1);
        CELLULAR_PORT_TEST_ASSERT(cellularCtrlSetBandMask(CELLULAR_CFG_TEST_RAT,
                                                          CELLULAR_CFG_TEST_BANDMASK1,
                                                          CELLULAR_CFG_TEST_BANDMASK2) == 0);
    }
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlReboot() == 0);

    cellularPortLog("CELLULAR_MQTT_TEST: connecting...\n");
    gStopTimeMs = cellularPortGetTickTimeMs() + (CELLULAR_CFG_TEST_CONNECT_TIMEOUT_SECONDS * 1000);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlConnect(keepGoingCallback,
                                                  pApn, pUsername, pPassword) == 0);
    cellularPortLog("CELLULAR_MQTT_TEST: RAT %d, cellularCtrlGetNetworkStatus() %d.\n",
                    CELLULAR_CFG_TEST_RAT, cellularCtrlGetNetworkStatus(cellularCtrlGetRanForRat(CELLULAR_CFG_TEST_RAT)));
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetNetworkStatus(cellularCtrlGetRanForRat(CELLULAR_CFG_TEST_RAT)) == CELLULAR_CTRL_NETWORK_STATUS_REGISTERED);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlIsRegistered());
}

// Disconnect from the network and restore teh saved settings.
static void networkDisconnect()
{
    bool screwy = false;

    cellularPortLog("CELLULAR_MQTT_TEST: disconnecting...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlDisconnect() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlGetNetworkStatus(cellularCtrlGetRanForRat(CELLULAR_CFG_TEST_RAT)) != CELLULAR_CTRL_NETWORK_STATUS_REGISTERED);
    CELLULAR_PORT_TEST_ASSERT(!cellularCtrlIsRegistered());

    cellularPortLog("CELLULAR_MQTT_TEST: restoring saved settings...\n");
    if ((CELLULAR_CFG_TEST_RAT == CELLULAR_CTRL_RAT_CATM1) || (CELLULAR_CFG_TEST_RAT == CELLULAR_CTRL_RAT_NB1)) {
        // No asserts here, we need it to plough on and succeed
        if (cellularCtrlSetBandMask(CELLULAR_CFG_TEST_RAT, gOriginalMask1,
                                                           gOriginalMask2) != 0) {
            cellularPortLog("CELLULAR_MQTT_TEST: !!! ATTENTION: the band mask for RAT %d on the module under test may have been left screwy, please check!!!\n", CELLULAR_CFG_TEST_RAT);
        }
    }
    for (size_t x = 0; x < sizeof (gOriginalRats) / sizeof (gOriginalRats[0]); x++) {
        cellularCtrlSetRatRank(gOriginalRats[x], x);
    }
    for (size_t x = 0; x < sizeof (gOriginalRats) / sizeof (gOriginalRats[0]); x++) {
        if (cellularCtrlGetRat(x) != gOriginalRats[x]) {
            screwy = true;
        }
    }
    if (screwy) {
        cellularPortLog("CELLULAR_MQTT_TEST: !!! ATTENTION: the RAT settings of the module under test may have been left screwy, please check!!!\n");
    }
    cellularCtrlReboot();
}

// Callback for unread message indications
static void messageIndicationCallback(int32_t numUnread, void *pParam)
{
    int32_t *pNumUnread = (int32_t *) pParam;

    cellularPortLog("messageIndicationCallback() called.\n");
    cellularPortLog("%d message(s) unread.\n", numUnread);
    *pNumUnread = numUnread;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Basic test: initialise and then deinitialise everything.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularMqttTestInitialisation(),
                            "mqttInitialisation",
                            "mqtt")
{
    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &gUartQueueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               CELLULAR_CFG_PIN_VINT,
                                               false,
                                               CELLULAR_CFG_UART,
                                               gUartQueueHandle) == 0);

    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);

    gStopTimeMs = cellularPortGetTickTimeMs() + (CELLULAR_CFG_TEST_MQTT_SERVER_TIMEOUT_SECONDS * 1000);
    CELLULAR_PORT_TEST_ASSERT(cellularMqttInit(CELLULAR_CFG_TEST_MQTT_SERVER_DOMAIN_NAME,
                                               CELLULAR_CFG_TEST_MQTT_SERVER_USERNAME,
                                               CELLULAR_CFG_TEST_MQTT_SERVER_PASSWORD,
                                               NULL,
                                               keepGoingCallback) == 0);

    cellularCtrlPowerOff(NULL);
    cellularMqttDeinit();

    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

/** Connect to an MQTT server.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularMqttTestConnectDisconnect(),
                            "mqttConnectDisconnect",
                            "mqtt")
{
    char buffer[32];
    int32_t y;
    int64_t startTimeMs;

    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &gUartQueueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               CELLULAR_CFG_PIN_VINT,
                                               false,
                                               CELLULAR_CFG_UART,
                                               gUartQueueHandle) == 0);

    // Call this first in a previous failed test left things initialised
    cellularMqttDeinit();

    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);

    networkConnect(CELLULAR_CFG_TEST_APN,
                   CELLULAR_CFG_TEST_USERNAME,
                   CELLULAR_CFG_TEST_PASSWORD);

    cellularPortLog("CELLULAR_MQTT_TEST: initialising MQTT with server \"%s\"...\n",
                    CELLULAR_CFG_TEST_MQTT_SERVER_DOMAIN_NAME);
    CELLULAR_PORT_TEST_ASSERT(cellularMqttInit(CELLULAR_CFG_TEST_MQTT_SERVER_DOMAIN_NAME,
                                               CELLULAR_CFG_TEST_MQTT_SERVER_USERNAME,
                                               CELLULAR_CFG_TEST_MQTT_SERVER_PASSWORD,
                                               "bong",
                                               keepGoingCallback) == 0);

    cellularPortLog("CELLULAR_MQTT_TEST: getting local MQTT client name...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularMqttGetClientName(buffer, sizeof(buffer)) == 0);
    cellularPortLog("CELLULAR_MQTT_TEST: local MQTT client name is \"%s\".\n", buffer);
    CELLULAR_PORT_TEST_ASSERT(cellularPort_strcmp(buffer, "bong") == 0);

    cellularPortLog("CELLULAR_MQTT_TEST: getting local MQTT port...\n");
    y = cellularMqttGetLocalPort();
    cellularPortLog("CELLULAR_MQTT_TEST: local MQTT port is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == CELLULAR_MQTT_SERVER_PORT_UNSECURE);

    cellularPortLog("CELLULAR_MQTT_TEST: getting inactivity timeout...\n");
    y = cellularMqttGetInactivityTimeout();
    cellularPortLog("CELLULAR_MQTT_TEST: inactivity timeout is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == 0);

    cellularPortLog("CELLULAR_MQTT_TEST: getting keep-alive value...\n");
    y = cellularMqttIsKeptAlive();
    cellularPortLog("CELLULAR_MQTT_TEST: keep-alive value is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == 0);

    cellularPortLog("CELLULAR_MQTT_TEST: getting session retention value...\n");
    y = cellularMqttIsSessionRetained();
    cellularPortLog("CELLULAR_MQTT_TEST: session retention value is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == 0);

    cellularPortLog("CELLULAR_MQTT_TEST: getting security value...\n");
    y = cellularMqttIsSecured(NULL);
    cellularPortLog("CELLULAR_MQTT_TEST: security value is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == 0);

    startTimeMs = cellularPortGetTickTimeMs();
    cellularPortLog("CELLULAR_MQTT_TEST: connecting to \"%s\"...\n",
                    CELLULAR_CFG_TEST_MQTT_SERVER_DOMAIN_NAME);
    gStopTimeMs = cellularPortGetTickTimeMs() + (CELLULAR_CFG_TEST_MQTT_SERVER_TIMEOUT_SECONDS * 1000);
    y = cellularMqttConnect(CELLULAR_CFG_TEST_MQTT_SERVER_DOMAIN_NAME);
    if (y == 0) {
        cellularPortLog("CELLULAR_MQTT_TEST: connected after %d seconds.\n",
                        ((int32_t) (cellularPortGetTickTimeMs() - startTimeMs)) / 1000);
        CELLULAR_PORT_TEST_ASSERT(cellularMqttIsConnected());
    } else {
        cellularPortLog("CELLULAR_MQTT_TEST: not connected after %d seconds, module error %d.\n",
                        ((int32_t) (cellularPortGetTickTimeMs() - startTimeMs)) / 1000,
                        cellularMqttGetLastErrorCode());
        CELLULAR_PORT_TEST_ASSERT(!cellularMqttIsConnected());
        CELLULAR_PORT_TEST_ASSERT(false);
    }

    cellularPortLog("CELLULAR_MQTT_TEST: disconnecting again...\n");
    gStopTimeMs = cellularPortGetTickTimeMs() + (CELLULAR_CFG_TEST_MQTT_SERVER_TIMEOUT_SECONDS * 1000);
    CELLULAR_PORT_TEST_ASSERT(cellularMqttDisconnect() == 0);
    CELLULAR_PORT_TEST_ASSERT(!cellularMqttIsConnected());

    // Disconnect from the cellular network and tidy up
    networkDisconnect();

    cellularCtrlPowerOff(NULL);
    cellularMqttDeinit();

    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

/** Subscribe/publish messages with an MQTT server.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularMqttTestSubscribePublish(),
                            "mqttSubscribePublish",
                            "mqtt")
{
    char buffer[32];
    int32_t y;
    int32_t numPublished = 0;
    int32_t numUnread = 0;
    int64_t startTimeMs;

    CELLULAR_PORT_TEST_ASSERT(cellularPortInit() == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartInit(CELLULAR_CFG_PIN_TXD,
                                                   CELLULAR_CFG_PIN_RXD,
                                                   CELLULAR_CFG_PIN_CTS,
                                                   CELLULAR_CFG_PIN_RTS,
                                                   CELLULAR_CFG_BAUD_RATE,
                                                   CELLULAR_CFG_RTS_THRESHOLD,
                                                   CELLULAR_CFG_UART,
                                                   &gUartQueueHandle) == 0);
    CELLULAR_PORT_TEST_ASSERT(cellularCtrlInit(CELLULAR_CFG_PIN_ENABLE_POWER,
                                               CELLULAR_CFG_PIN_PWR_ON,
                                               CELLULAR_CFG_PIN_VINT,
                                               false,
                                               CELLULAR_CFG_UART,
                                               gUartQueueHandle) == 0);

    // Call this first in a previous failed test left things initialised
    cellularMqttDeinit();

    CELLULAR_PORT_TEST_ASSERT(cellularCtrlPowerOn(NULL) == 0);

    networkConnect(CELLULAR_CFG_TEST_APN,
                   CELLULAR_CFG_TEST_USERNAME,
                   CELLULAR_CFG_TEST_PASSWORD);

    cellularPortLog("CELLULAR_MQTT_TEST: initialising MQTT with server \"%s\"...\n",
                    CELLULAR_CFG_TEST_MQTT_SERVER_DOMAIN_NAME);
    CELLULAR_PORT_TEST_ASSERT(cellularMqttInit(CELLULAR_CFG_TEST_MQTT_SERVER_DOMAIN_NAME,
                                               CELLULAR_CFG_TEST_MQTT_SERVER_USERNAME,
                                               CELLULAR_CFG_TEST_MQTT_SERVER_PASSWORD,
                                               NULL,
                                               keepGoingCallback) == 0);

    cellularPortLog("CELLULAR_MQTT_TEST: getting local MQTT client name...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularMqttGetClientName(buffer, sizeof(buffer)) == 0);
    cellularPortLog("CELLULAR_MQTT_TEST: local MQTT client name is \"%s\".\n", buffer);

#ifdef CELLULAR_CFG_MODULE_SARA_R5
    CELLULAR_PORT_TEST_ASSERT(cellularMqttSetLocalPort(10) == CELLULAR_MQTT_NOT_SUPPORTED);
#else
    cellularPortLog("CELLULAR_MQTT_TEST: setting local MQTT port to %d...\n", 10);
    CELLULAR_PORT_TEST_ASSERT(cellularMqttSetLocalPort(10) == 0);
    y = cellularMqttGetLocalPort();
    cellularPortLog("CELLULAR_MQTT_TEST: local MQTT port is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == 10);
    cellularPortLog("CELLULAR_MQTT_TEST: setting local MQTT port to %d...\n",
                    CELLULAR_MQTT_SERVER_PORT_UNSECURE);
    CELLULAR_PORT_TEST_ASSERT(cellularMqttSetLocalPort(CELLULAR_MQTT_SERVER_PORT_UNSECURE) == 0);
#endif
    y = cellularMqttGetLocalPort();
    cellularPortLog("CELLULAR_MQTT_TEST: local MQTT port is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == CELLULAR_MQTT_SERVER_PORT_UNSECURE);

    cellularPortLog("CELLULAR_MQTT_TEST: setting inactivity timeout to %d"
                    " second(s)...\n", 60);
    CELLULAR_PORT_TEST_ASSERT(cellularMqttSetInactivityTimeout(60) == 0);
    y = cellularMqttGetInactivityTimeout();
    cellularPortLog("CELLULAR_MQTT_TEST: inactivity timeout is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == 60);

    cellularPortLog("CELLULAR_MQTT_TEST: switching keep-alive on...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularMqttSetKeepAliveOn() == 0);
    cellularPortLog("CELLULAR_MQTT_TEST: getting keep-alive value...\n");
    y = cellularMqttIsKeptAlive();
    cellularPortLog("CELLULAR_MQTT_TEST: keep-alive value is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == 1);
    cellularPortLog("CELLULAR_MQTT_TEST: switching keep-alive off again...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularMqttSetKeepAliveOff() == 0);
    cellularPortLog("CELLULAR_MQTT_TEST: getting keep-alive value...\n");
    y = cellularMqttIsKeptAlive();
    cellularPortLog("CELLULAR_MQTT_TEST: keep-alive value is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == 0);

#ifdef CELLULAR_CFG_MODULE_SARA_R5
    CELLULAR_PORT_TEST_ASSERT(cellularMqttSetSessionRetentionOn() == CELLULAR_MQTT_NOT_SUPPORTED);
    CELLULAR_PORT_TEST_ASSERT(cellularMqttSetSessionRetentionOff() == CELLULAR_MQTT_NOT_SUPPORTED);
#else
    cellularPortLog("CELLULAR_MQTT_TEST: switching session retention on...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularMqttSetSessionRetentionOn() == 0);
    cellularPortLog("CELLULAR_MQTT_TEST: getting session retention value...\n");
    y = cellularMqttIsSessionRetained();
    cellularPortLog("CELLULAR_MQTT_TEST: session retention value is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == 1);
    cellularPortLog("CELLULAR_MQTT_TEST: switching session retention off again...\n");
    CELLULAR_PORT_TEST_ASSERT(cellularMqttSetSessionRetentionOff() == 0);
    cellularPortLog("CELLULAR_MQTT_TEST: getting session retention value...\n");
#endif
    y = cellularMqttIsSessionRetained();
    cellularPortLog("CELLULAR_MQTT_TEST: session retention value is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == 0);

    cellularPortLog("CELLULAR_MQTT_TEST: getting security value...\n");
    y = cellularMqttIsSecured(NULL);
    cellularPortLog("CELLULAR_MQTT_TEST: security value is %d.\n", y);
    CELLULAR_PORT_TEST_ASSERT(y == 0);

    startTimeMs = cellularPortGetTickTimeMs();
    cellularPortLog("CELLULAR_MQTT_TEST: connecting to \"%s\"...\n",
                    CELLULAR_CFG_TEST_MQTT_SERVER_DOMAIN_NAME);
    gStopTimeMs = cellularPortGetTickTimeMs() + (CELLULAR_CFG_TEST_MQTT_SERVER_TIMEOUT_SECONDS * 1000);
    y = cellularMqttConnect(CELLULAR_CFG_TEST_MQTT_SERVER_DOMAIN_NAME);
    if (y == 0) {
        cellularPortLog("CELLULAR_MQTT_TEST: connected after %d seconds.\n",
                        ((int32_t) (cellularPortGetTickTimeMs() - startTimeMs)) / 1000);
        CELLULAR_PORT_TEST_ASSERT(cellularMqttIsConnected());
    } else {
        cellularPortLog("CELLULAR_MQTT_TEST: not connected after %d seconds, module error %d.\n",
                        ((int32_t) (cellularPortGetTickTimeMs() - startTimeMs)) / 1000,
                        cellularMqttGetLastErrorCode());
        CELLULAR_PORT_TEST_ASSERT(!cellularMqttIsConnected());
        CELLULAR_PORT_TEST_ASSERT(false);
    }

    // Set the callback
    CELLULAR_PORT_TEST_ASSERT(cellularMqttSetMessageIndicationCallback(messageIndicationCallback,
                                                                       &numUnread) == 0);

    cellularPortLog("CELLULAR_MQTT_TEST: publishing %d byte(s) to a topic...\n",
                    CELLULAR_MQTT_PUBLISH_MAX_LENGTH_BYTES);
    startTimeMs = cellularPortGetTickTimeMs();
    gStopTimeMs = cellularPortGetTickTimeMs() + (CELLULAR_CFG_TEST_MQTT_SERVER_TIMEOUT_SECONDS * 1000);
    y = cellularMqttPublish(CELLULAR_MQTT_EXACTLY_ONCE, false, "ubx_test_1",
                            gAllChars, CELLULAR_MQTT_PUBLISH_MAX_LENGTH_BYTES);
    if (y == 0) {
        cellularPortLog("CELLULAR_MQTT_TEST: publish successful after %d ms.\n",
                        (int32_t) (cellularPortGetTickTimeMs() - startTimeMs));
        numPublished++;
    } else {
        cellularPortLog("CELLULAR_MQTT_TEST: publish returned error %d after %d ms, module"
                        " error %d.\n",
                        y, (int32_t) (cellularPortGetTickTimeMs() - startTimeMs),
                        cellularMqttGetLastErrorCode());
        CELLULAR_PORT_TEST_ASSERT(false);
    }

    cellularPortLog("CELLULAR_MQTT_TEST: subscribing to the topic...\n");
    startTimeMs = cellularPortGetTickTimeMs();
    gStopTimeMs = cellularPortGetTickTimeMs() + (CELLULAR_CFG_TEST_MQTT_SERVER_TIMEOUT_SECONDS * 1000);
    y = cellularMqttSubscribe(CELLULAR_MQTT_EXACTLY_ONCE, "ubx_test_1");
    if (y == 0) {
        cellularPortLog("CELLULAR_MQTT_TEST: subscribing successful after %d ms.\n",
                        (int32_t) (cellularPortGetTickTimeMs() - startTimeMs));
    } else {
        cellularPortLog("CELLULAR_MQTT_TEST: subscribe returned error %d after %d ms,"
                        " module error %d.\n",
                        y, (int32_t) (cellularPortGetTickTimeMs() - startTimeMs),
                        cellularMqttGetLastErrorCode());
        CELLULAR_PORT_TEST_ASSERT(false);
    }

    cellularPortLog("CELLULAR_MQTT_TEST: waiting for an unread message indication...\n");
    startTimeMs = cellularPortGetTickTimeMs();
    while ((numUnread == 0) &&
           (cellularPortGetTickTimeMs() < startTimeMs +
                                         (CELLULAR_CFG_TEST_MQTT_SERVER_TIMEOUT_SECONDS * 1000))) {
        cellularPortTaskBlock(1000);
    }

    if (numUnread > 0) {
        cellularPortLog("CELLULAR_MQTT_TEST: %d message(s) unread.\n", numUnread);
    } else {
        cellularPortLog("CELLULAR_MQTT_TEST: no messages unread after %d ms.\n",
                        (int32_t) (cellularPortGetTickTimeMs() - startTimeMs));
    }

    CELLULAR_PORT_TEST_ASSERT(cellularMqttGetUnread() == numUnread);

    // TODO: read message

    // Cancel the subscribe
    gStopTimeMs = cellularPortGetTickTimeMs() + (CELLULAR_CFG_TEST_MQTT_SERVER_TIMEOUT_SECONDS * 1000);
    CELLULAR_PORT_TEST_ASSERT(cellularMqttUnsubscribe("ubx_test_1") == 0);

    // Remove the callback
    CELLULAR_PORT_TEST_ASSERT(cellularMqttSetMessageIndicationCallback(NULL, NULL) == 0);

    cellularPortLog("CELLULAR_MQTT_TEST: disconnecting again...\n");
    gStopTimeMs = cellularPortGetTickTimeMs() + (CELLULAR_CFG_TEST_MQTT_SERVER_TIMEOUT_SECONDS * 1000);
    CELLULAR_PORT_TEST_ASSERT(cellularMqttDisconnect() == 0);
    CELLULAR_PORT_TEST_ASSERT(!cellularMqttIsConnected());

    // Disconnect from the cellular network and tidy up
    networkDisconnect();

    cellularCtrlPowerOff(NULL);
    cellularMqttDeinit();

    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
CELLULAR_PORT_TEST_FUNCTION(void cellularMqttTestCleanUp(),
                            "mqttCleanUp",
                            "mqtt")
{
    cellularMqttDeinit();
    cellularCtrlDeinit();
    CELLULAR_PORT_TEST_ASSERT(cellularPortUartDeinit(CELLULAR_CFG_UART) == 0);
    cellularPortDeinit();
}

// End of file
