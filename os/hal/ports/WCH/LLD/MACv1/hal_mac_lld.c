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
 * @file    MACv1/hal_mac_lld.c
 * @brief   CH32H417 low level MAC driver code.
 *
 * @details Port of the STM32 MACv1 driver to the CH32H417 Synopsys
 *          DWC Ether MAC (identical register names), no RMII-select
 *          register handling (see porting notes), ETH PLL + 125MHz
 *          clock chain enabled in start.
 *
 * @addtogroup MAC
 * @{
 */

#include <string.h>

#include "hal.h"

#if HAL_USE_MAC || defined(__DOXYGEN__)

#include "hal_mii.h"
#include "ch32h417_eth.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

#define BUFFER_SIZE ((((CH32H417_MAC_BUFFERS_SIZE - 1) | 3) + 1) / 4)

/* RISC-V ordering barrier (descriptor OWN handshakes). */
#define CH32_DSB()  __asm volatile ("fence w, w" ::: "memory")

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/**
 * @brief   Ethernet driver 1.
 */
MACDriver ETHD1;

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

static const uint8_t default_mac_address[] = {0xAA, 0x55, 0x13,
                                              0x37, 0x01, 0x10};

static ch32_eth_rx_descriptor_t __eth_desc_rx[CH32H417_MAC_RECEIVE_BUFFERS];
static ch32_eth_tx_descriptor_t __eth_desc_tx[CH32H417_MAC_TRANSMIT_BUFFERS];

static uint32_t __eth_buf_rx[CH32H417_MAC_RECEIVE_BUFFERS][BUFFER_SIZE];
static uint32_t __eth_buf_tx[CH32H417_MAC_TRANSMIT_BUFFERS][BUFFER_SIZE];

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Writes a PHY register.
 *
 * @notapi
 */
void mii_write(MACDriver *macp, uint32_t reg, uint32_t value) {

  ETH->MACMIIDR = value;
  ETH->MACMIIAR = macp->phyaddr | (reg << 6) | CH32H417_MAC_MDC_CR |
                  ETH_MACMIIAR_MW | ETH_MACMIIAR_MB;
  while ((ETH->MACMIIAR & ETH_MACMIIAR_MB) != 0)
    ;
}

/**
 * @brief   Reads a PHY register.
 *
 * @notapi
 */
uint32_t mii_read(MACDriver *macp, uint32_t reg) {

  ETH->MACMIIAR = macp->phyaddr | (reg << 6) | CH32H417_MAC_MDC_CR |
                  ETH_MACMIIAR_MB;
  while ((ETH->MACMIIAR & ETH_MACMIIAR_MB) != 0)
    ;
  return ETH->MACMIIDR;
}

#if !defined(BOARD_PHY_ADDRESS)
/**
 * @brief   PHY address detection.
 *
 * @param[in] macp      pointer to the @p MACDriver object
 */
static void mii_find_phy(MACDriver *macp) {
  uint32_t i;

#if CH32H417_MAC_PHY_TIMEOUT > 0
  unsigned n = CH32H417_MAC_PHY_TIMEOUT;
 do {
#endif
    for (i = 0U; i <= 31U; i++) {
      macp->phyaddr = i << 11U;
      ETH->MACMIIDR = (i << 6U) | CH32H417_MAC_MDC_CR;
      if ((mii_read(macp, MII_PHYSID1) == (BOARD_PHY_ID >> 16U)) &&
          ((mii_read(macp, MII_PHYSID2) & 0xFFF0U) == (BOARD_PHY_ID & 0xFFF0U))) {
        return;
      }
    }
#if CH32H417_MAC_PHY_TIMEOUT > 0
    n--;
  } while (n > 0U);
#endif
  /* Wrong or defective board.*/
  osalSysHalt("MAC failure");
}
#endif

/**
 * @brief   MAC address setup.
 *
 * @param[in] p         pointer to a six bytes buffer containing the MAC
 *                      address
 */
static void mac_lld_set_address(const uint8_t *p) {

  /* MAC address configuration, only a single address comparator is used,
     hash table not used.*/
  ETH->MACA0HR   = ((uint32_t)p[5] << 8) |
                   ((uint32_t)p[4] << 0);
  ETH->MACA0LR   = ((uint32_t)p[3] << 24) |
                   ((uint32_t)p[2] << 16) |
                   ((uint32_t)p[1] << 8) |
                   ((uint32_t)p[0] << 0);
  ETH->MACA1HR   = 0x0000FFFF;
  ETH->MACA1LR   = 0xFFFFFFFF;
  ETH->MACA2HR   = 0x0000FFFF;
  ETH->MACA2LR   = 0xFFFFFFFF;
  ETH->MACA3HR   = 0x0000FFFF;
  ETH->MACA3LR   = 0xFFFFFFFF;
  ETH->MACHTHR   = 0;
  ETH->MACHTLR   = 0;
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

/**
 * @brief   ETH global interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(ETH_IRQHandler) {
  MACDriver *macp = &ETHD1;
  uint32_t dmasr;

  OSAL_IRQ_PROLOGUE();

  dmasr = ETH->DMASR;
  ETH->DMASR = dmasr; /* Clear status bits.*/

  if ((dmasr & (ETH_DMASR_RS | ETH_DMASR_TS)) != 0U) {
    if ((dmasr & ETH_DMASR_TS) != 0U) {
      /* Data Transmitted.*/
      __mac_tx_wakeup(macp);
    }

    if ((dmasr & ETH_DMASR_RS) != 0U) {
      /* Data Received.*/
      __mac_rx_wakeup(macp);
    }

    __mac_callback(macp);
  }

  OSAL_IRQ_EPILOGUE();
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level MAC initialization.
 *
 * @notapi
 */
bool mac_lld_init(void) {
  unsigned i;

  macObjectInit(&ETHD1);
  ETHD1.link_up = false;

  /* Descriptor tables are initialized in chained mode, note that the first
     word is not initialized here but in mac_lld_start().*/
  for (i = 0; i < CH32H417_MAC_RECEIVE_BUFFERS; i++) {
    __eth_desc_rx[i].rdes1 = CH32_RDES1_RCH | CH32H417_MAC_BUFFERS_SIZE;
    __eth_desc_rx[i].rdes2 = (uint32_t)__eth_buf_rx[i];
    __eth_desc_rx[i].rdes3 = (uint32_t)&__eth_desc_rx[(i + 1) % CH32H417_MAC_RECEIVE_BUFFERS];
  }
  for (i = 0; i < CH32H417_MAC_TRANSMIT_BUFFERS; i++) {
    __eth_desc_tx[i].tdes1 = 0;
    __eth_desc_tx[i].tdes2 = (uint32_t)__eth_buf_tx[i];
    __eth_desc_tx[i].tdes3 = (uint32_t)&__eth_desc_tx[(i + 1) % CH32H417_MAC_TRANSMIT_BUFFERS];
  }

  /* Optional RGMII pin interface enable (gigabit wiring). */
#if CH32H417_MAC_RGMII
  RCC_RGMIIcmd(ENABLE);
#endif

  /* Reset of the MAC core.*/
  RCC_HBPeriphResetCmd(RCC_HBPeriph_ETH, ENABLE);
  RCC_HBPeriphResetCmd(RCC_HBPeriph_ETH, DISABLE);

  /* MAC clocks temporary activation.*/
  RCC_HBPeriphClockCmd(RCC_HBPeriph_ETH, ENABLE);

  /* PHY address setup.*/
#if defined(BOARD_PHY_ADDRESS)
  ETHD1.phyaddr = BOARD_PHY_ADDRESS << 11;
#else
  mii_find_phy(&ETHD1);
#endif

#if defined(BOARD_PHY_RESET)
  /* PHY board-specific reset procedure.*/
  BOARD_PHY_RESET();
#else
  /* PHY soft reset procedure.*/
  mii_write(&ETHD1, MII_BMCR, BMCR_RESET);
#if defined(BOARD_PHY_RESET_DELAY)
  osalSysPolledDelayX(BOARD_PHY_RESET_DELAY);
#endif
  while (mii_read(&ETHD1, MII_BMCR) & BMCR_RESET)
    ;
#endif

#if CH32H417_MAC_ETH1_CHANGE_PHY_STATE
  /* PHY in power down mode until the driver will be started.*/
  mii_write(&ETHD1, MII_BMCR, mii_read(&ETHD1, MII_BMCR) | BMCR_PDOWN);
#endif

  /* MAC clocks stopped again.*/
  RCC_HBPeriphClockCmd(RCC_HBPeriph_ETH, DISABLE);

  return true;
}

/**
 * @brief   Configures and activates the MAC peripheral.
 *
 * @param[in] macp      pointer to the @p MACDriver object
 *
 * @notapi
 */
void mac_lld_start(MACDriver *macp) {
  unsigned i;

  /* Resets the state of all descriptors.*/
  for (i = 0; i < CH32H417_MAC_RECEIVE_BUFFERS; i++)
    __eth_desc_rx[i].rdes0 = CH32_RDES0_OWN;
  macp->rxptr = (ch32_eth_rx_descriptor_t *)__eth_desc_rx;
  for (i = 0; i < CH32H417_MAC_TRANSMIT_BUFFERS; i++)
    __eth_desc_tx[i].tdes0 = CH32_TDES0_TCH;
  macp->txptr = (ch32_eth_tx_descriptor_t *)__eth_desc_tx;

  /* MAC clocks activation and commanded reset procedure.*/
  RCC_HBPeriphClockCmd(RCC_HBPeriph_ETH, ENABLE);
  ETH->DMABMR |= ETH_DMABMR_SR;
  while (ETH->DMABMR & ETH_DMABMR_SR)
    ;

  /* Dedicated ETH PLL and 125MHz reference chain.*/
#if CH32H417_MAC_ENABLE_ETH_PLL
  RCC_ETH_PLLCmd(ENABLE);
  {
    unsigned spin = 1000000U;
    while ((RCC_GetFlagStatus(RCC_FLAG_ETHPLLRDY) == RESET) && (spin > 0U)) {
      spin--;
    }
  }
  RCC_ETH125MCLKConfig(RCC_ETH125MSource_PLLCLK);
#endif

  /* ISR vector enabled.*/
  NVIC_EnableIRQ(ETH_IRQn);

#if CH32H417_MAC_ETH1_CHANGE_PHY_STATE
  /* PHY in power up mode.*/
  mii_write(macp, MII_BMCR, mii_read(macp, MII_BMCR) & ~BMCR_PDOWN);
#endif

  /* MAC configuration.*/
  ETH->MACFFR    = 0;
  ETH->MACFCR    = 0;
  ETH->MACVLANTR = 0;

  /* MAC address setup.*/
  if (macp->config->mac_address == NULL)
    mac_lld_set_address(default_mac_address);
  else
    mac_lld_set_address(macp->config->mac_address);

  /* Transmitter and receiver enabled.
     Note that the complete setup of the MAC is performed when the link
     status is detected.*/
#if CH32H417_MAC_IP_CHECKSUM_OFFLOAD
  ETH->MACCR = ETH_MACCR_IPCO | ETH_MACCR_RE | ETH_MACCR_TE;
#else
  ETH->MACCR =                  ETH_MACCR_RE | ETH_MACCR_TE;
#endif

  /* MMC configuration:
     Disable all MMC interrupts.*/
  ETH->MMCRIMR = ETH_MMCRIMR_RFCEM  | ETH_MMCRIMR_RFAEM   | ETH_MMCRIMR_RGUFM;
  ETH->MMCTIMR = ETH_MMCTIMR_TGFSCM | ETH_MMCTIMR_TGFMSCM | ETH_MMCTIMR_TGFM;

  /* DMA configuration:
     Descriptor chains pointers.*/
  ETH->DMARDLAR = (uint32_t)__eth_desc_rx;
  ETH->DMATDLAR = (uint32_t)__eth_desc_tx;

  /* Enabling required interrupt sources.*/
  ETH->DMASR    = ETH->DMASR;
  ETH->DMAIER   = ETH_DMAIER_NISE | ETH_DMAIER_RIE | ETH_DMAIER_TIE;

  /* DMA general settings.*/
  ETH->DMABMR   = ETH_DMABMR_AAB | ETH_DMABMR_RDP_1Beat | ETH_DMABMR_PBL_1Beat;

  /* Transmit FIFO flush.*/
  ETH->DMAOMR   = ETH_DMAOMR_FTF;
  while (ETH->DMAOMR & ETH_DMAOMR_FTF)
    ;

  /* DMA final configuration and start.*/
  ETH->DMAOMR   = ETH_DMAOMR_DTCEFD | ETH_DMAOMR_RSF | ETH_DMAOMR_TSF |
                  ETH_DMAOMR_ST | ETH_DMAOMR_SR;
}

/**
 * @brief   Deactivates the MAC peripheral.
 *
 * @param[in] macp      pointer to the @p MACDriver object
 *
 * @notapi
 */
void mac_lld_stop(MACDriver *macp) {

  if (macp->state != MAC_STOP) {
#if CH32H417_MAC_ETH1_CHANGE_PHY_STATE
    /* PHY in power down mode until the driver will be restarted.*/
    mii_write(macp, MII_BMCR, mii_read(macp, MII_BMCR) | BMCR_PDOWN);
#endif

    /* MAC and DMA stopped.*/
    ETH->MACCR    = 0;
    ETH->DMAOMR   = 0;
    ETH->DMAIER   = 0;
    ETH->DMASR    = ETH->DMASR;

    /* MAC clocks stopped.*/
    RCC_HBPeriphClockCmd(RCC_HBPeriph_ETH, DISABLE);

#if CH32H417_MAC_ENABLE_ETH_PLL
    RCC_ETH_PLLCmd(DISABLE);
#endif

    /* ISR vector disabled.*/
    NVIC_DisableIRQ(ETH_IRQn);
  }
}

/**
 * @brief   Returns a transmission descriptor.
 *
 * @notapi
 */
msg_t mac_lld_get_transmit_descriptor(MACDriver *macp,
                                      MACTransmitDescriptor *tdp) {
  ch32_eth_tx_descriptor_t *tdes;

  if (!macp->link_up)
    return MSG_TIMEOUT;

  /* Get Current TX descriptor.*/
  tdes = macp->txptr;

  /* Ensure that descriptor isn't owned by the Ethernet DMA or locked by
     another thread.*/
  if (tdes->tdes0 & (CH32_TDES0_OWN | CH32_TDES0_LOCKED)) {
    return MSG_TIMEOUT;
  }

  /* Marks the current descriptor as locked using a reserved bit.*/
  tdes->tdes0 |= CH32_TDES0_LOCKED;

  /* Next TX descriptor to use.*/
  macp->txptr = (ch32_eth_tx_descriptor_t *)tdes->tdes3;

  /* Set the buffer size and configuration.*/
  tdp->offset   = 0;
  tdp->size     = CH32H417_MAC_BUFFERS_SIZE;
  tdp->physdesc = tdes;

  return MSG_OK;
}

/**
 * @brief   Releases a transmit descriptor and starts the transmission of the
 *          enqueued data as a single frame.
 *
 * @notapi
 */
void mac_lld_release_transmit_descriptor(MACTransmitDescriptor *tdp) {

  osalDbgAssert(!(tdp->physdesc->tdes0 & CH32_TDES0_OWN),
              "attempt to release descriptor already owned by DMA");

  osalSysLock();

  /* Unlocks the descriptor and returns it to the DMA engine.*/
  tdp->physdesc->tdes1 = tdp->offset;
  tdp->physdesc->tdes0 = CH32_TDES0_CIC(CH32H417_MAC_IP_CHECKSUM_OFFLOAD) |
                         CH32_TDES0_IC | CH32_TDES0_LS | CH32_TDES0_FS |
                         CH32_TDES0_TCH | CH32_TDES0_OWN;

  /* Wait for the write to tdes0 to go through before resuming the DMA.*/
  CH32_DSB();

  /* If the DMA engine is stalled then a restart request is issued.*/
  if ((ETH->DMASR & ETH_DMASR_TPS) == ETH_DMASR_TPS_Suspended) {
    ETH->DMASR   = ETH_DMASR_TBUS;
    ETH->DMATPDR = ETH_DMASR_TBUS; /* Any value is OK.*/
  }

  osalSysUnlock();
}

/**
 * @brief   Returns a receive descriptor.
 *
 * @notapi
 */
msg_t mac_lld_get_receive_descriptor(MACDriver *macp,
                                     MACReceiveDescriptor *rdp) {
  ch32_eth_rx_descriptor_t *rdes;

  /* Get Current RX descriptor.*/
  rdes = macp->rxptr;

  /* Iterates through received frames until a valid one is found, invalid
     frames are discarded.*/
  while (!(rdes->rdes0 & CH32_RDES0_OWN)) {
    if (!(rdes->rdes0 & (CH32_RDES0_AFM | CH32_RDES0_ES))
#if CH32H417_MAC_IP_CHECKSUM_OFFLOAD
        && (rdes->rdes0 & CH32_RDES0_FT)
        && !(rdes->rdes0 & (CH32_RDES0_IPHCE | CH32_RDES0_PCE))
#endif
        && (rdes->rdes0 & CH32_RDES0_FS) && (rdes->rdes0 & CH32_RDES0_LS)) {
      /* Found a valid one.*/
      rdp->offset   = 0;
      rdp->size     = ((rdes->rdes0 & CH32_RDES0_FL_MASK) >> 16) - 4;
      rdp->physdesc = rdes;
      macp->rxptr   = (ch32_eth_rx_descriptor_t *)rdes->rdes3;

      return MSG_OK;
    }
    /* Invalid frame found, purging.*/
    rdes->rdes0 = CH32_RDES0_OWN;
    rdes = (ch32_eth_rx_descriptor_t *)rdes->rdes3;
  }

  /* Next descriptor to check.*/
  macp->rxptr = rdes;

  return MSG_TIMEOUT;
}

/**
 * @brief   Releases a receive descriptor.
 *
 * @notapi
 */
void mac_lld_release_receive_descriptor(MACReceiveDescriptor *rdp) {

  osalDbgAssert(!(rdp->physdesc->rdes0 & CH32_RDES0_OWN),
              "attempt to release descriptor already owned by DMA");

  osalSysLock();

  /* Give buffer back to the Ethernet DMA.*/
  rdp->physdesc->rdes0 = CH32_RDES0_OWN;

  /* Wait for the write to rdes0 to go through before resuming the DMA.*/
  CH32_DSB();

  /* If the DMA engine is stalled then a restart request is issued.*/
  if ((ETH->DMASR & ETH_DMASR_RPS) == ETH_DMASR_RPS_Suspended) {
    ETH->DMASR   = ETH_DMASR_RBUS;
    ETH->DMARPDR = ETH_DMASR_RBUS; /* Any value is OK.*/
  }

  osalSysUnlock();
}

/**
 * @brief   Updates and returns the link status.
 *
 * @notapi
 */
bool mac_lld_poll_link_status(MACDriver *macp) {
  uint32_t maccr, bmsr, bmcr;

  maccr = ETH->MACCR;

  /* PHY CR and SR registers read.*/
  (void)mii_read(macp, MII_BMSR);
  bmsr = mii_read(macp, MII_BMSR);
  bmcr = mii_read(macp, MII_BMCR);

  /* Check on auto-negotiation mode.*/
  if (bmcr & BMCR_ANENABLE) {
    uint32_t lpa;

    /* Auto-negotiation must be finished without faults and link established.*/
    if ((bmsr & (BMSR_LSTATUS | BMSR_RFAULT | BMSR_ANEGCOMPLETE)) !=
        (BMSR_LSTATUS | BMSR_ANEGCOMPLETE))
      return macp->link_up = false;

    /* Auto-negotiation enabled, checks the LPA register.*/
    lpa = mii_read(macp, MII_LPA);

    /* Check on link speed.*/
    if (lpa & (LPA_100HALF | LPA_100FULL | LPA_100BASE4))
      maccr |= ETH_MACCR_FES;
    else
      maccr &= ~ETH_MACCR_FES;

    /* Check on link mode.*/
    if (lpa & (LPA_10FULL | LPA_100FULL))
      maccr |= ETH_MACCR_DM;
    else
      maccr &= ~ETH_MACCR_DM;
  }
  else {
    /* Link must be established.*/
    if (!(bmsr & BMSR_LSTATUS))
      return macp->link_up = false;

    /* Check on link speed.*/
    if (bmcr & BMCR_SPEED100)
      maccr |= ETH_MACCR_FES;
    else
      maccr &= ~ETH_MACCR_FES;

    /* Check on link mode.*/
    if (bmcr & BMCR_FULLDPLX)
      maccr |= ETH_MACCR_DM;
    else
      maccr &= ~ETH_MACCR_DM;
  }

  /* Changes the mode in the MAC.*/
  ETH->MACCR = maccr;

  /* Returns the link status.*/
  return macp->link_up = true;
}

/**
 * @brief   Writes to a transmit descriptor's stream.
 *
 * @notapi
 */
size_t mac_lld_write_transmit_descriptor(MACTransmitDescriptor *tdp,
                                         uint8_t *buf,
                                         size_t size) {

  osalDbgAssert(!(tdp->physdesc->tdes0 & CH32_TDES0_OWN),
              "attempt to write descriptor already owned by DMA");

  if (size > tdp->size - tdp->offset)
    size = tdp->size - tdp->offset;

  if (size > 0) {
    memcpy((uint8_t *)(tdp->physdesc->tdes2) + tdp->offset, buf, size);
    tdp->offset += size;
  }
  return size;
}

/**
 * @brief   Reads from a receive descriptor's stream.
 *
 * @notapi
 */
size_t mac_lld_read_receive_descriptor(MACReceiveDescriptor *rdp,
                                       uint8_t *buf,
                                       size_t size) {

  osalDbgAssert(!(rdp->physdesc->rdes0 & CH32_RDES0_OWN),
              "attempt to read descriptor already owned by DMA");

  if (size > rdp->size - rdp->offset)
    size = rdp->size - rdp->offset;

  if (size > 0) {
    memcpy(buf, (uint8_t *)(rdp->physdesc->rdes2) + rdp->offset, size);
    rdp->offset += size;
  }
  return size;
}

#if MAC_USE_ZERO_COPY || defined(__DOXYGEN__)
/**
 * @brief   Returns a pointer to the next transmit buffer in the descriptor
 *          chain.
 *
 * @notapi
 */
uint8_t *mac_lld_get_next_transmit_buffer(MACTransmitDescriptor *tdp,
                                          size_t size,
                                          size_t *sizep) {

  if (tdp->offset == 0) {
    *sizep      = tdp->size;
    tdp->offset = size;
    return (uint8_t *)tdp->physdesc->tdes2;
  }
  *sizep = 0;
  return NULL;
}

/**
 * @brief   Returns a pointer to the next receive buffer in the descriptor
 *          chain.
 *
 * @notapi
 */
const uint8_t *mac_lld_get_next_receive_buffer(MACReceiveDescriptor *rdp,
                                               size_t *sizep) {

  if (rdp->size > 0) {
    *sizep      = rdp->size;
    rdp->offset = rdp->size;
    rdp->size   = 0;
    return (uint8_t *)rdp->physdesc->rdes2;
  }
  *sizep = 0;
  return NULL;
}
#endif /* MAC_USE_ZERO_COPY */

#endif /* HAL_USE_MAC */

/** @} */
