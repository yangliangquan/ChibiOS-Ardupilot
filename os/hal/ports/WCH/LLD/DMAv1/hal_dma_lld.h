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
 * @file    DMAv1/hal_dma_lld.h
 * @brief   DMA helper driver header.
 * @details The CH32H417 has two DMA controllers with eight channels each.
 *          Every channel is fed by a fully flexible DMAMUX selector, the
 *          channel to peripheral association is therefore a software
 *          convention and not a hardware constraint.
 *
 *          The channel register block is F1 compatible:
 *          @p CFGR, @p CNTR, @p PADDR, @p MADDR and @p M1ADDR (double buffer).
 *
 * @addtogroup DMA
 * @{
 */

#ifndef HAL_DMA_LLD_H
#define HAL_DMA_LLD_H

/* This header is included by hal_lld.h right after ch32h417.h, the vendor
   device definitions are therefore always available here. It pulls in the
   device encoding header on its own so that it can also be used directly
   from a driver implementation file.*/
#include "riscv_encoding.h"

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Total number of DMA channels.
 * @details DMAMUX channels 1..8 feed DMA1 channels 1..8, channels 9..16
 *          feed DMA2 channels 1..8.
 */
#define CH32_DMA_STREAMS                    16U

/**
 * @brief   Maximum number of transfers in a single operation.
 */
#define CH32_DMA_MAX_TRANSFER               65535U

/**
 * @brief   Mask of the INTFR bits passed to the DMA callback functions.
 * @note    The bits are already shifted to bit zero.
 */
#define CH32_DMA_ISR_MASK                   0x0EU

/**
 * @brief   Returns an unique numeric identifier for a DMA channel.
 *
 * @param[in] dma       the DMA unit number, 1 or 2
 * @param[in] ch        the channel number, 1..8
 * @return              An unique numeric channel identifier.
 */
#define CH32_DMA_STREAM_ID(dma, ch)                                         \
  ((((dma) - 1) * 8) + ((ch) - 1))

/**
 * @name    Special channel identifiers
 * @details The DMAMUX selector on this device is fully flexible, any channel
 *          can be routed to any request, therefore channels are normally
 *          allocated dynamically at driver start instead of being pinned by
 *          the configuration.
 * @{
 */
#define CH32_DMA_STREAM_ID_ANY              CH32_DMA_STREAMS
#define CH32_DMA_STREAM_ID_ANY_DMA1         (CH32_DMA_STREAMS + 1)
#define CH32_DMA_STREAM_ID_ANY_DMA2         (CH32_DMA_STREAMS + 2)
/** @} */

/**
 * @brief   Returns a pointer to a @p ch32_dma_stream_t structure.
 *
 * @param[in] id        the channel numeric identifier
 * @return              A pointer to the channel descriptor.
 * @note    Only valid for a concrete identifier, use @p ch32DmaStreamAlloc()
 *          with @p CH32_DMA_STREAM_ID_ANY and keep the returned pointer.
 */
#define CH32_DMA_STREAM(id)                 (&_ch32_dma_streams[id])

/**
 * @brief   Checks if a DMA channel identifier is valid.
 * @note    Written without casts so that it can be used in @p \#if
 *          expressions by the driver headers.
 */
#define CH32_DMA_IS_VALID_STREAM(id)                                         \
  (((id) >= 0) && ((id) <= CH32_DMA_STREAM_ID_ANY_DMA2))

/**
 * @brief   Checks if a DMA IRQ priority is within the valid range.
 * @note    The value is the PFIC @p IPRIOR[7:4] field, with PMTCFG=2 the top
 *          two bits are the preemption level (0 = highest).
 */
#define CH32_DMA_IS_VALID_PRIORITY(prio)    ((prio) <= 15)

/**
 * @name    CFGR register constants
 * @{
 */
#define CH32_DMA_CFGR_EN                    (1UL << 0)
#define CH32_DMA_CFGR_TCIE                  (1UL << 1)
#define CH32_DMA_CFGR_HTIE                  (1UL << 2)
#define CH32_DMA_CFGR_TEIE                  (1UL << 3)
#define CH32_DMA_CFGR_DIR                   (1UL << 4)
#define CH32_DMA_CFGR_CIRC                  (1UL << 5)
#define CH32_DMA_CFGR_PINC                  (1UL << 6)
#define CH32_DMA_CFGR_MINC                  (1UL << 7)
#define CH32_DMA_CFGR_PSIZE_MASK            (3UL << 8)
#define CH32_DMA_CFGR_PSIZE_BYTE            (0UL << 8)
#define CH32_DMA_CFGR_PSIZE_HWORD           (1UL << 8)
#define CH32_DMA_CFGR_PSIZE_WORD            (2UL << 8)
#define CH32_DMA_CFGR_MSIZE_MASK            (3UL << 10)
#define CH32_DMA_CFGR_MSIZE_BYTE            (0UL << 10)
#define CH32_DMA_CFGR_MSIZE_HWORD           (1UL << 10)
#define CH32_DMA_CFGR_MSIZE_WORD            (2UL << 10)
#define CH32_DMA_CFGR_SIZE_MASK             (CH32_DMA_CFGR_PSIZE_MASK |      \
                                             CH32_DMA_CFGR_MSIZE_MASK)
#define CH32_DMA_CFGR_PL_MASK               (3UL << 12)
#define CH32_DMA_CFGR_PL(n)                 ((uint32_t)(n) << 12)
#define CH32_DMA_CFGR_M2M                   (1UL << 14)
#define CH32_DMA_CFGR_DBM                   (1UL << 15)
#define CH32_DMA_CFGR_DBM_START             (1UL << 16)
/** @} */

/**
 * @name    Direction shortcuts
 * @{
 */
#define CH32_DMA_CFGR_DIR_P2M               0U
#define CH32_DMA_CFGR_DIR_M2P               CH32_DMA_CFGR_DIR
#define CH32_DMA_CFGR_DIR_M2M               CH32_DMA_CFGR_M2M
/** @} */

/**
 * @name    Status flags passed to the ISR callbacks, aligned to bit zero
 * @{
 */
#define CH32_DMA_ISR_TCIF                   (1U << 1)
#define CH32_DMA_ISR_HTIF                   (1U << 2)
#define CH32_DMA_ISR_TEIF                   (1U << 3)
/** @} */

/**
 * @name    DMAMUX requests, 1-based as in RM table 10-2
 * @{
 */
#define CH32_DMAMUX_SPI1_TX                 63U
#define CH32_DMAMUX_SPI1_RX                 64U
#define CH32_DMAMUX_SPI2_TX                 65U
#define CH32_DMAMUX_SPI2_RX                 66U
#define CH32_DMAMUX_SPI3_TX                 67U
#define CH32_DMAMUX_SPI3_RX                 68U
#define CH32_DMAMUX_SPI4_TX                 69U
#define CH32_DMAMUX_SPI4_RX                 70U
#define CH32_DMAMUX_QSPI1                   71U
#define CH32_DMAMUX_QSPI2                   72U
#define CH32_DMAMUX_I2C1_TX                 73U
#define CH32_DMAMUX_I2C1_RX                 74U
#define CH32_DMAMUX_I2C2_TX                 75U
#define CH32_DMAMUX_I2C2_RX                 76U
#define CH32_DMAMUX_I2C3_TX                 77U
#define CH32_DMAMUX_I2C3_RX                 78U
#define CH32_DMAMUX_I2C4_TX                 79U
#define CH32_DMAMUX_I2C4_RX                 80U
#define CH32_DMAMUX_USART1_TX               85U
#define CH32_DMAMUX_USART1_RX               86U
#define CH32_DMAMUX_USART2_TX               87U
#define CH32_DMAMUX_USART2_RX               88U
#define CH32_DMAMUX_USART3_TX               89U
#define CH32_DMAMUX_USART3_RX               90U
#define CH32_DMAMUX_USART4_TX               91U
#define CH32_DMAMUX_USART4_RX               92U
#define CH32_DMAMUX_USART5_TX               93U
#define CH32_DMAMUX_USART5_RX               94U
#define CH32_DMAMUX_USART6_TX               95U
#define CH32_DMAMUX_USART6_RX               96U
#define CH32_DMAMUX_USART7_TX               97U
#define CH32_DMAMUX_USART7_RX               98U
#define CH32_DMAMUX_USART8_TX               99U
#define CH32_DMAMUX_USART8_RX               100U
#define CH32_DMAMUX_DAC1                    103U
#define CH32_DMAMUX_DAC2                    104U
#define CH32_DMAMUX_SDIO                    111U
#define CH32_DMAMUX_ADC1                    120U
#define CH32_DMAMUX_ADC2                    121U
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Type of a DMA callback.
 *
 * @param[in] p         parameter for the registered function
 * @param[in] flags     pre-shifted content of the INTFR register, the bits
 *                      are aligned to bit zero
 */
typedef void (*ch32_dmaisr_t)(void *p, uint32_t flags);

/**
 * @brief   DMA channel descriptor structure.
 */
typedef struct {
  DMA_TypeDef           *dma;         /**< @brief Associated DMA unit.      */
  DMA_Channel_TypeDef   *channel;     /**< @brief Associated DMA channel.   */
  uint8_t               muxindex;     /**< @brief DMAMUX channel, 0..15.    */
  uint8_t               selfindex;    /**< @brief Index to self in array.   */
  uint8_t               shift;        /**< @brief Bit offset in INTFR and
                                           INTFCR registers.                */
  uint8_t               vector;       /**< @brief Associated IRQ vector.    */
} ch32_dma_stream_t;

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @name    Macro Functions
 * @{
 */
/**
 * @brief   Associates a peripheral data register to a DMA channel.
 */
#define ch32DmaStreamSetPeripheral(dmastp, addr) {                          \
  (dmastp)->channel->PADDR = (uint32_t)(addr);                              \
}

/**
 * @brief   Associates the memory destination to a DMA channel.
 */
#define ch32DmaStreamSetMemory0(dmastp, addr) {                             \
  (dmastp)->channel->MADDR = (uint32_t)(addr);                              \
}

/**
 * @brief   Associates the second memory buffer to a DMA channel.
 * @note    Only meaningful in double buffer mode.
 */
#define ch32DmaStreamSetMemory1(dmastp, addr) {                             \
  (dmastp)->channel->M1ADDR = (uint32_t)(addr);                             \
}

/**
 * @brief   Sets the number of transfers to be performed.
 */
#define ch32DmaStreamSetTransactionSize(dmastp, size) {                     \
  (dmastp)->channel->CNTR = (uint32_t)(size);                               \
}

/**
 * @brief   Returns the number of transfers to be performed.
 */
#define ch32DmaStreamGetTransactionSize(dmastp)                             \
  ((size_t)((dmastp)->channel->CNTR))

/**
 * @brief   Programs the channel mode settings.
 */
#define ch32DmaStreamSetMode(dmastp, mode) {                                \
  (dmastp)->channel->CFGR = (uint32_t)(mode);                               \
}

/**
 * @brief   DMA channel enable.
 */
#define ch32DmaStreamEnable(dmastp) {                                       \
  (dmastp)->channel->CFGR |= CH32_DMA_CFGR_EN;                              \
}

/**
 * @brief   DMA channel disable.
 * @details The channel is disabled, its interrupt enables cleared and any
 *          pending interrupt cleared.
 */
#define ch32DmaStreamDisable(dmastp) {                                      \
  (dmastp)->channel->CFGR &= ~(CH32_DMA_CFGR_TCIE | CH32_DMA_CFGR_HTIE |    \
                               CH32_DMA_CFGR_TEIE | CH32_DMA_CFGR_EN);      \
  ch32DmaStreamClearInterrupt(dmastp);                                      \
}

/**
 * @brief   DMA channel interrupt sources clear.
 */
#define ch32DmaStreamClearInterrupt(dmastp) {                               \
  (dmastp)->dma->INTFCR = CH32_DMA_ISR_MASK << (dmastp)->shift;             \
}

/**
 * @brief   Associates a peripheral request to a DMA channel.
 * @note    The DMAMUX selector is fully flexible, no hardware restriction
 *          prevents two channels from selecting the same request, the
 *          uniqueness of the routing is a software responsibility.
 *
 * @param[in] dmastp    pointer to a @p ch32_dma_stream_t structure
 * @param[in] req       1-based DMAMUX request identifier
 */
#define ch32DmaStreamSetRequest(dmastp, req) {                              \
  volatile uint32_t *muxr_ = &DMAMUX->CFGR0_3 + ((dmastp)->muxindex >> 2U); \
  uint32_t shift_ = ((uint32_t)(dmastp)->muxindex & 3U) * 8U;               \
  *muxr_ = (*muxr_ & ~(0x7FU << shift_)) |                                  \
           ((((uint32_t)(req) - 1U) & 0x7FU) << shift_);                    \
}

/**
 * @brief   Starts a memory to memory operation using the specified channel.
 */
#define ch32DmaStartMemCopy(dmastp, mode, src, dst, n) {                    \
  ch32DmaStreamSetPeripheral(dmastp, src);                                  \
  ch32DmaStreamSetMemory0(dmastp, dst);                                     \
  ch32DmaStreamSetTransactionSize(dmastp, n);                               \
  ch32DmaStreamSetMode(dmastp, (mode) |                                     \
                               CH32_DMA_CFGR_MINC | CH32_DMA_CFGR_PINC |    \
                               CH32_DMA_CFGR_DIR_M2M | CH32_DMA_CFGR_EN);   \
}

/**
 * @brief   Polled wait for DMA transfer end.
 */
#define ch32DmaWaitCompletion(dmastp) {                                     \
  while ((dmastp)->channel->CNTR > 0U) {                                    \
  }                                                                         \
  ch32DmaStreamDisable(dmastp);                                             \
}
/** @} */

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if !defined(__DOXYGEN__)
extern const ch32_dma_stream_t _ch32_dma_streams[CH32_DMA_STREAMS];
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void dmaInit(void);
  const ch32_dma_stream_t *ch32DmaStreamAllocI(uint32_t id,
                                               uint32_t priority,
                                               ch32_dmaisr_t func,
                                               void *param);
  const ch32_dma_stream_t *ch32DmaStreamAlloc(uint32_t id,
                                              uint32_t priority,
                                              ch32_dmaisr_t func,
                                              void *param);
  void ch32DmaStreamFreeI(const ch32_dma_stream_t *dmastp);
  void ch32DmaStreamFree(const ch32_dma_stream_t *dmastp);
  void ch32DmaServeInterrupt(const ch32_dma_stream_t *dmastp);
#ifdef __cplusplus
}
#endif

#endif /* HAL_DMA_LLD_H */

/** @} */
