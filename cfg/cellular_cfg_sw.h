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

#ifndef _CELLULAR_CFG_SW_H_
#define _CELLULAR_CFG_SW_H_

/* No #includes allowed here */

/* This header file contains software configuration information for
 * cellular.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef CELLULAR_CFG_ENABLE_LOGGING
/** Enable or disable logging from the cellular module.
 */
# define CELLULAR_CFG_ENABLE_LOGGING                 1
#endif

#endif // _CELLULAR_CFG_SW_H_

// End of file
