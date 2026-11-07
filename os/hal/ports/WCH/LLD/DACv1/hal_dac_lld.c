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
 * @file    DACv1/hal_dac_lld.c
 * @brief   CH32H417 DAC subsystem low level driver source.
 *
 * @details Both channels are initialized at start with the software
 *          trigger; dacPutChannelX() writes the holding register and fires
 *          the trigger. Buffer conversions (dacStartConversion) switch the
 *          channels to automatic conversion and let the DMA channel feed the
 *          data registers.
 *
 * @addtogroup DAC
 * @{
 */

#include "hal.h"

#if (HAL_USE_DAC == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_dac.h"

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   DMA end of transfer service routine.
 *
 * @param[in] dacp      pointer to the @p DACDriver object
 * @param[in] flags     pre-shifted content of the INTFR register
 */
static void dac_lld_serve_interrupt(DACDriver *dacp, uint32_t flags) {

  /* DMA errors handling.*/
  if ((flags & CH32_DMA_ISR_TEIF) != 0U) {
    ch32DmaStreamDisable(dacp->dma);
    DAC_DMACmd(DAC_Channel_1, DISABLE);
    DAC_DMACmd(DAC_Channel_2, DISABLE);

    /* DMA failure reported to the upper layer.*/
    _dac_isr_error_code(dacp, HAL_RET_HW_FAILURE);
    return;
  }

  /* It is possible that the conversion group has already be reset by the
     error handler, in this case this interrupt is spurious.*/
  if (dacp->grpp == NULL) {
    return;
  }

  /* DAC conversions are always circular, the DMA keeps refilling the
     output from the samples buffer until the conversion is stopped.*/
  if ((flags & CH32_DMA_ISR_TCIF) != 0U) {
    _dac_isr_full_code(dacp);
  }
  else if ((flags & CH32_DMA_ISR_HTIF) != 0U) {
    _dac_isr_half_code(dacp);
  }
}

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_DAC_USE_DAC1 == TRUE) && !defined(__DOXYGEN__)
/**
 * @brief   DACD1 driver identifier.
 */
DACDriver DACD1 = {
  .dac = DAC
};
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level DAC driver initialization.
 *
 * @notapi
 */
void dac_lld_init(void) {

#if CH32H417_DAC_USE_DAC1 == TRUE
  dacObjectInit(&DACD1);
#endif
}

/**
 * @brief   DAC driver configuration and start.
 *
 * @notapi
 */
msg_t dac_lld_start(DACDriver *dacp) {
  DAC_InitTypeDef init;

  RCC_HB1PeriphClockCmd(RCC_HB1Periph_DAC, ENABLE);

  DAC_StructInit(&init);
  init.DAC_Trigger = DAC_Trigger_Software;
  init.DAC_WaveGeneration = DAC_WaveGeneration_None;
  init.DAC_OutputBuffer = DAC_OutputBuffer_Enable;

  DAC_Init(DAC_Channel_1, &init);
  DAC_Init(DAC_Channel_2, &init);
  DAC_Cmd(DAC_Channel_1, ENABLE);
  DAC_Cmd(DAC_Channel_2, ENABLE);

  /* Dynamic channel allocation, the request routing is done on the channel
     that was handed out.*/
  dacp->dma = ch32DmaStreamAllocI(CH32_DMA_STREAM_ID_ANY,
                                  CH32H417_DAC_DAC1_DMA_PRIORITY,
                                  (ch32_dmaisr_t)dac_lld_serve_interrupt,
                                  (void *)dacp);
  osalDbgAssert(dacp->dma != NULL, "DAC1 DMA channel not available");
  if (dacp->dma == NULL) {
    return HAL_RET_NO_RESOURCE;
  }

  return HAL_RET_SUCCESS;
}

/**
 * @brief   DAC driver stop.
 *
 * @notapi
 */
void dac_lld_stop(DACDriver *dacp) {

  DAC_Cmd(DAC_Channel_1, DISABLE);
  DAC_Cmd(DAC_Channel_2, DISABLE);

  if (dacp->dma != NULL) {
    ch32DmaStreamFreeI(dacp->dma);
    dacp->dma = NULL;
  }
}

/**
 * @brief   Direct channel output of one sample.
 *
 * @notapi
 */
msg_t dac_lld_put_channel(DACDriver *dacp,
                          dacchannel_t channel,
                          dacsample_t sample) {

  osalDbgAssert(channel < 2U, "invalid channel");
  (void)dacp;

  if (channel == 0U) {
    DAC_SetChannel1Data(DAC_Align_12b_R, sample);
    DAC_SoftwareTriggerCmd(DAC_Channel_1, ENABLE);
  }
  else {
    DAC_SetChannel2Data(DAC_Align_12b_R, sample);
    DAC_SoftwareTriggerCmd(DAC_Channel_2, ENABLE);
  }

  return HAL_RET_SUCCESS;
}

/**
 * @brief   Starts a buffered conversion.
 *
 * @notapi
 */
msg_t dac_lld_start_conversion(DACDriver *dacp) {
  const DACConversionGroup *grpp = dacp->grpp;
  DAC_InitTypeDef init;
  uint32_t mode;
  uint32_t total;
  bool dual = (grpp->num_channels > 1U);

  osalDbgAssert((grpp->num_channels == 1U) || (grpp->num_channels == 2U),
                "invalid channel count");

  /* No channel could be allocated at start, the conversion cannot run.*/
  osalDbgAssert(dacp->dma != NULL, "DAC DMA channel not allocated");
  if (dacp->dma == NULL) {
    return HAL_RET_NO_RESOURCE;
  }

  /* The conversions are paced by the trigger selected in the conversion
     group. A hardware trigger is mandatory for the buffered output: the
     DAC issues a DMA request on each trigger event only, with
     DAC_Trigger_None no request is generated and the DMA stays idle. The
     software trigger used by dacPutChannelX() is replaced here and
     restored in dac_lld_stop_conversion().*/
  DAC_StructInit(&init);
  init.DAC_Trigger = grpp->trigger;
  init.DAC_WaveGeneration = DAC_WaveGeneration_None;
  init.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
  DAC_Init(DAC_Channel_1, &init);
  DAC_Init(DAC_Channel_2, &init);
  DAC_Cmd(DAC_Channel_1, ENABLE);
  DAC_Cmd(DAC_Channel_2, ENABLE);

  /* Destination register: the dual register when both channels are driven,
     so that a single transfer updates both channels.*/
  if (dual) {
    ch32DmaStreamSetPeripheral(dacp->dma, &dacp->dac->RD12BDHR);
    ch32DmaStreamSetRequest(dacp->dma, CH32_DMAMUX_DAC1);
  }
  else if (grpp->channels[0] == 0U) {
    ch32DmaStreamSetPeripheral(dacp->dma, &dacp->dac->R12BDHR1);
    ch32DmaStreamSetRequest(dacp->dma, CH32_DMAMUX_DAC1);
  }
  else {
    ch32DmaStreamSetPeripheral(dacp->dma, &dacp->dac->R12BDHR2);
    ch32DmaStreamSetRequest(dacp->dma, CH32_DMAMUX_DAC2);
  }

  /* DMA setup: the dual register takes one 32-bit word per group, both
     channels packed, the single channel registers one halfword per
     sample. DAC conversions are always circular, the half transfer
     interrupt is used for streaming.*/
  total = (uint32_t)dacp->depth;
  mode = CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_MINC |
         (dual ? (CH32_DMA_CFGR_PSIZE_WORD | CH32_DMA_CFGR_MSIZE_WORD)
               : (CH32_DMA_CFGR_PSIZE_HWORD | CH32_DMA_CFGR_MSIZE_HWORD)) |
         CH32_DMA_CFGR_CIRC | CH32_DMA_CFGR_TCIE | CH32_DMA_CFGR_TEIE;
  if (dacp->depth > 1U) {
    mode |= CH32_DMA_CFGR_HTIE;
  }

  ch32DmaStreamDisable(dacp->dma);
  ch32DmaStreamSetMemory0(dacp->dma, dacp->samples);
  ch32DmaStreamSetTransactionSize(dacp->dma, total);
  ch32DmaStreamSetMode(dacp->dma, mode);
  ch32DmaStreamClearInterrupt(dacp->dma);

  /* One request per trigger event: the dual data register write updates
     both channels at once, so only channel 1 has to request the DMA, a
     second request would consume the buffer at twice the trigger rate.*/
  DAC_DMACmd(DAC_Channel_1, ENABLE);

  ch32DmaStreamEnable(dacp->dma);

  return HAL_RET_SUCCESS;
}

/**
 * @brief   Stops a buffered conversion.
 *
 * @notapi
 */
void dac_lld_stop_conversion(DACDriver *dacp) {
  DAC_InitTypeDef init;

  if (dacp->dma != NULL) {
    ch32DmaStreamDisable(dacp->dma);
  }
  DAC_DMACmd(DAC_Channel_1, DISABLE);
  DAC_DMACmd(DAC_Channel_2, DISABLE);

  /* Restoring the software triggered mode used by dacPutChannelX().*/
  DAC_StructInit(&init);
  init.DAC_Trigger = DAC_Trigger_Software;
  init.DAC_WaveGeneration = DAC_WaveGeneration_None;
  init.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
  DAC_Init(DAC_Channel_1, &init);
  DAC_Init(DAC_Channel_2, &init);
}

#endif /* HAL_USE_DAC == TRUE */

/** @} */
