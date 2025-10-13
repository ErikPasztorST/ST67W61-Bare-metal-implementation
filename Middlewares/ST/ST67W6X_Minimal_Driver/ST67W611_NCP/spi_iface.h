/**
  ******************************************************************************
  * @file    spi_iface.h
  * @brief   SPI bus interface definition
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
#ifndef SPI_IFACE_H
#define SPI_IFACE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define SPI_IFACE_LOG_OUTGOING 0
#define SPI_IFACE_LOG_DATA_OUT 0
#define SPI_IFACE_LOG_INCOMING 0

#define SPI_IFACE_RESP_INCL_OK_ERR 0
#define SPI_IFACE_ADD_TRAILING_RN 1
#define SPI_IFACE_TIMEOUT_DEF_MS 10000
#define SPI_IFACE_TIMEOUT_INIT_MS 60000

// used at high frequency SPI (40MHz) to avoid RX-only transaction (which sometimes fails)
#define SPI_IFACE_RXTX_WORKAROUND 1

/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/

typedef void(* wifi_iface_report_cb_t)(char *report, int32_t report_len);

/* Exported constants --------------------------------------------------------*/


/* Exported functions ------------------------------------------------------- */
int32_t spi_iface_init(wifi_iface_report_cb_t cb);
void spi_iface_deinit(void);
int32_t spi_iface_command(const char *cmd, char **resp);
int32_t spi_iface_data(const uint8_t *data, uint32_t data_len, char **resp);
int32_t spi_iface_receive_report(char **report);
void spi_iface_ncp_ready_high(void);
void spi_iface_ncp_ready_low(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SPI_IFACE_H */
