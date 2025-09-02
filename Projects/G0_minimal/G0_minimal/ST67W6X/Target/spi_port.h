/**
  ******************************************************************************
  * @file    spi_port.h
  * @brief   SPI bus interface porting layer
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SPI_PORT_H
#define SPI_PORT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include <inttypes.h>
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/
#define NCP_SPI_HANDLE hspi1
#define SPI_TRANSMIT_TIMEOUT_MS 5000
#define SPI_PHY_LOG_CHARS 0

/* Exported macro ------------------------------------------------------------*/
#ifdef __ICCARM__
#define memcpy spi_port_memcpy
#endif /* __ICCARM__ */

/* Exported functions ------------------------------------------------------- */
/**
  * @brief  Initialize SPI port
  * @param  transaction_complete_cb: Callback function to be called when a DMA transaction is completed
  * @retval 0 if successful, -1 otherwise
  */
int32_t spi_port_init();

/**
  * @brief  De-Initialize SPI port
  * @retval 0 if successful, -1 otherwise
  */
int32_t spi_port_deinit(void);

/**
  * @brief  Execute SPI transaction in blocking mode (polling) with timeout. Can be TxRX or Rx only
  * @retval 0 if successful, -1 otherwise
  */
int32_t spi_port_transfer(void *tx_buf, void *rx_buf, uint16_t len);

/**
  * @brief  Check if NCP requires to send a new data packet
  * @retval 1 if ready, 0 otherwise
  */
int32_t spi_port_is_ready(void);

/**
  * @brief  Enable or disable the SPI Chip Select (CS) line
  * @param  state: 1 to set the CS line high, 0 to set it low
  * @retval 0 if successful, -1 otherwise
  */
int32_t spi_port_set_cs(int32_t state);

/**
  * @brief  Set an ITM event
  * @retval 1 if high, 0 if low
  */
uint32_t spi_port_itm(uint32_t ch);

#ifdef __ICCARM__
/**
  * @brief  Copy memory from source to destination
  * @retval Destination address
  */
void *spi_port_memcpy(void *dest, const void *src, unsigned int len);
#endif /* __ICCARM__ */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SPI_PORT_H */
