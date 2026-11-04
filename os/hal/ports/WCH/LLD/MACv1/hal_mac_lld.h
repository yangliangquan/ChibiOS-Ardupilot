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
 * @file    MACv1/hal_mac_lld.h
 * @brief   CH32H417 low level MAC driver header.
 *
 * @details Ethernet on the CH32H417 (Synopsys DWC Ether MAC, F4-style
 *          register set: MAC, MMC, PTP and DMA register blocks). The
 *          application/board must provide BOARD_PHY_ID (PHY scan) or
 *          BOARD_PHY_ADDRESS.
 *
 * @addtogroup MAC
 * @{
 */

#ifndef HAL_MAC_LLD_H
#define HAL_MAC_LLD_H

#if HAL_USE_MAC || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   This implementation supports the zero-copy mode API.
 */
#define MAC_SUPPORTS_ZERO_COPY      TRUE

/**
 * @name    RDES0 constants
 * @{
 */
#define CH32_RDES0_OWN             0x80000000
#define CH32_RDES0_AFM             0x40000000
#define CH32_RDES0_FL_MASK         0x3FFF0000
#define CH32_RDES0_ES              0x00008000
#define CH32_RDES0_DESERR          0x00004000
#define CH32_RDES0_SAF             0x00002000
#define CH32_RDES0_LE              0x00001000
#define CH32_RDES0_OE              0x00000800
#define CH32_RDES0_VLAN            0x00000400
#define CH32_RDES0_FS              0x00000200
#define CH32_RDES0_LS              0x00000100
#define CH32_RDES0_IPHCE           0x00000080
#define CH32_RDES0_LCO             0x00000040
#define CH32_RDES0_FT              0x00000020
#define CH32_RDES0_RWT             0x00000010
#define CH32_RDES0_RE              0x00000008
#define CH32_RDES0_DE              0x00000004
#define CH32_RDES0_CE              0x00000002
#define CH32_RDES0_PCE             0x00000001
/** @} */

/**
 * @name    RDES1 constants
 * @{
 */
#define CH32_RDES1_DIC             0x80000000
#define CH32_RDES1_RBS2_MASK       0x1FFF0000
#define CH32_RDES1_RER             0x00008000
#define CH32_RDES1_RCH             0x00004000
#define CH32_RDES1_RBS1_MASK       0x00001FFF
/** @} */

/**
 * @name    TDES0 constants
 * @{
 */
#define CH32_TDES0_OWN             0x80000000
#define CH32_TDES0_IC              0x40000000
#define CH32_TDES0_LS              0x20000000
#define CH32_TDES0_FS              0x10000000
#define CH32_TDES0_DC              0x08000000
#define CH32_TDES0_DP              0x04000000
#define CH32_TDES0_TTSE            0x02000000
#define CH32_TDES0_LOCKED          0x01000000 /* NOTE: Pseudo flag.        */
#define CH32_TDES0_CIC_MASK        0x00C00000
#define CH32_TDES0_CIC(n)          ((n) << 22)
#define CH32_TDES0_TER             0x00200000
#define CH32_TDES0_TCH             0x00100000
#define CH32_TDES0_TTSS            0x00020000
#define CH32_TDES0_IHE             0x00010000
#define CH32_TDES0_ES              0x00008000
#define CH32_TDES0_JT              0x00004000
#define CH32_TDES0_FF              0x00002000
#define CH32_TDES0_IPE             0x00001000
#define CH32_TDES0_LCA             0x00000800
#define CH32_TDES0_NC              0x00000400
#define CH32_TDES0_LCO             0x00000200
#define CH32_TDES0_EC              0x00000100
#define CH32_TDES0_VF              0x00000080
#define CH32_TDES0_CC_MASK         0x00000078
#define CH32_TDES0_ED              0x00000004
#define CH32_TDES0_UF              0x00000002
#define CH32_TDES0_DB              0x00000001
/** @} */

/**
 * @name    TDES1 constants
 * @{
 */
#define CH32_TDES1_TBS2_MASK       0x1FFF0000
#define CH32_TDES1_TBS1_MASK       0x00001FFF
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   Number of available transmit buffers.
 */
#if !defined(CH32H417_MAC_TRANSMIT_BUFFERS) || defined(__DOXYGEN__)
#define CH32H417_MAC_TRANSMIT_BUFFERS      2
#endif

/**
 * @brief   Number of available receive buffers.
 */
#if !defined(CH32H417_MAC_RECEIVE_BUFFERS) || defined(__DOXYGEN__)
#define CH32H417_MAC_RECEIVE_BUFFERS       4
#endif

/**
 * @brief   Maximum supported frame size.
 */
#if !defined(CH32H417_MAC_BUFFERS_SIZE) || defined(__DOXYGEN__)
#define CH32H417_MAC_BUFFERS_SIZE          1536
#endif

/**
 * @brief   PHY detection timeout (scans before failure), 0 = single scan.
 */
#if !defined(CH32H417_MAC_PHY_TIMEOUT) || defined(__DOXYGEN__)
#define CH32H417_MAC_PHY_TIMEOUT           100
#endif

/**
 * @brief   Change the PHY power state inside the driver.
 */
#if !defined(CH32H417_MAC_ETH1_CHANGE_PHY_STATE) || defined(__DOXYGEN__)
#define CH32H417_MAC_ETH1_CHANGE_PHY_STATE TRUE
#endif

/**
 * @brief   Enable the dedicated ETH PLL and the 125MHz ETH clock
 *          (RCC_ETH125MSource_PLLCLK) during mac_lld_start().
 */
#if !defined(CH32H417_MAC_ENABLE_ETH_PLL) || defined(__DOXYGEN__)
#define CH32H417_MAC_ENABLE_ETH_PLL         TRUE
#endif

/**
 * @brief   Enable the RGMII pin interface (gigabit PHY wiring).
 * @note    RM pin mux group, see RM 9.2.11.5/RCC_RGMIIcmd.
 */
#if !defined(CH32H417_MAC_RGMII) || defined(__DOXYGEN__)
#define CH32H417_MAC_RGMII                  FALSE
#endif

/**
 * @brief   MII management clock divider (MACMIIAR.CR field).
 * @note    Vendor header exposes the F1 values (Div42/16/26). The MDC
 *          clock comes from the Ethernet clock domain (ETHCLK_125M,
 *          RM 3.3.5), not from HCLK: Div42 yields MDC=125MHz/84
 *          (1.49MHz) which is within the usual 2.5MHz PHY limit.
 *          (RM ch31 has no register table for MACMIIAR).
 */
#if !defined(CH32H417_MAC_MDC_CR) || defined(__DOXYGEN__)
#define CH32H417_MAC_MDC_CR                 ETH_MACMIIAR_CR_Div42
#endif

/**
 * @brief   IP checksum offload (0..3, see STM32 MAC notes).
 */
#if !defined(CH32H417_MAC_IP_CHECKSUM_OFFLOAD) || defined(__DOXYGEN__)
#define CH32H417_MAC_IP_CHECKSUM_OFFLOAD   0
#endif
/** @} */

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Type of an Ethernet receive descriptor.
 */
typedef struct {
  volatile uint32_t             rdes0;
  volatile uint32_t             rdes1;
  volatile uint32_t             rdes2;
  volatile uint32_t             rdes3;
} ch32_eth_rx_descriptor_t;

/**
 * @brief   Type of an Ethernet transmit descriptor.
 */
typedef struct {
  volatile uint32_t             tdes0;
  volatile uint32_t             tdes1;
  volatile uint32_t             tdes2;
  volatile uint32_t             tdes3;
} ch32_eth_tx_descriptor_t;

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Low level fields of the MAC driver structure.
 */
#define mac_lld_driver_fields                                               \
  /* Link status flag.*/                                                    \
  bool                          link_up;                                    \
  /* PHY address (pre shifted).*/                                           \
  uint32_t                      phyaddr;                                    \
  /* Receive next frame pointer.*/                                          \
  ch32_eth_rx_descriptor_t      *rxptr;                                     \
  /* Transmit next frame pointer.*/                                         \
  ch32_eth_tx_descriptor_t      *txptr;

/**
 * @brief   Low level fields of the MAC configuration structure.
 */
#define mac_lld_config_fields                                               \
  /* MAC address.*/                                                         \
  const uint8_t                 *mac_address;

/**
 * @brief   Low level fields of the MAC transmit descriptor structure.
 */
#define mac_lld_transmit_descriptor_fields                                  \
  /* Pointer to the physical descriptor.*/                                 \
  ch32_eth_tx_descriptor_t      *physdesc;

/**
 * @brief   Low level fields of the MAC receive descriptor structure.
 */
#define mac_lld_receive_descriptor_fields                                   \
  /* Pointer to the physical descriptor.*/                                 \
  ch32_eth_rx_descriptor_t      *physdesc;

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if !defined(__DOXYGEN__)
extern MACDriver ETHD1;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void mii_write(MACDriver *macp, uint32_t reg, uint32_t value);
  uint32_t mii_read(MACDriver *macp, uint32_t reg);
  bool mac_lld_init(void);
  void mac_lld_start(MACDriver *macp);
  void mac_lld_stop(MACDriver *macp);
  msg_t mac_lld_get_transmit_descriptor(MACDriver *macp,
                                        MACTransmitDescriptor *tdp);
  void mac_lld_release_transmit_descriptor(MACTransmitDescriptor *tdp);
  msg_t mac_lld_get_receive_descriptor(MACDriver *macp,
                                       MACReceiveDescriptor *rdp);
  void mac_lld_release_receive_descriptor(MACReceiveDescriptor *rdp);
  bool mac_lld_poll_link_status(MACDriver *macp);
  size_t mac_lld_write_transmit_descriptor(MACTransmitDescriptor *tdp,
                                           uint8_t *buf,
                                           size_t size);
  size_t mac_lld_read_receive_descriptor(MACReceiveDescriptor *rdp,
                                         uint8_t *buf,
                                         size_t size);
#if MAC_USE_ZERO_COPY
  uint8_t *mac_lld_get_next_transmit_buffer(MACTransmitDescriptor *tdp,
                                            size_t size,
                                            size_t *sizep);
  const uint8_t *mac_lld_get_next_receive_buffer(MACReceiveDescriptor *rdp,
                                                 size_t *sizep);
#endif /* MAC_USE_ZERO_COPY */
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_MAC */

#endif /* HAL_MAC_LLD_H */

/** @} */
