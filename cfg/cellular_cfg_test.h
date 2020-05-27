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

#ifndef _CELLULAR_CFG_TEST_H_
#define _CELLULAR_CFG_TEST_H_

/* No #includes allowed here */

/* This header file contains configuration information to be used
 * when testing cellular.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: MISC
 * -------------------------------------------------------------- */

#ifndef CELLULAR_CFG_TEST_FILTER
/** A filter on the basis of which the tests to run
 * will be selected.  If you set a value for this do NOT
 * put quotes around it.  Use, for instance:
 * #define CELLULAR_CFG_TEST_FILTER sock
 */
# define CELLULAR_CFG_TEST_FILTER NULL
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: CELLULAR NETWORK RELATED
 * -------------------------------------------------------------- */

#ifndef CELLULAR_CFG_TEST_RAT
/** The RAT to use during testing.
 */
# define CELLULAR_CFG_TEST_RAT        CELLULAR_CTRL_RAT_CATM1
#endif

#ifndef CELLULAR_CFG_TEST_BANDMASK1
/** The bandmask 1 to use during testing. 0x080092 is bands
 * 2, 5, 8 and 20.
 */
# define CELLULAR_CFG_TEST_BANDMASK1   0x080092ULL
#endif

#ifndef CELLULAR_CFG_TEST_BANDMASK2
/** The bandmask 2 to use during testing.
 */
# define CELLULAR_CFG_TEST_BANDMASK2   0ULL
#endif

#ifndef CELLULAR_CFG_TEST_APN
/** The APN to use when testing connectivity, NULL for unspecified,
 * "" for empty.
 */
# define CELLULAR_CFG_TEST_APN        NULL
#endif

#ifndef CELLULAR_CFG_TEST_USERNAME
/** The username to use when testing connectivity, NULL for unspecified,
 * "" for empty.
 */
# define CELLULAR_CFG_TEST_USERNAME   NULL
#endif

#ifndef CELLULAR_CFG_TEST_PASSWORD
/** The password to use when testing connectivity, NULL for unspecified,
 * "" for empty.
 */
# define CELLULAR_CFG_TEST_PASSWORD   NULL
#endif

#ifndef CELLULAR_CFG_TEST_CONNECT_TIMEOUT_SECONDS
// The time in seconds allowed for a connection to complete.
#define CELLULAR_CFG_TEST_CONNECT_TIMEOUT_SECONDS 240
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: IP RELATED
 * -------------------------------------------------------------- */

#ifndef CELLULAR_CFG_TEST_ECHO_UDP_SERVER_DOMAIN_NAME
/** Echo server to use for UDP sockets testing as a domain name.
 */
# define CELLULAR_CFG_TEST_ECHO_UDP_SERVER_DOMAIN_NAME  "echo.u-blox.com"
#endif

#ifndef CELLULAR_CFG_TEST_ECHO_UDP_SERVER_IP_ADDRESS
/** Echo server to use for UDP sockets testing as an IP address.
 */
# define CELLULAR_CFG_TEST_ECHO_UDP_SERVER_IP_ADDRESS  "195.34.89.241"
#endif

#ifndef CELLULAR_CFG_TEST_ECHO_UDP_SERVER_PORT
/** Port number on the echo server to use for UDP testing.
 */
# define CELLULAR_CFG_TEST_ECHO_UDP_SERVER_PORT  7
#endif

#ifndef CELLULAR_CFG_TEST_ECHO_TCP_SERVER_DOMAIN_NAME
/** Echo server to use for TCP sockets testing as a domain name.
 * (note: the u-blox one adds a prefix to the echoed TCP packets
 * which is undesirable; mbed don't seem to mind us using theirs).
 */
# define CELLULAR_CFG_TEST_ECHO_TCP_SERVER_DOMAIN_NAME  "echo.mbedcloudtesting.com"
#endif

#ifndef CELLULAR_CFG_TEST_ECHO_TCP_SERVER_IP_ADDRESS
/** Echo server to use for TCP sockets testing as an IP address.
 * (note: the u-blox one adds a prefix to the echoed TCP packets
 * which is undesirable; mbed don't seem to mind us using theirs).
 */
# define CELLULAR_CFG_TEST_ECHO_TCP_SERVER_IP_ADDRESS  "52.215.34.155"
#endif

#ifndef CELLULAR_CFG_TEST_ECHO_TCP_SERVER_PORT
/** Port number on the echo server to use for TCP testing.
 */
# define CELLULAR_CFG_TEST_ECHO_TCP_SERVER_PORT  7
#endif

#ifndef CELLULAR_CFG_TEST_LOCAL_PORT
/** Local port number, used when testing binding.
 */
# define CELLULAR_CFG_TEST_LOCAL_PORT 65543
#endif

#ifndef CELLULAR_CFG_TEST_UDP_RETRIES
/** The number of retries to allow when sending
 * data over UDP.
 */
# define CELLULAR_CFG_TEST_UDP_RETRIES 10
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: MQTT RELATED
 * -------------------------------------------------------------- */

#ifndef CELLULAR_CFG_TEST_MQTT_SERVER_TIMEOUT_SECONDS
/** The time to wait for an MQTT operation involving
 * the MQTT server to complete during testing.
 */
# define CELLULAR_CFG_TEST_MQTT_SERVER_TIMEOUT_SECONDS 180
#endif

#ifndef CELLULAR_CFG_TEST_MQTT_SERVER_DOMAIN_NAME
/** Server to use for MTT testing as a domain name.
 */
# define CELLULAR_CFG_TEST_MQTT_SERVER_DOMAIN_NAME  "test.mosquitto.org:1883"
#endif

#ifndef CELLULAR_CFG_TEST_MQTT_SERVER_IP_ADDRESS
/** Server to use for MQTT testing as an IP address.
 */
# define CELLULAR_CFG_TEST_MQTT_SERVER_IP_ADDRESS  "5.196.95.208"
#endif

#ifndef CELLULAR_CFG_TEST_MQTT_SERVER_USERNAME
/** User name to use for the MQTT test server.
 */
# define CELLULAR_CFG_TEST_MQTT_SERVER_USERNAME  NULL
#endif

#ifndef CELLULAR_CFG_TEST_MQTT_SERVER_PASSWORD
/** Password to use for the MQTT test server.
 */
# define CELLULAR_CFG_TEST_MQTT_SERVER_PASSWORD  NULL
#endif


#endif // _CELLULAR_CFG_TEST_H_

// End of file
