/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/**
 * @file    DMAv1/hal_dma_lld.c
 * @brief   DMA helper driver code.
 *
 * @addtogroup DMA
 * @details DMA sharing helper driver. The DMA channels are a shared resource,
 *          this driver allows to allocate and free them at runtime in order
 *          to let all the device drivers coordinate the access.
 * @note    The DMA ISR handlers are all declared into this module because
 *          sharing, the device drivers associate a callback function to a
 *          channel when allocating it.
 * @{
 */

#include "hal.h"

/* The following macro is only defined if some driver requiring DMA services
   has been enabled.*/
#if defined(CH32_DMA_REQUIRED) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/**
 * @brief   Mask of the DMA1 channels in @p dma.allocated_mask.
 */
#define CH32_DMA1_STREAMS_MASK      ((1UL << 8) - 1UL)

/**
 * @brief   Mask of the DMA2 channels in @p dma.allocated_mask.
 */
#define CH32_DMA2_STREAMS_MASK      (((1UL << 8) - 1UL) << 8)

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/**
 * @brief   DMA channel descriptors.
 * @details This table keeps the association between an unique channel
 *          identifier and the involved physical registers.
 * @note    Don't use this array directly, use the appropriate wrapper macros
 *          instead: @p CH32_DMA_STREAM() and @p CH32_DMA_STREAM_ID().
 */
const ch32_dma_stream_t _ch32_dma_streams[CH32_DMA_STREAMS] = {
  {DMA1, DMA1_Channel1,  0,  0,  0, DMA1_Channel1_IRQn},
  {DMA1, DMA1_Channel2,  1,  1,  4, DMA1_Channel2_IRQn},
  {DMA1, DMA1_Channel3,  2,  2,  8, DMA1_Channel3_IRQn},
  {DMA1, DMA1_Channel4,  3,  3, 12, DMA1_Channel4_IRQn},
  {DMA1, DMA1_Channel5,  4,  4, 16, DMA1_Channel5_IRQn},
  {DMA1, DMA1_Channel6,  5,  5, 20, DMA1_Channel6_IRQn},
  {DMA1, DMA1_Channel7,  6,  6, 24, DMA1_Channel7_IRQn},
  {DMA1, DMA1_Channel8,  7,  7, 28, DMA1_Channel8_IRQn},
  {DMA2, DMA2_Channel1,  8,  8,  0, DMA2_Channel1_IRQn},
  {DMA2, DMA2_Channel2,  9,  9,  4, DMA2_Channel2_IRQn},
  {DMA2, DMA2_Channel3, 10, 10,  8, DMA2_Channel3_IRQn},
  {DMA2, DMA2_Channel4, 11, 11, 12, DMA2_Channel4_IRQn},
  {DMA2, DMA2_Channel5, 12, 12, 16, DMA2_Channel5_IRQn},
  {DMA2, DMA2_Channel6, 13, 13, 20, DMA2_Channel6_IRQn},
  {DMA2, DMA2_Channel7, 14, 14, 24, DMA2_Channel7_IRQn},
  {DMA2, DMA2_Channel8, 15, 15, 28, DMA2_Channel8_IRQn}
};

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/**
 * @brief   Global DMA-related data structures.
 */
static struct {
  /**
   * @brief   Mask of the allocated channels.
   */
  uint32_t          allocated_mask;
  /**
   * @brief   DMA IRQ redirectors.
   */
  struct {
    /**
     * @brief   DMA callback function.
     */
    ch32_dmaisr_t     func;
    /**
     * @brief   DMA callback parameter.
     */
    void              *param;
  } streams[CH32_DMA_STREAMS];
} dma;

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

/**
 * @brief   DMA1 channel 1 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA1_Channel1_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(0));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA1 channel 2 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA1_Channel2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(1));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA1 channel 3 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA1_Channel3_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(2));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA1 channel 4 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA1_Channel4_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(3));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA1 channel 5 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA1_Channel5_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(4));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA1 channel 6 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA1_Channel6_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(5));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA1 channel 7 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA1_Channel7_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(6));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA1 channel 8 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA1_Channel8_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(7));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA2 channel 1 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA2_Channel1_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(8));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA2 channel 2 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA2_Channel2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(9));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA2 channel 3 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA2_Channel3_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(10));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA2 channel 4 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA2_Channel4_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(11));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA2 channel 5 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA2_Channel5_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(12));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA2 channel 6 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA2_Channel6_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(13));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA2 channel 7 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA2_Channel7_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(14));
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   DMA2 channel 8 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(DMA2_Channel8_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  ch32DmaServeInterrupt(CH32_DMA_STREAM(15));
  OSAL_IRQ_EPILOGUE();
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   DMA helper initialization.
 *
 * @init
 */
void dmaInit(void) {
  unsigned i;

  /* DMA and DMAMUX clocks are part of the HB bus.*/
  RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1 | RCC_HBPeriph_DMA2, ENABLE);

  dma.allocated_mask = 0U;
  for (i = 0U; i < CH32_DMA_STREAMS; i++) {
    _ch32_dma_streams[i].channel->CFGR  = 0U;
    _ch32_dma_streams[i].channel->CNTR  = 0U;
    _ch32_dma_streams[i].channel->PADDR = 0U;
    _ch32_dma_streams[i].channel->MADDR = 0U;
    _ch32_dma_streams[i].channel->M1ADDR = 0U;
    dma.streams[i].func  = NULL;
    dma.streams[i].param = NULL;
  }
  DMA1->INTFCR = 0xFFFFFFFFU;
  DMA2->INTFCR = 0xFFFFFFFFU;
  (void)DMA1->INTFR;
  (void)DMA2->INTFR;
}

/**
 * @brief   Allocates a DMA channel.
 * @details The DMAMUX routing is fully flexible on this device, so a channel
 *          does not have to be pinned to a given peripheral: passing
 *          @p CH32_DMA_STREAM_ID_ANY picks the first free channel. The DMA
 *          clocks are enabled and the associated IRQ vector is enabled and
 *          initialized.
 *
 * @param[in] id        one of the special identifiers:
 *                      - @p CH32_DMA_STREAM_ID_ANY, first free channel of
 *                        either unit
 *                      - @p CH32_DMA_STREAM_ID_ANY_DMA1, first free DMA1
 *                        channel
 *                      - @p CH32_DMA_STREAM_ID_ANY_DMA2, first free DMA2
 *                        channel
 *                      .
 *                      A concrete identifier 0..15 can still be used to pin
 *                      a channel when the application really needs it.
 * @param[in] priority  IRQ priority for the DMA channel, this is the PFIC
 *                      @p IPRIOR[7:4] field
 * @param[in] func      handling function pointer, can be @p NULL
 * @param[in] param     a parameter to be passed to the handling function
 * @return              Pointer to the allocated @p ch32_dma_stream_t
 *                      structure.
 * @retval NULL         if no channel is available.
 *
 * @iclass
 */
const ch32_dma_stream_t *ch32DmaStreamAllocI(uint32_t id,
                                             uint32_t priority,
                                             ch32_dmaisr_t func,
                                             void *param) {
  uint32_t i, startid, endid, mask;

  osalDbgCheckClassI();

  if (id < CH32_DMA_STREAMS) {
    startid = id;
    endid   = id;
  }
  else if (id == CH32_DMA_STREAM_ID_ANY) {
    startid = 0U;
    endid   = CH32_DMA_STREAMS - 1U;
  }
  else if (id == CH32_DMA_STREAM_ID_ANY_DMA1) {
    startid = 0U;
    endid   = 7U;
  }
  else if (id == CH32_DMA_STREAM_ID_ANY_DMA2) {
    startid = 8U;
    endid   = CH32_DMA_STREAMS - 1U;
  }
  else {
    osalDbgCheck(false);
    return NULL;
  }

  /* First fit search, a specific request never steals a channel from
     another pending allocation because the whole scan runs with the kernel
     locked.*/
  for (i = startid; i <= endid; i++) {
    mask = 1UL << i;
    if ((dma.allocated_mask & mask) != 0U) {
      continue;
    }

    /* Installs the DMA handler.*/
    dma.streams[i].func  = func;
    dma.streams[i].param = param;
    dma.allocated_mask  |= mask;

    /* Channel in a known state.*/
    ch32DmaStreamDisable(CH32_DMA_STREAM(i));
    CH32_DMA_STREAM(i)->channel->CFGR = 0U;

    /* Enables the associated IRQ vector.*/
    if (func != NULL) {
      NVIC_SetPriority((IRQn_Type)CH32_DMA_STREAM(i)->vector,
                       (uint8_t)CH32_IRQ_PRIORITY(priority));
      NVIC_ClearPendingIRQ((IRQn_Type)CH32_DMA_STREAM(i)->vector);
      NVIC_EnableIRQ((IRQn_Type)CH32_DMA_STREAM(i)->vector);
    }

    return CH32_DMA_STREAM(i);
  }

  return NULL;
}

/**
 * @brief   Allocates a DMA channel.
 *
 * @param[in] id        numeric identifier of a specific channel
 * @param[in] priority  IRQ priority for the DMA channel
 * @param[in] func      handling function pointer, can be @p NULL
 * @param[in] param     a parameter to be passed to the handling function
 * @return              Pointer to the allocated @p ch32_dma_stream_t
 *                      structure.
 * @retval NULL         if the channel is not available.
 *
 * @api
 */
const ch32_dma_stream_t *ch32DmaStreamAlloc(uint32_t id,
                                            uint32_t priority,
                                            ch32_dmaisr_t func,
                                            void *param) {
  const ch32_dma_stream_t *dmastp;

  osalSysLock();
  dmastp = ch32DmaStreamAllocI(id, priority, func, param);
  osalSysUnlock();

  return dmastp;
}

/**
 * @brief   Releases a DMA channel.
 *
 * @param[in] dmastp    pointer to a @p ch32_dma_stream_t structure
 *
 * @iclass
 */
void ch32DmaStreamFreeI(const ch32_dma_stream_t *dmastp) {
  uint32_t selfindex = (uint32_t)dmastp->selfindex;

  osalDbgCheck(dmastp != NULL);
  osalDbgAssert((dma.allocated_mask & (1UL << selfindex)) != 0U,
                "not allocated");

  ch32DmaStreamDisable(dmastp);

  dma.allocated_mask &= ~(1UL << selfindex);

  NVIC_DisableIRQ((IRQn_Type)dmastp->vector);

  dma.streams[selfindex].func  = NULL;
  dma.streams[selfindex].param = NULL;
}

/**
 * @brief   Releases a DMA channel.
 *
 * @param[in] dmastp    pointer to a @p ch32_dma_stream_t structure
 *
 * @api
 */
void ch32DmaStreamFree(const ch32_dma_stream_t *dmastp) {

  osalSysLock();
  ch32DmaStreamFreeI(dmastp);
  osalSysUnlock();
}

/**
 * @brief   Serves a DMA IRQ.
 *
 * @param[in] dmastp    pointer to a @p ch32_dma_stream_t structure
 *
 * @special
 */
void ch32DmaServeInterrupt(const ch32_dma_stream_t *dmastp) {
  uint32_t flags = (dmastp->dma->INTFR >> dmastp->shift) & CH32_DMA_ISR_MASK;

  /* Only the sources with the corresponding enable bit set are serviced,
     the four low CFGR bits are aligned with the INTFR status bits.*/
  if ((flags & dmastp->channel->CFGR) != 0U) {
    dmastp->dma->INTFCR = flags << dmastp->shift;
    if (dma.streams[dmastp->selfindex].func != NULL) {
      dma.streams[dmastp->selfindex].func(dma.streams[dmastp->selfindex].param,
                                          flags);
    }
  }
}

#endif /* defined(CH32_DMA_REQUIRED) */

/** @} */
