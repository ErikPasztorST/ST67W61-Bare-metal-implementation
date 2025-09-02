/**
  ******************************************************************************
  * @file    spi_iface.c
  * @brief   SPI bus interface implementation
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

/* Includes ------------------------------------------------------------------*/
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "spi_iface.h"
#include "spi_port.h"

#include "stm32g0xx_hal.h"

#define SPI_HEADER_MAGIC_CODE 0x55AA
#define SPI_MIN_BUFFER_LEN 8

/* private functions */
spi_iface_cmd_type_t spi_iface_get_cmd_type(const char *cmd);
int32_t wait_for_rdy(uint32_t timeout);
int32_t spi_iface_rx(uint16_t len);
int32_t spi_iface_txRx(const void *tx_data, uint16_t data_len, void **tx, void **rx, void **rx_requested);
void fill_buffer_header(void* buffer, uint16_t data_len);
void fill_buffer_data(void* buffer, const void* data, uint16_t data_len);

/* Used to signalize ongoing communication.
 * If 0, no communication ongoing, RDY high is treated as out-of-band report. */
volatile uint8_t spi_iface_lock = 1;
/* Used to signalize incoming report messages to the application layer.
 * The application should then call spi_iface_receive_report() when convenient. */
wifi_iface_report_cb_t wifi_iface_report_cb = NULL;

/* public functions */

/* initialize the NCP, wait for RDY and verify first AT command
 * return: 0 or (negative) error code
 * */
int32_t spi_iface_init(wifi_iface_report_cb_t cb)
{
    int32_t ret = 0;
    void *tx = NULL, *rx = NULL, *rx_requested = NULL;

	wifi_iface_report_cb = (wifi_iface_report_cb_t)cb;
	spi_iface_lock = 1;

	spi_port_init();

	/* wait for the state of the slave data ready pin
	 * expect to receive a request for RX 9 bytes
	 * receive 'ready'*/
	if(wait_for_rdy(SPI_IFACE_TIMEOUT_INIT_MS) < 0) {ret = -1; goto _err;}

	ret = spi_iface_txRx(NULL, 0, &tx, &rx, &rx_requested);
	if (ret < 0)
	{
		printf("Err: spi_iface_txRx = %d\n", (int)ret);
		goto _err;
	}

#if (SPI_IFACE_LOG_INCOMING > 0)
        printf("IN: ");
        printf("%.*s", (int)ret, (char *)rx_requested);
#endif

	ret = strncmp((const char *)rx_requested, "\r\nready\r\n", 9);
	if (ret < 0)
	{
		printf("Err: unexpected return from NCP (ready)\n");
		goto _err;
	}

	/* With delay, the response to 'AT' is incorrect but after that works ok.
	 * Without, it gets stuck after. */
	HAL_Delay(500);

	/* send first 'AT' command */
	ret = spi_iface_txRx((const void *) "AT\r\n", 4, &tx, &rx, &rx_requested);
	if (ret < 0)
	{
		printf("Err: spi_iface_txRx = %d\n", (int)ret);
		goto _err;
	}

#if (SPI_IFACE_LOG_OUTGOING > 0)
    printf("OUT: AT\r\n");
#endif

	/* wait for NCP to initialize response */
    spi_iface_lock = 1;
    if(wait_for_rdy(SPI_IFACE_TIMEOUT_DEF_MS) < 0) {ret = -1; goto _err;}

	ret = spi_iface_txRx(NULL, 0, &tx, &rx, &rx_requested);
	if (ret < 0)
	{
		printf("Err: spi_iface_txRx = %d\n", (int)ret);
		goto _err;
	}

#if (SPI_IFACE_LOG_INCOMING > 0)
        printf("IN: ");
        printf("%.*s", (int)ret, (char *)rx_requested);
#endif

	ret = strncmp((const char *)rx_requested, "\r\nOK\r\n", 6);
	if (ret)
	{
		printf("Err: unexpected return from NCP (AT)\n");
		goto _err;
	}


_err:
	spi_iface_lock = 0;

	free(tx);
	free(rx);
	free(rx_requested);

	return ret;
}

/*
 * Response is allocated by function and freed by caller.
 * Response is a combination of partial responses from NCP separated by "\r\n",
 * with NULL termination.
 *
 * return: response length or (negative) error code
 * */
int32_t spi_iface_send(const char *cmd, char **resp)
{
    int32_t ret = -1;
    void *tx = NULL, *rx = NULL, *rx_req = NULL, *cmd_ext = NULL;
    int32_t rx_len = 0, total_resp_len = 0;
    char *resp_buf = NULL;

    spi_iface_cmd_type_t cmd_type = spi_iface_get_cmd_type(cmd);
    uint16_t cmd_len = strlen(cmd);

#if (SPI_IFACE_ADD_TRAILING_RN > 0)
    cmd_ext = calloc(cmd_len + 2, 1);
#else
    cmd_ext = calloc(cmd_len, 1);
#endif
    memcpy(cmd_ext, cmd, cmd_len);
#if (SPI_IFACE_ADD_TRAILING_RN > 0)
    ((uint8_t*)cmd_ext)[cmd_len] = '\r';
	((uint8_t*)cmd_ext)[cmd_len + 1] = '\n';
	cmd_len = cmd_len + 2;
#endif

    if (cmd_type == CMD_QUERY)
    {
    	// append two bytes to the end
		// otherwise, it is ignored by ncp
		// value does not seem to matter, this based on u5 example
		cmd_ext = realloc(cmd_ext, cmd_len + 2);
		((uint8_t*)cmd_ext)[cmd_len] = 0x88;
		((uint8_t*)cmd_ext)[cmd_len + 1] = 0x88;
		cmd_len = cmd_len + 2;
    }
    else if (cmd_type == CMD_SET)
    {
    	// append two bytes to the end
		// otherwise, it is ignored by ncp
		// value does not seem to matter, this based on u5 example
		cmd_ext = realloc(cmd_ext, cmd_len + 3);
		((uint8_t*)cmd_ext)[cmd_len] = 0x88;
		((uint8_t*)cmd_ext)[cmd_len + 1] = 0x88;
		((uint8_t*)cmd_ext)[cmd_len + 2] = 0x88;
		cmd_len = cmd_len + 3;
    }
    else if (cmd_type == CMD_EXECUTE)
    {
    }
    else return -1;

    spi_iface_lock = 1;

    // send command
    ret = spi_iface_txRx(cmd_ext, cmd_len, &tx, &rx, &rx_req);
    if (ret < 0) goto _err;

#if (SPI_IFACE_LOG_OUTGOING > 0)
    printf("OUT: ");
    printf("%.*s", cmd_len, (char *)cmd_ext);
#endif

    // first response is usually "busy", skip all "busy" responses
    while (1) {
    	if(wait_for_rdy(SPI_IFACE_TIMEOUT_DEF_MS) < 0) {ret = -1; goto _err;}

        rx_len = spi_iface_txRx(NULL, 0, &tx, &rx, &rx_req);

        if (rx_len < 0) { ret = rx_len; goto _err; }
        if (rx_len >= 8 && memcmp(rx_req, "\r\nbusy p...\r\n", 8) == 0) {
#if (SPI_IFACE_LOG_INCOMING > 0)
			printf("IN: busy p...\n");
#endif
            continue;
        }
        break;
    }

    // Collect responses until "OK" is received
    // Usually, it's data in one msg, "OK" is second msg
    while (1) {
        if ((rx_len >= 4 && memcmp((char*)rx_req + rx_len - 4, "OK\r\n", 4) == 0) ||
        	(rx_len == 9 && memcmp((char*)rx_req, "\r\nready\r\n", 5) == 0)) {
            resp_buf = realloc(resp_buf, total_resp_len + rx_len + 1);
            memcpy(resp_buf + total_resp_len, rx_req, rx_len);
            total_resp_len += rx_len;
            break;
        }

        resp_buf = realloc(resp_buf, total_resp_len + rx_len + 1);
        memcpy(resp_buf + total_resp_len, rx_req, rx_len);
        total_resp_len += rx_len;

        if(wait_for_rdy(SPI_IFACE_TIMEOUT_DEF_MS) < 0) {ret = -1; goto _err;}

        rx_len = spi_iface_txRx(NULL, 0, &tx, &rx, &rx_req);
        if (rx_len < 0) { ret = rx_len; goto _err; }
    }

    // Null-terminate and return response
    if (total_resp_len > 0) {
        resp_buf[total_resp_len] = 0;
        *resp = resp_buf;
    }

    ret = total_resp_len;

#if (SPI_IFACE_LOG_INCOMING > 0)
	printf("IN: ");
	printf("%s", (char *)*resp);
#endif

_err:
	spi_iface_lock = 0;

    free(tx);
    free(rx);
    free(rx_req);
    free(cmd_ext);
    if (ret < 0 && resp_buf) free(resp_buf);
    return ret;
}

/*
 * Report is allocated by function and freed by caller.
 * Response is the response from NCP, with NULL termination.
 *
 * return: response length or (negative) error code
 * */
int32_t spi_iface_receive_report(char **report)
{
    int32_t ret = -1, rx_len = 0;
    void *tx = NULL, *rx = NULL, *rx_req = NULL;

    spi_iface_lock = 1;

    rx_len = spi_iface_txRx(NULL, 0, &tx, &rx, &rx_req);
    if (rx_len < 0) { ret = rx_len; goto _err; }

    *report = calloc(rx_len + 1, 1);
    memcpy(*report, rx_req, rx_len);
    (*report)[rx_len] = 0;

    ret = rx_len + 1;

#if (SPI_IFACE_LOG_INCOMING > 0)
    printf("IN: ");
    printf("%s", (char *)*report);
#endif

_err:
    spi_iface_lock = 0;

    free(tx);
    free(rx);
    free(rx_req);
    return ret;
}

void spi_iface_ncp_ready_high(void)
{
	if (spi_iface_lock == 0)
	{
		wifi_iface_report_cb();
	}
}

void spi_iface_ncp_ready_low(void)
{
}

/* private function definitions */

int32_t wait_for_rdy(uint32_t timeout)
{
	uint32_t tickstart = HAL_GetTick();
	while (spi_port_is_ready() == 0)
	{
		if ((HAL_GetTick() - tickstart) > timeout)
		{
			return -1;
		}
	}

	return 0;
}
/*
 * tx_data - (char *) data to be sent
 * data_len - length of the data
 *
 * If data_len is 0, fills the TX buffer only with mandatory flags.
 * Otherwise, the buffer is extended and data added to the end.
 *
 * return: the length of received data or (negative) error code
 * */
int32_t spi_iface_txRx(const void *tx_data, uint16_t data_len, void **tx, void **rx, void **rx_requested)
{
    uint16_t buffer_len, rx_request_len;
    int32_t ret;

    if (*tx != NULL) { free(*tx); *tx = NULL; }
    if (*rx != NULL) { free(*rx); *rx = NULL; }
    if (*rx_requested != NULL) { free(*rx_requested); *rx_requested = NULL; }

    if (data_len == 0) buffer_len = SPI_MIN_BUFFER_LEN;
    else buffer_len = SPI_MIN_BUFFER_LEN + data_len;

    *tx = calloc(buffer_len, 1);
    *rx = calloc(buffer_len, 1);


    if (*tx == NULL)
    {
        ret = -1;
        goto _err;
    }
    if (*rx == NULL)
    {
        ret = -2;
        goto _err;
    }

    fill_buffer_header(*tx, data_len);
    if (data_len != 0) fill_buffer_data(*tx, tx_data, data_len);

    spi_port_set_cs(1);
    while (spi_port_is_ready() == 0) {}

    ret = spi_port_transfer(*tx, *rx, buffer_len);

    if (ret != 0)
    {
        ret = -3;
        goto _err;
    }

    rx_request_len = ((uint8_t*)(*rx))[2];
    if (rx_request_len > 0)
    {
        *rx_requested = calloc(rx_request_len, 1);

        if (*rx_requested == NULL)
        {
            ret = -4;
            goto _err;
        }

        ret = spi_port_transfer(NULL, *rx_requested, rx_request_len);
        if (ret != 0)
        {
            ret = -5;
            goto _err;
        }
    }

    ret = rx_request_len;
_err:

    spi_port_set_cs(0);
    return ret;
}

spi_iface_cmd_type_t spi_iface_get_cmd_type(const char *cmd)
{
	spi_iface_cmd_type_t ret = CMD_EXECUTE;

	const char *eq = strchr(cmd, '=');
	const char *q = strchr(cmd, '?');

	if (eq) {
		// Found '='
		ret = CMD_SET;

		// CWQAP command is always EXECUTE even with '='
		if (strncmp(cmd, "AT+CWQAP:", 8) == 0) ret = CMD_EXECUTE;
	} else if (q) {
		// Found '?'
		ret = CMD_QUERY;
	} else {
		// Neither '=' nor '?' found, check for subset of SET without '=' and parameters
		if (strcmp(cmd, "AT+EFUSE-CFM") == 0) ret = CMD_SET;
		else if (strcmp(cmd, "AT+OTAFIN") == 0) ret = CMD_SET;
		else if (strcmp(cmd, "AT+IPERFSTOP") == 0) ret = CMD_SET;
		else if (strcmp(cmd, "AT+SLCLDTIM") == 0) ret = CMD_SET;
		else if (strcmp(cmd, "AT+TWT_SLEEP") == 0) ret = CMD_SET;
	}

	return ret;
}

void fill_buffer_header(void* buffer, uint16_t data_len)
{
    unsigned char* buf = (unsigned char*)buffer;
    buf[0] = SPI_HEADER_MAGIC_CODE & 0xFF;
    buf[1] = (SPI_HEADER_MAGIC_CODE >> 8) & 0xFF;
    buf[2] = (unsigned char)data_len;
}

void fill_buffer_data(void* buffer, const void* data, uint16_t data_len)
{
	unsigned char* buf = (unsigned char*)buffer;
	memcpy(buf + SPI_MIN_BUFFER_LEN, data, data_len);
}
