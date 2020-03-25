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

#ifndef _CELLULAR_PORT_TEST_H_
#define _CELLULAR_PORT_TEST_H_

/* Only bring in #includes specifically related to the test framework */

#include "unity.h"

/** Porting layer for test execution for the Espressif platform.
 * Since test execution is often macro-ised rather than
 * function-calling this header file forms part of the platform
 * test source code rather than pretending to be a generic API.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
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
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

#endif // _CELLULAR_PORT_TEST_H_

// End of file
