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
#include "cellular_cfg_sw.h"
#include "cellular_cfg_hw_platform_specific.h"
#include "cellular_port_debug.h"
#include "cellular_port_clib.h"
#include "cellular_port.h"
#include "cellular_port_os.h"
#include "cellular_port_uart.h"

#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_usart.h"

#include "cmsis_os.h"

#include "cellular_port_private.h"  // Down here 'cos it needs GPIO_TypeDef

/* The code here was written using the really useful information
 * here:
 * *
 * https://stm32f4-discovery.net/2017/07/stm32-tutorial-efficiently-receive-uart-data-using-dma/
 * *
 * This code uses the LL API, as that tutorial does, and sticks
 * to it exactly, hence where the LL API has a series of
 * named functions rather than taking a parameter (e.g.
 * LL_DMA_ClearFlag_HT0(), LL_DMA_ClearFlag_HT1(), etc.)
 * the correct function is accessed through a jump table,
 * making it possible to use it in a parameterised manner
 * again.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// The maximum number of UART HW blocks on an STM32F4.
#define CELLULAR_PORT_MAX_NUM_UARTS 8

// The maximum number of DMA engines on an STM32F4.
#define CELLULAR_PORT_MAX_NUM_DMA_ENGINES 2

// The maximum number of DMA streams on an STM32F4.
#define CELLULAR_PORT_MAX_NUM_DMA_STREAMS 8

// Determine if the given DMA engine is in use
#define CELLULAR_PORT_DMA_ENGINE_IN_USE(x) ((CELLULAR_CFG_UART1_DMA_ENGINE == x) || \
                                            (CELLULAR_CFG_UART2_DMA_ENGINE == x) || \
                                            (CELLULAR_CFG_UART3_DMA_ENGINE == x) || \
                                            (CELLULAR_CFG_UART4_DMA_ENGINE == x) || \
                                            (CELLULAR_CFG_UART5_DMA_ENGINE == x) || \
                                            (CELLULAR_CFG_UART6_DMA_ENGINE == x) || \
                                            (CELLULAR_CFG_UART7_DMA_ENGINE == x) || \
                                            (CELLULAR_CFG_UART8_DMA_ENGINE == x))

// Determine if the given DMA stream is in use
#define CELLULAR_PORT_DMA_STREAM_IN_USE(x) ((CELLULAR_CFG_UART1_DMA_STREAM == x) || \
                                            (CELLULAR_CFG_UART2_DMA_STREAM == x) || \
                                            (CELLULAR_CFG_UART3_DMA_STREAM == x) || \
                                            (CELLULAR_CFG_UART4_DMA_STREAM == x) || \
                                            (CELLULAR_CFG_UART5_DMA_STREAM == x) || \
                                            (CELLULAR_CFG_UART6_DMA_STREAM == x) || \
                                            (CELLULAR_CFG_UART7_DMA_STREAM == x) || \
                                            (CELLULAR_CFG_UART8_DMA_STREAM == x))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold a UART event.
 */
typedef struct {
    int32_t type;
    size_t size;
} CellularPortUartEventData_t;

/** Structure of the constant data per UART.
 */
typedef struct CellularPortUartConstData_t {
    USART_TypeDef *pReg;
    int32_t dmaEngine;
    int32_t dmaStream;
    int32_t dmaChannel;
    IRQn_Type irq;
} CellularPortUartConstData_t;

/** Structure of the data per UART.
 */
typedef struct CellularPortUartData_t {
    int32_t number;
    const CellularPortUartConstData_t * pConstData;
    CellularPortMutexHandle_t mutex;
    CellularPortQueueHandle_t queue;
    char *pRxBufferStart;
    char *pRxBufferRead;
    volatile char *pRxBufferWrite;
    bool userNeedsNotify; //!< set this if toRead has hit zero and
                          // hence the user would like a notification
                          // when new data arrives.
    struct CellularPortUartData_t *pNext;
} CellularPortUartData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Root of the UART linked list.
static CellularPortUartData_t *gpUartDataHead = NULL;

// Get the bus enable function for the given UART/USART.
static const void (*gLlApbClkEnable[])(uint32_t) = {0, // This to avoid having to -1 all the time
                                                    LL_APB2_GRP1_EnableClock,
                                                    LL_APB1_GRP1_EnableClock,
                                                    LL_APB1_GRP1_EnableClock,
                                                    LL_APB1_GRP1_EnableClock,
                                                    LL_APB1_GRP1_EnableClock,
                                                    LL_APB2_GRP1_EnableClock,
                                                    LL_APB1_GRP1_EnableClock,
                                                    LL_APB1_GRP1_EnableClock};

// Get the LL driver peripheral number for a given UART/USART.
static const int32_t gLlApbGrpPeriphUart[] = {0, // This to avoid having to -1 all the time
                                              LL_APB2_GRP1_PERIPH_USART1,
                                              LL_APB1_GRP1_PERIPH_USART2,
                                              LL_APB1_GRP1_PERIPH_USART3,
                                              LL_APB1_GRP1_PERIPH_UART4,
                                              LL_APB1_GRP1_PERIPH_UART5,
                                              LL_APB2_GRP1_PERIPH_USART6,
                                              LL_APB1_GRP1_PERIPH_UART7,
                                              LL_APB1_GRP1_PERIPH_UART8};

// Get the LL driver peripheral number for a given DMA engine.
static const int32_t gLlApbGrpPeriphDma[] = {0, // This to avoid having to -1 all the time
                                             LL_AHB1_GRP1_PERIPH_DMA1,
                                             LL_AHB1_GRP1_PERIPH_DMA2};

// Get the LL driver peripheral number for a given GPIO port.
static const int32_t gLlApbGrpPeriphGpioPort[] = {LL_AHB1_GRP1_PERIPH_GPIOA,
                                                  LL_AHB1_GRP1_PERIPH_GPIOB,
                                                  LL_AHB1_GRP1_PERIPH_GPIOC,
                                                  LL_AHB1_GRP1_PERIPH_GPIOD,
                                                  LL_AHB1_GRP1_PERIPH_GPIOE,
                                                  LL_AHB1_GRP1_PERIPH_GPIOF,
                                                  LL_AHB1_GRP1_PERIPH_GPIOG,
                                                  LL_AHB1_GRP1_PERIPH_GPIOH,
                                                  LL_AHB1_GRP1_PERIPH_GPIOI,
                                                  LL_AHB1_GRP1_PERIPH_GPIOJ,
                                                  LL_AHB1_GRP1_PERIPH_GPIOK};

// Get the DMA base address for a given DMA engine
static DMA_TypeDef * const gpDmaReg[] =  {0, // This to avoid having to -1 all the time
                                          DMA1,
                                          DMA2};

// Get the alternate function required on a GPIO line for a given UART.
// Note: which function a GPIO line actually performs on that UART is
// hard coded in the chip; for instance see table 12 of the STM32F437 data sheet.
static const int32_t gGpioAf[] = {0, // This to avoid having to -1 all the time
                                  LL_GPIO_AF_7,  /* UART 1 */
                                  LL_GPIO_AF_7,  /* UART 2 */
                                  LL_GPIO_AF_7,  /* UART 3 */
                                  LL_GPIO_AF_8,  /* UART 4 */
                                  LL_GPIO_AF_8,  /* UART 5 */
                                  LL_GPIO_AF_8,  /* UART 6 */
                                  LL_GPIO_AF_8,  /* UART 7 */
                                  LL_GPIO_AF_8}; /* UART 8 */

// Table of stream IRQn for DMA1
static const IRQn_Type gDma1StreamIrq[] = {DMA1_Stream0_IRQn,
                                           DMA1_Stream1_IRQn,
                                           DMA1_Stream2_IRQn,
                                           DMA1_Stream3_IRQn,
                                           DMA1_Stream4_IRQn,
                                           DMA1_Stream5_IRQn,
                                           DMA1_Stream6_IRQn,
                                           DMA1_Stream7_IRQn};

// Table of stream IRQn for DMA2
static const IRQn_Type gDma2StreamIrq[] = {DMA2_Stream0_IRQn,
                                           DMA2_Stream1_IRQn,
                                           DMA2_Stream2_IRQn,
                                           DMA2_Stream3_IRQn,
                                           DMA2_Stream4_IRQn,
                                           DMA2_Stream5_IRQn,
                                           DMA2_Stream6_IRQn,
                                           DMA2_Stream7_IRQn};

// Table of DMAx_Stream_IRQn
static const IRQn_Type *gpDmaStreamIrq[] = {NULL, // This to avoid having to -1 all the time
                                            gDma1StreamIrq,
                                            gDma2StreamIrq};

// Table of functions LL_DMA_ClearFlag_HTx(DMA_TypeDef *DMAx) for each stream.
static const void (*gpLlDmaClearFlagHt[]) (DMA_TypeDef *)  = {LL_DMA_ClearFlag_HT0,
                                                              LL_DMA_ClearFlag_HT1,
                                                              LL_DMA_ClearFlag_HT2,
                                                              LL_DMA_ClearFlag_HT3,
                                                              LL_DMA_ClearFlag_HT4,
                                                              LL_DMA_ClearFlag_HT5,
                                                              LL_DMA_ClearFlag_HT6,
                                                              LL_DMA_ClearFlag_HT7};

// Table of functions LL_DMA_ClearFlag_TCx(DMA_TypeDef *DMAx) for each stream.
static const void (*gpLlDmaClearFlagTc[]) (DMA_TypeDef *)  = {LL_DMA_ClearFlag_TC0,
                                                              LL_DMA_ClearFlag_TC1,
                                                              LL_DMA_ClearFlag_TC2,
                                                              LL_DMA_ClearFlag_TC3,
                                                              LL_DMA_ClearFlag_TC4,
                                                              LL_DMA_ClearFlag_TC5,
                                                              LL_DMA_ClearFlag_TC6,
                                                              LL_DMA_ClearFlag_TC7};

// Table of functions LL_DMA_ClearFlag_TEx(DMA_TypeDef *DMAx) for each stream.
static const void (*gpLlDmaClearFlagTe[]) (DMA_TypeDef *)  = {LL_DMA_ClearFlag_TE0,
                                                              LL_DMA_ClearFlag_TE1,
                                                              LL_DMA_ClearFlag_TE2,
                                                              LL_DMA_ClearFlag_TE3,
                                                              LL_DMA_ClearFlag_TE4,
                                                              LL_DMA_ClearFlag_TE5,
                                                              LL_DMA_ClearFlag_TE6,
                                                              LL_DMA_ClearFlag_TE7};

// Table of functions LL_DMA_ClearFlag_DMEx(DMA_TypeDef *DMAx) for each stream.
static const void (*gpLlDmaClearFlagDme[]) (DMA_TypeDef *)  = {LL_DMA_ClearFlag_DME0,
                                                               LL_DMA_ClearFlag_DME1,
                                                               LL_DMA_ClearFlag_DME2,
                                                               LL_DMA_ClearFlag_DME3,
                                                               LL_DMA_ClearFlag_DME4,
                                                               LL_DMA_ClearFlag_DME5,
                                                               LL_DMA_ClearFlag_DME6,
                                                               LL_DMA_ClearFlag_DME7};

// Table of functions LL_DMA_ClearFlag_FEx(DMA_TypeDef *DMAx) for each stream.
static const void (*gpLlDmaClearFlagFe[]) (DMA_TypeDef *)  = {LL_DMA_ClearFlag_FE0,
                                                              LL_DMA_ClearFlag_FE1,
                                                              LL_DMA_ClearFlag_FE2,
                                                              LL_DMA_ClearFlag_FE3,
                                                              LL_DMA_ClearFlag_FE4,
                                                              LL_DMA_ClearFlag_FE5,
                                                              LL_DMA_ClearFlag_FE6,
                                                              LL_DMA_ClearFlag_FE7};

// Table of the constant data per UART.
static const CellularPortUartConstData_t gUartCfg[] = {{}, // This to avoid having to -1 all the time
                                                       {USART1,
                                                        CELLULAR_CFG_UART1_DMA_ENGINE,
                                                        CELLULAR_CFG_UART1_DMA_STREAM,
                                                        CELLULAR_CFG_UART1_DMA_CHANNEL,
                                                        USART1_IRQn},
                                                       {USART2,
                                                        CELLULAR_CFG_UART2_DMA_ENGINE,
                                                        CELLULAR_CFG_UART2_DMA_STREAM,
                                                        CELLULAR_CFG_UART2_DMA_CHANNEL,
                                                        USART2_IRQn},
                                                       {USART3,
                                                        CELLULAR_CFG_UART3_DMA_ENGINE,
                                                        CELLULAR_CFG_UART3_DMA_STREAM,
                                                        CELLULAR_CFG_UART3_DMA_CHANNEL,
                                                        USART3_IRQn},
                                                       {UART4,
                                                        CELLULAR_CFG_UART4_DMA_ENGINE,
                                                        CELLULAR_CFG_UART4_DMA_STREAM,
                                                        CELLULAR_CFG_UART4_DMA_CHANNEL,
                                                        UART4_IRQn},
                                                       {UART5,
                                                        CELLULAR_CFG_UART5_DMA_ENGINE,
                                                        CELLULAR_CFG_UART5_DMA_STREAM,
                                                        CELLULAR_CFG_UART5_DMA_CHANNEL,
                                                        UART5_IRQn},
                                                       {USART6,
                                                        CELLULAR_CFG_UART6_DMA_ENGINE,
                                                        CELLULAR_CFG_UART6_DMA_STREAM,
                                                        CELLULAR_CFG_UART6_DMA_CHANNEL,
                                                        USART6_IRQn},
                                                       {UART7,
                                                        CELLULAR_CFG_UART7_DMA_ENGINE,
                                                        CELLULAR_CFG_UART7_DMA_STREAM,
                                                        CELLULAR_CFG_UART7_DMA_CHANNEL,
                                                        UART7_IRQn},
                                                       {UART8,
                                                        CELLULAR_CFG_UART8_DMA_ENGINE,
                                                        CELLULAR_CFG_UART8_DMA_STREAM,
                                                        CELLULAR_CFG_UART8_DMA_CHANNEL,
                                                        UART8_IRQn}};

// Table to make it possible for UART interrupts to get to the UART data
// without having to trawl through a list.
static CellularPortUartData_t *gpUart[CELLULAR_PORT_MAX_NUM_UARTS + 1] = {NULL};

// Table to make it possible for a DMA interrupt to
// get to the UART data.  +1 is for the usual reason
static CellularPortUartData_t *gpDmaUart[(CELLULAR_PORT_MAX_NUM_DMA_ENGINES + 1) *
                                          CELLULAR_PORT_MAX_NUM_DMA_ENGINES] = {NULL};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Add a UART data structure to the list.
// The required memory is malloc()ed.
CellularPortUartData_t *pAddUart(int32_t uart,
                                 CellularPortUartData_t *pUartData)
{
    CellularPortUartData_t **ppUartData = &gpUartDataHead;

    // Go to the end of the list
    while (*ppUartData != NULL) {
        ppUartData = &((*ppUartData)->pNext);
    }

    // Malloc memory for the item
    *ppUartData = (CellularPortUartData_t *) pCellularPort_malloc(sizeof(CellularPortUartData_t));
    if (*ppUartData != NULL) {
        // Copy the data in
        pCellularPort_memcpy(*ppUartData, pUartData, sizeof(CellularPortUartData_t));
        (*ppUartData)->pNext = NULL;
        // Set the UART table up to point to it
        // so that the UART interrupt can find it
        gpUart[uart] = *ppUartData;
        // And set the other table up so that the
        // DMA interrupt can find the UART data as well
        gpDmaUart[pUartData->pConstData->dmaEngine +
                  pUartData->pConstData->dmaStream] = *ppUartData;
    }

    return *ppUartData;
}

// Find the UART data structure for a given UART.
CellularPortUartData_t *pGetUart(int32_t uart)
{
    CellularPortUartData_t *pUartData = gpUartDataHead;
    bool found = false;

    while (!found && (pUartData != NULL)) {
        if (pUartData->number == uart) {
            found = true;
        } else {
            pUartData = pUartData->pNext;
        }
    }

    return pUartData;
}

// Remove a UART from the list.
// The memory occupied is free()ed.
bool removeUart(int32_t uart)
{
    CellularPortUartData_t **ppUartData = &gpUartDataHead;
    CellularPortUartData_t *pTmp = NULL;
    bool found = false;

    // Find it in the list
    while (!found && (*ppUartData != NULL)) {
        if ((*ppUartData)->number == uart) {
            found = true;
        } else {
            pTmp = *ppUartData;
            ppUartData = &((*ppUartData)->pNext);
        }
    }

    // Remove the item
    if (*ppUartData != NULL) {
        // Move the next pointer of the previous
        // entry on
        if (pTmp != NULL) {
            pTmp->pNext = (*ppUartData)->pNext;
        }
        // NULL the entries in the two tables
        gpUart[uart] = NULL;
        gpDmaUart[(*ppUartData)->pConstData->dmaEngine +
                  (*ppUartData)->pConstData->dmaStream] = NULL;
        // Free memory and NULL the pointer
        cellularPort_free(*ppUartData);
        *ppUartData = NULL;
    }

    return found;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: INTERRUPT HANDLERS
 * -------------------------------------------------------------- */

// DMA interrupt handler
void dmaIrqHandler(uint32_t dmaEngine, uint32_t dmaStream)
{
    DMA_TypeDef * const pDmaReg = gpDmaReg[dmaEngine];
    CellularPortUartData_t *pUartData = NULL;

    // Check half-transfer complete interrupt
    if (LL_DMA_IsEnabledIT_HT(pDmaReg, dmaStream) &&
        LL_DMA_IsActiveFlag_HT1(pDmaReg)) {
        pUartData = gpDmaUart[dmaEngine + dmaStream];
        // Clear the flag
        gpLlDmaClearFlagHt[dmaStream](pDmaReg);
    }

    // Check transfer complete interrupt
    if (LL_DMA_IsEnabledIT_TC(pDmaReg, dmaStream) &&
        LL_DMA_IsActiveFlag_TC1(pDmaReg)) {
        pUartData = gpDmaUart[dmaEngine + dmaStream];
        // Clear the flag
        gpLlDmaClearFlagTc[dmaStream](pDmaReg);
    }

    if (pUartData != NULL) {
        CellularPortUartEventData_t uartEvent;

        // Stuff has arrived: how much?
        uartEvent.type = 0;
        uartEvent.size = CELLULAR_PORT_UART_RX_BUFFER_SIZE -
                         LL_DMA_GetDataLength(pDmaReg, dmaStream);
        // Move the write pointer on
        pUartData->pRxBufferWrite += uartEvent.size;
        if (pUartData->pRxBufferWrite >= pUartData->pRxBufferStart +
                                         CELLULAR_PORT_UART_RX_BUFFER_SIZE) {
            pUartData->pRxBufferWrite = pUartData->pRxBufferStart;
        }

        // If the user wanted to know then send a message
        if (pUartData->userNeedsNotify) {
            BaseType_t yield = false;

            xQueueSendFromISR((QueueHandle_t) (pUartData->queue),
                              &uartEvent, &yield);
            pUartData->userNeedsNotify = false;

            // Required for correct FreeRTOS operation
            portEND_SWITCHING_ISR(yield);
        }
    }

}

// UART interrupt handler
void uartIrqHandler(CellularPortUartData_t *pUartData)
{
    const CellularPortUartConstData_t *pUartCfg = pUartData->pConstData;
    USART_TypeDef * const pUartReg = pUartCfg->pReg;

    // Check for IDLE line interrupt
    if (LL_USART_IsEnabledIT_IDLE(pUartReg) &&
        LL_USART_IsActiveFlag_IDLE(pUartReg)) {
        CellularPortUartEventData_t uartEvent;

        // Clear flag
        LL_USART_ClearFlag_IDLE(pUartReg);
        uartEvent.type = 0;
        uartEvent.size = CELLULAR_PORT_UART_RX_BUFFER_SIZE -
                         LL_DMA_GetDataLength(gpDmaReg[pUartCfg->dmaEngine],
                                              pUartCfg->dmaStream);
        // Move the write pointer on
        pUartData->pRxBufferWrite += uartEvent.size;
        if (pUartData->pRxBufferWrite >= pUartData->pRxBufferStart +
                                         CELLULAR_PORT_UART_RX_BUFFER_SIZE) {
            pUartData->pRxBufferWrite = pUartData->pRxBufferStart;
        }

        // If there is new data and the user wanted to know
        // then send a message to let them know.
        if ((uartEvent.size > 0) && pUartData->userNeedsNotify) {
            BaseType_t yield = false;

            xQueueSendFromISR((QueueHandle_t) (pUartData->queue),
                              &uartEvent, &yield);
            pUartData->userNeedsNotify = false;

            // Required for correct FreeRTOS operation
            portEND_SWITCHING_ISR(yield);
        }
    }
}

#if CELLULAR_CFG_UART1_AVAILABLE
// USART 1 interrupt handler.
void USART1_IRQHandler()
{
    if (gpUart[1] != NULL) {
        uartIrqHandler(gpUart[1]);
    }
}
#endif

#if CELLULAR_CFG_UART2_AVAILABLE
// USART 2 interrupt handler.
void USART2_IRQHandler()
{
    if (gpUart[2] != NULL) {
        uartIrqHandler(gpUart[2]);
    }
}
#endif

#if CELLULAR_CFG_UART3_AVAILABLE
// USART 3 interrupt handler.
void USART3_IRQHandler()
{
    if (gpUart[3] != NULL) {
        uartIrqHandler(gpUart[3]);
    }
}
#endif

#if CELLULAR_CFG_UART4_AVAILABLE
// UART 4 interrupt handler.
void UART4_IRQHandler()
{
    if (gpUart[4] != NULL) {
        uartIrqHandler(gpUart[4]);
    }
}
#endif

#if CELLULAR_CFG_UART5_AVAILABLE
// UART 5 interrupt handler.
void UART5_IRQHandler()
{
    if (gpUart[5] != NULL) {
        uartIrqHandler(gpUart[5]);
    }
}
#endif

#if CELLULAR_CFG_UART6_AVAILABLE
// USART 6 interrupt handler.
void USART6_IRQHandler()
{
    if (gpUart[6] != NULL) {
        uartIrqHandler(gpUart[6]);
    }
}
#endif

#if CELLULAR_CFG_UART7_AVAILABLE
// UART 7 interrupt handler.
void UART7_IRQHandler()
{
    if (gpUart[7] != NULL) {
        uartIrqHandler(gpUart[7]);
    }
}
#endif

#if CELLULAR_CFG_UART8_AVAILABLE
// UART 8 interrupt handler.
void UART8_IRQHandler()
{
    if (gpUart[8] != NULL) {
        uartIrqHandler(gpUart[8]);
    }
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(1) && CELLULAR_PORT_DMA_STREAM_IN_USE(0)
void DMA1_Stream0_IRQHandler()
{
    dmaIrqHandler(1, 0);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(1) && CELLULAR_PORT_DMA_STREAM_IN_USE(1)
void DMA1_Stream1_IRQHandler()
{
    dmaIrqHandler(1, 1);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(1) && CELLULAR_PORT_DMA_STREAM_IN_USE(2)
void DMA1_Stream2_IRQHandler()
{
    dmaIrqHandler(1, 2);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(1) && CELLULAR_PORT_DMA_STREAM_IN_USE(3)
void DMA1_Stream3_IRQHandler()
{
    dmaIrqHandler(1, 3);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(1) && CELLULAR_PORT_DMA_STREAM_IN_USE(4)
void DMA1_Stream4_IRQHandler()
{
    dmaIrqHandler(1, 4);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(1) && CELLULAR_PORT_DMA_STREAM_IN_USE(5)
void DMA1_Stream5_IRQHandler()
{
    dmaIrqHandler(1, 5);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(1) && CELLULAR_PORT_DMA_STREAM_IN_USE(6)
void DMA1_Stream6_IRQHandler()
{
    dmaIrqHandler(1, 6);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(1) && CELLULAR_PORT_DMA_STREAM_IN_USE(7)
void DMA1_Stream7_IRQHandler()
{
    dmaIrqHandler(1, 7);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(2) && CELLULAR_PORT_DMA_STREAM_IN_USE(0)
void DMA2_Stream0_IRQHandler()
{
    dmaIrqHandler(2, 0);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(2) && CELLULAR_PORT_DMA_STREAM_IN_USE(1)
void DMA2_Stream1_IRQHandler()
{
    dmaIrqHandler(2, 1);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(2) && CELLULAR_PORT_DMA_STREAM_IN_USE(2)
void DMA2_Stream2_IRQHandler()
{
    dmaIrqHandler(2, 2);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(2) && CELLULAR_PORT_DMA_STREAM_IN_USE(3)
void DMA2_Stream3_IRQHandler()
{
    dmaIrqHandler(2, 3);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(2) && CELLULAR_PORT_DMA_STREAM_IN_USE(4)
void DMA2_Stream4_IRQHandler()
{
    dmaIrqHandler(2, 4);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(2) && CELLULAR_PORT_DMA_STREAM_IN_USE(5)
void DMA2_Stream5_IRQHandler()
{
    dmaIrqHandler(2, 5);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(2) && CELLULAR_PORT_DMA_STREAM_IN_USE(6)
void DMA2_Stream6_IRQHandler()
{
    dmaIrqHandler(2, 6);
}
#endif

#if CELLULAR_PORT_DMA_ENGINE_IN_USE(2) && CELLULAR_PORT_DMA_STREAM_IN_USE(7)
void DMA2_Stream7_IRQHandler()
{
    dmaIrqHandler(2, 7);
}
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise a UART.
int32_t cellularPortUartInit(int32_t pinTx, int32_t pinRx,
                             int32_t pinCts, int32_t pinRts,
                             int32_t baudRate,
                             size_t rtsThreshold,
                             int32_t uart,
                             CellularPortQueueHandle_t *pUartQueue)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;
    ErrorStatus platformError;
    CellularPortUartData_t uartData = {0};
    LL_USART_InitTypeDef usartInitStruct = {0};
    LL_GPIO_InitTypeDef gpioInitStruct = {0};

    // TODO: use this
    (void) rtsThreshold;

    if ((pUartQueue != NULL) && (pinRx >= 0) && (pinTx >= 0) &&
        (uart > 0) && (uart <= CELLULAR_PORT_MAX_NUM_UARTS) &&
        (baudRate >= 0)) {
        errorCode = CELLULAR_PORT_SUCCESS;
        if (pGetUart(uart) == NULL) {
            // Create the mutex
            errorCode = cellularPortMutexCreate(&(uartData.mutex));
            if (errorCode == 0) {

                CELLULAR_PORT_MUTEX_LOCK(uartData.mutex);

                errorCode = CELLULAR_PORT_OUT_OF_MEMORY;

                uartData.number = uart;
                // Malloc memory for the read buffer
                uartData.pRxBufferStart = (char *) pCellularPort_malloc(CELLULAR_PORT_UART_RX_BUFFER_SIZE);
                if (uartData.pRxBufferStart != NULL) {
                    uartData.pConstData = &(gUartCfg[uart]);
                    uartData.pRxBufferRead = uartData.pRxBufferStart;
                    uartData.pRxBufferWrite = uartData.pRxBufferStart;
                    uartData.userNeedsNotify = true;

                    // Create the queue
                    errorCode = cellularPortQueueCreate(CELLULAR_PORT_UART_EVENT_QUEUE_SIZE,
                                                        sizeof(CellularPortUartEventData_t),
                                                        pUartQueue);
                    if (errorCode == 0) {
                        uartData.queue = *pUartQueue;

                        // Now do the platform stuff
                        errorCode = CELLULAR_PORT_PLATFORM_ERROR;

                        // Enable UART clock
                        gLlApbClkEnable[uart](gLlApbGrpPeriphUart[uart]);

                        // Enable DMA clock (all DMAs are on bus 1)
                        LL_AHB1_GRP1_EnableClock(gLlApbGrpPeriphDma[gUartCfg[uart].dmaEngine]);

                        // Enable GPIO clocks (all on bus 1): note, using the LL driver rather
                        // than our driver or the HAL driver here partly because the
                        // example code does that and also because lower down
                        // we need to enable the UART alternate function for
                        // these pins.
                        LL_AHB1_GRP1_EnableClock(gLlApbGrpPeriphGpioPort[CELLULAR_PORT_STM32F4_GPIO_PORT(pinTx)]);
                        LL_AHB1_GRP1_EnableClock(gLlApbGrpPeriphGpioPort[CELLULAR_PORT_STM32F4_GPIO_PORT(pinRx)]);
                        if (pinCts >= 0) {
                            LL_AHB1_GRP1_EnableClock(gLlApbGrpPeriphGpioPort[CELLULAR_PORT_STM32F4_GPIO_PORT(pinCts)]);
                        }
                        if (pinRts >= 0) {
                            LL_AHB1_GRP1_EnableClock(gLlApbGrpPeriphGpioPort[CELLULAR_PORT_STM32F4_GPIO_PORT(pinRts)]);
                        }

                        //  Configure the GPIOs, start with Tx
                        gpioInitStruct.Pin = CELLULAR_PORT_STM32F4_GPIO_PIN(pinTx);
                        gpioInitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
                        gpioInitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
                        // Output type doesn't matter, it is overridden by
                        // the alternate function
                        gpioInitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
                        gpioInitStruct.Pull = LL_GPIO_PULL_UP;
                        gpioInitStruct.Alternate = gGpioAf[uart];
                        platformError = LL_GPIO_Init(pCellularPortPrivateGpioGetReg(pinTx),
                                                     &gpioInitStruct);

                        //  Configure Rx
                        if (platformError == SUCCESS) {
                            gpioInitStruct.Pin = CELLULAR_PORT_STM32F4_GPIO_PIN(pinRx);
                            platformError = LL_GPIO_Init(pCellularPortPrivateGpioGetReg(pinRx),
                                                         &gpioInitStruct);
                        }

                        //  Configure RTS if present
                        if ((pinRts >= 0) && (platformError == SUCCESS)) {
                            gpioInitStruct.Pin = CELLULAR_PORT_STM32F4_GPIO_PIN(pinRts);
                            platformError = LL_GPIO_Init(pCellularPortPrivateGpioGetReg(pinRts),
                                                         &gpioInitStruct);
                        }

                        //  Configure CTS if present
                        if ((pinCts >= 0) && (platformError == SUCCESS)) {
                            gpioInitStruct.Pin = CELLULAR_PORT_STM32F4_GPIO_PIN(pinCts);
                            platformError = LL_GPIO_Init(pCellularPortPrivateGpioGetReg(pinCts),
                                                         &gpioInitStruct);
                        }

                        // Configure DMA
                        if (platformError == SUCCESS) {
                            // Channel CELLULAR_CFG_DMA_CHANNEL on our DMA/Stream
                            LL_DMA_SetChannelSelection(gpDmaReg[gUartCfg[uart].dmaEngine],
                                                       gUartCfg[uart].dmaStream,
                                                       gUartCfg[uart].dmaChannel);
                            // Towards RAM
                            LL_DMA_SetDataTransferDirection(gpDmaReg[gUartCfg[uart].dmaEngine],
                                                            gUartCfg[uart].dmaStream,
                                                            LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
                            // Low priority
                            LL_DMA_SetStreamPriorityLevel(gpDmaReg[gUartCfg[uart].dmaEngine],
                                                          gUartCfg[uart].dmaStream,
                                                          LL_DMA_PRIORITY_LOW);
                            // Circular
                            LL_DMA_SetMode(gpDmaReg[gUartCfg[uart].dmaEngine],
                                           gUartCfg[uart].dmaStream,
                                           LL_DMA_MODE_CIRCULAR);
                            // Byte-wise transfers
                            LL_DMA_SetPeriphIncMode(gpDmaReg[gUartCfg[uart].dmaEngine],
                                                    gUartCfg[uart].dmaStream,
                                                    LL_DMA_PERIPH_NOINCREMENT);
                            LL_DMA_SetMemoryIncMode(gpDmaReg[gUartCfg[uart].dmaEngine],
                                                    gUartCfg[uart].dmaStream,
                                                    LL_DMA_MEMORY_INCREMENT);
                            LL_DMA_SetPeriphSize(gpDmaReg[gUartCfg[uart].dmaEngine],
                                                 gUartCfg[uart].dmaStream,
                                                 LL_DMA_PDATAALIGN_BYTE);
                            LL_DMA_SetMemorySize(gpDmaReg[gUartCfg[uart].dmaEngine],
                                                 gUartCfg[uart].dmaStream,
                                                 LL_DMA_MDATAALIGN_BYTE);
                            // Not FIFO mode, whatever that is
                            LL_DMA_DisableFifoMode(gpDmaReg[gUartCfg[uart].dmaEngine],
                                                   gUartCfg[uart].dmaStream);

                            // Attach the DMA to the UART at one end
                            LL_DMA_SetPeriphAddress(gpDmaReg[gUartCfg[uart].dmaEngine],
                                                    gUartCfg[uart].dmaStream,
                                                    (uint32_t) (gUartCfg[uart].pReg->DR));

                            // ...and to the RAM buffer at the other end
                            LL_DMA_SetMemoryAddress(gpDmaReg[gUartCfg[uart].dmaEngine],
                                                    gUartCfg[uart].dmaStream,
                                                    (uint32_t) (uartData.pRxBufferStart));
                            LL_DMA_SetDataLength(gpDmaReg[gUartCfg[uart].dmaEngine],
                                                 gUartCfg[uart].dmaStream,
                                                 CELLULAR_PORT_UART_RX_BUFFER_SIZE);

                            // Set DMA priority
                            NVIC_SetPriority(gpDmaStreamIrq[gUartCfg[uart].dmaEngine][gUartCfg[uart].dmaStream],
                                             NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));

                            // Clear all the DMA flags and the DMA pending IRQ from any previous
                            // session first, or an unexpected interrupt may result
                            gpLlDmaClearFlagHt[gUartCfg[uart].dmaStream](gpDmaReg[gUartCfg[uart].dmaEngine]);
                            gpLlDmaClearFlagTc[gUartCfg[uart].dmaStream](gpDmaReg[gUartCfg[uart].dmaEngine]);
                            gpLlDmaClearFlagTe[gUartCfg[uart].dmaStream](gpDmaReg[gUartCfg[uart].dmaEngine]);
                            gpLlDmaClearFlagDme[gUartCfg[uart].dmaStream](gpDmaReg[gUartCfg[uart].dmaEngine]);
                            gpLlDmaClearFlagFe[gUartCfg[uart].dmaStream](gpDmaReg[gUartCfg[uart].dmaEngine]);
                            NVIC_ClearPendingIRQ(gpDmaStreamIrq[gUartCfg[uart].dmaEngine][gUartCfg[uart].dmaStream]);

                            // Enable half full and transmit complete interrupts
                            LL_DMA_EnableIT_HT(gpDmaReg[gUartCfg[uart].dmaEngine],
                                               gUartCfg[uart].dmaStream);
                            LL_DMA_EnableIT_TC(gpDmaReg[gUartCfg[uart].dmaEngine],
                                               gUartCfg[uart].dmaStream);

                            // Go!
                            NVIC_EnableIRQ(gpDmaStreamIrq[gUartCfg[uart].dmaEngine][gUartCfg[uart].dmaStream]);

                            // Initialise the USART
                            usartInitStruct.BaudRate = baudRate;
                            usartInitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
                            usartInitStruct.StopBits = LL_USART_STOPBITS_1;
                            usartInitStruct.Parity = LL_USART_PARITY_NONE;
                            usartInitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
                            // TODO: need to connect flow control to DMA?
                            usartInitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
                            if ((pinRts >= 0) && (pinCts >= 0)) {
                                usartInitStruct.HardwareFlowControl = LL_USART_HWCONTROL_RTS_CTS;
                            } else {
                                if (pinRts >= 0) {
                                    usartInitStruct.HardwareFlowControl = LL_USART_HWCONTROL_RTS;
                                } else {
                                    usartInitStruct.HardwareFlowControl = LL_USART_HWCONTROL_CTS;
                                }
                            }
                            usartInitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
                            platformError = LL_USART_Init(gUartCfg[uart].pReg, &usartInitStruct);
                        }

                        // STILL more stuff to configure...
                        if (platformError == SUCCESS) {
                            LL_USART_ConfigAsyncMode(gUartCfg[uart].pReg);
                            LL_USART_EnableDMAReq_RX(gUartCfg[uart].pReg);
                            LL_USART_EnableIT_IDLE(gUartCfg[uart].pReg);

                            // Enable USART interrupt
                            NVIC_SetPriority(gUartCfg[uart].irq,
                                             NVIC_EncodePriority(NVIC_GetPriorityGrouping(),
                                                                 5, 1));
                            NVIC_ClearPendingIRQ(gUartCfg[uart].irq);
                            NVIC_EnableIRQ(gUartCfg[uart].irq);

                            // Enable USART and DMA
                            LL_DMA_EnableStream(gpDmaReg[gUartCfg[uart].dmaEngine],
                                                gUartCfg[uart].dmaStream);
                            LL_USART_Enable(gUartCfg[uart].pReg);
                        }

                        // Finally, add the UART to the list
                        if (platformError == SUCCESS) {
                            errorCode = CELLULAR_PORT_OUT_OF_MEMORY;
                            if (pAddUart(uart, &uartData) != NULL) {
                                errorCode = CELLULAR_PORT_SUCCESS;
                            }
                        }
                    }
                }

                CELLULAR_PORT_MUTEX_UNLOCK(uartData.mutex);

                // If we failed, clean up
                if (errorCode != 0) {
                    cellularPortMutexDelete(uartData.mutex);
                    cellularPort_free(uartData.pRxBufferStart);
                }
            }
        }
    }

    return (int32_t) errorCode;
}

// Shutdown a UART.
int32_t cellularPortUartDeinit(int32_t uart)
{
    CellularPortErrorCode_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;
    CellularPortUartData_t *pUartData;

    if ((uart > 0) && (uart <= CELLULAR_PORT_MAX_NUM_UARTS)) {
        pUartData = pGetUart(uart);
        errorCode = CELLULAR_PORT_SUCCESS;
        if (pUartData != NULL) {

            errorCode = CELLULAR_PORT_PLATFORM_ERROR;
            // No need to lock the mutex, we need to delete it
            // and we're not allowed to delete a locked mutex.
            // The caller needs to make sure that no read/write
            // is in progress when this function is called.

            // TODO check this

            // Disable DMA interrupt
            NVIC_DisableIRQ(gpDmaStreamIrq[gUartCfg[uart].dmaEngine][gUartCfg[uart].dmaStream]);

            // Disable USART interrupt
            NVIC_DisableIRQ(gUartCfg[uart].irq);
            // Disable DMA and USART
            LL_DMA_DisableStream(gpDmaReg[gUartCfg[uart].dmaEngine],
                                          gUartCfg[uart].dmaStream);
            LL_USART_Disable(gUartCfg[uart].pReg);
            LL_USART_DeInit(gUartCfg[uart].pReg);

            // Delete the queue
            cellularPortQueueDelete(pUartData->queue);
            // Free the buffer
            cellularPort_free(pUartData->pRxBufferStart);
            // Delete the mutex
            cellularPortMutexDelete(pUartData->mutex);
            // And finally remove the UART from the list
            removeUart(uart);
            errorCode = CELLULAR_PORT_SUCCESS;
        }
    }

    return (int32_t) errorCode;
}

// Push a UART event onto the UART event queue.
int32_t cellularPortUartEventSend(const CellularPortQueueHandle_t queueHandle,
                                  int32_t sizeBytes)
{
    int32_t errorCode = CELLULAR_PORT_INVALID_PARAMETER;
    CellularPortUartEventData_t uartEvent;

    if (queueHandle != NULL) {
        uartEvent.type = -1;
        uartEvent.size = 0;
        if (sizeBytes >= 0) {
            uartEvent.type = 0;
            uartEvent.size = sizeBytes;
        }
        errorCode = cellularPortQueueSend(queueHandle, (void *) &uartEvent);
    }

    return errorCode;
}

// Receive a UART event, blocking until one turns up.
int32_t cellularPortUartEventReceive(const CellularPortQueueHandle_t queueHandle)
{
    int32_t sizeOrErrorCode = CELLULAR_PORT_INVALID_PARAMETER;
    CellularPortUartEventData_t uartEvent;

    if (queueHandle != NULL) {
        sizeOrErrorCode = CELLULAR_PORT_PLATFORM_ERROR;
        if (cellularPortQueueReceive(queueHandle, &uartEvent) == 0) {
            sizeOrErrorCode = CELLULAR_PORT_UNKNOWN_ERROR;
            if (uartEvent.type >= 0) {
                sizeOrErrorCode = uartEvent.size;
            }
        }
    }

    return sizeOrErrorCode;
}

// Get the number of bytes waiting in the receive buffer.
int32_t cellularPortUartGetReceiveSize(int32_t uart)
{
    CellularPortErrorCode_t sizeOrErrorCode = CELLULAR_PORT_INVALID_PARAMETER;
    CellularPortUartData_t *pUartData = pGetUart(uart);
    const volatile char *pRxBufferWrite;

    if (pUartData != NULL) {

        CELLULAR_PORT_MUTEX_LOCK(pUartData->mutex);

        pRxBufferWrite = pUartData->pRxBufferWrite;
        sizeOrErrorCode = 0;
        if (pUartData->pRxBufferRead < pRxBufferWrite) {
            // Read pointer is behind write, bytes
            // received is simply the difference
            sizeOrErrorCode = pRxBufferWrite - pUartData->pRxBufferRead;
        } else if (pUartData->pRxBufferRead > pRxBufferWrite) {
            // Read pointer is ahead of write, bytes received
            // is up to the end of the buffer then wrap
            // around to the write pointer
            sizeOrErrorCode = (pUartData->pRxBufferStart +
                               CELLULAR_PORT_UART_RX_BUFFER_SIZE -
                               pUartData->pRxBufferRead) +
                              (pRxBufferWrite - pUartData->pRxBufferStart);
        }

        CELLULAR_PORT_MUTEX_UNLOCK(pUartData->mutex);

    }

    return (int32_t) sizeOrErrorCode;
}

// Read from the given UART interface.
int32_t cellularPortUartRead(int32_t uart, char *pBuffer,
                             size_t sizeBytes)
{
    CellularPortErrorCode_t sizeOrErrorCode = CELLULAR_PORT_INVALID_PARAMETER;
    size_t thisSize;
    CellularPortUartData_t *pUartData = pGetUart(uart);
    const volatile char *pRxBufferWrite;

    if (pUartData != NULL) {
        sizeOrErrorCode = CELLULAR_PORT_PLATFORM_ERROR;

        CELLULAR_PORT_MUTEX_LOCK(pUartData->mutex);

        pRxBufferWrite = pUartData->pRxBufferWrite;
        if (pUartData->pRxBufferRead < pRxBufferWrite) {
            // Read pointer is behind write, just take as much
            // of the difference as the user allows
            sizeOrErrorCode = pRxBufferWrite - pUartData->pRxBufferRead;
            if (sizeOrErrorCode > sizeBytes) {
                sizeOrErrorCode = sizeBytes;
            }
            pCellularPort_memcpy(pBuffer, pUartData->pRxBufferRead,
                                 sizeOrErrorCode);
            pUartData->pRxBufferRead += sizeOrErrorCode;
            if (pUartData->pRxBufferRead >= pUartData->pRxBufferStart +
                                            CELLULAR_PORT_UART_RX_BUFFER_SIZE) {
                pUartData->pRxBufferRead = pUartData->pRxBufferStart;
            }
        } else if (pUartData->pRxBufferRead > pRxBufferWrite) {
            // Read pointer is ahead of write, first take up to the
            // end of the buffer as far as the user allows
            thisSize = pUartData->pRxBufferStart +
                       CELLULAR_PORT_UART_RX_BUFFER_SIZE -
                       pUartData->pRxBufferRead;
            if (thisSize > sizeBytes) {
                thisSize = sizeBytes;
            }
            pCellularPort_memcpy(pBuffer, pUartData->pRxBufferRead,
                                 thisSize);
            pBuffer += thisSize;
            sizeBytes -= thisSize;
            sizeOrErrorCode = thisSize;
            // Move the read pointer on, wrapping as necessary
            pUartData->pRxBufferRead += thisSize;
            if (pUartData->pRxBufferRead >= pUartData->pRxBufferStart +
                                            CELLULAR_PORT_UART_RX_BUFFER_SIZE) {
                pUartData->pRxBufferRead = pUartData->pRxBufferStart;
            }
            // If there is still room in the user buffer then take
            // up to the write pointer
            if (sizeBytes > 0) {
                thisSize = pRxBufferWrite - pUartData->pRxBufferRead;
                if (thisSize > sizeBytes) {
                    thisSize = sizeBytes;
                }
                pCellularPort_memcpy(pBuffer, pUartData->pRxBufferRead,
                                     thisSize);
                pBuffer += thisSize;
                sizeBytes -= thisSize;
                sizeOrErrorCode += thisSize;
                // Move the read pointer on
                pUartData->pRxBufferRead += thisSize;
            }
        } else {
            sizeOrErrorCode = 0;
        }

        CELLULAR_PORT_MUTEX_UNLOCK(pUartData->mutex);

    }

    return (int32_t) sizeOrErrorCode;
}

// Write to the given UART interface.
int32_t cellularPortUartWrite(int32_t uart,
                              const char *pBuffer,
                              size_t sizeBytes)
{
    CellularPortErrorCode_t sizeOrErrorCode = CELLULAR_PORT_INVALID_PARAMETER;
    CellularPortUartData_t *pUartData = pGetUart(uart);

    if (pUartData != NULL) {

        CELLULAR_PORT_MUTEX_LOCK(pUartData->mutex);

        // Do the blocking send
        while (sizeBytes > 0) {
            LL_USART_TransmitData8(gUartCfg[uart].pReg, (uint8_t) *pBuffer);
            while (!LL_USART_IsActiveFlag_TXE(gUartCfg[uart].pReg)) {}
            pBuffer++;
            sizeBytes--;
        }
        while (!LL_USART_IsActiveFlag_TC(gUartCfg[uart].pReg)) {}

        CELLULAR_PORT_MUTEX_UNLOCK(pUartData->mutex);

    }

    return (int32_t) sizeOrErrorCode;
}

// Determine if RTS flow control is enabled.
bool cellularPortIsRtsFlowControlEnabled(int32_t uart)
{
    CellularPortUartData_t *pUartData = pGetUart(uart);
    bool rtsFlowControlIsEnabled = false;
    uint32_t flowControlStatus;

    if (pUartData != NULL) {
        // No need to lock the mutex, this is atomic
        flowControlStatus = LL_USART_GetHWFlowCtrl(gUartCfg[uart].pReg);
        if ((flowControlStatus == LL_USART_HWCONTROL_RTS) ||
            (flowControlStatus == LL_USART_HWCONTROL_RTS_CTS)) {
            rtsFlowControlIsEnabled = true;
        }
    }

    return rtsFlowControlIsEnabled;
}

// Determine if CTS flow control is enabled.
bool cellularPortIsCtsFlowControlEnabled(int32_t uart)
{
    CellularPortUartData_t *pUartData = pGetUart(uart);
    bool ctsFlowControlIsEnabled = false;
    uint32_t flowControlStatus;

    if (pUartData != NULL) {
        // No need to lock the mutex, this is atomic
        flowControlStatus = LL_USART_GetHWFlowCtrl(gUartCfg[uart].pReg);
        if ((flowControlStatus == LL_USART_HWCONTROL_CTS) ||
            (flowControlStatus == LL_USART_HWCONTROL_RTS_CTS)) {
            ctsFlowControlIsEnabled = true;
        }
    }

    return ctsFlowControlIsEnabled;
}

// End of file
