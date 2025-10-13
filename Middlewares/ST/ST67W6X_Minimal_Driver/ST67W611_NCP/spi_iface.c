/**
 ******************************************************************************
 * @file    spi_iface.c
 * @brief   SPI bus interface implementation for communication with NCP
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

/* --- Macros and Constants -------------------------------------------------- */
#define SPI_HEADER_MAGIC_CODE      0x55AA
#define SPI_MIN_BUFFER_LEN        8
#define MAX_REPORTS_DURING_CMD    10

/* --- Private Function Prototypes ------------------------------------------- */
static int32_t spi_iface_txRx(const void *tx_data, uint32_t data_len, void **rx_requested);
static void fill_buffer_header(void *buffer, uint16_t data_len);
static void fill_buffer_data(void *buffer, const void *data, uint16_t data_len, uint16_t pad);
static uint8_t check_report_prefix(const void *rx_req, int32_t rx_len);
static int32_t set_cs(int32_t state);
static int32_t wait_for_rdy_rising(uint32_t timeout);
static uint8_t is_serial_data_cmd(const void *data, uint16_t data_len);

/* --- Global/Static Variables ---------------------------------------------- */
/* Indicates ongoing communication. If 0, RDY high is treated as out-of-band report. */
volatile uint8_t spi_iface_lock = 1;

/* Callback for incoming report messages to the application layer. */
wifi_iface_report_cb_t wifi_iface_report_cb = NULL;

/* Flag for RDY pin rising edge detection. */
uint8_t rdy_rising_edge = 0;

/* List of known report prefixes for parsing incoming reports. */
static const uint8_t num_report_prefixes = 34;
static const char *report_prefixes[] = {
		"+CW:CONNECTING", "+CW:CONNECTED", "+CW:ERROR", "+CW:GOTIP", "+CW:DISCONNECTED",
		"+CW:STA_CONNECTED", "+CW:STA_DISCONNECTED", "+CW:DIST_STA_IP", "+CW:SCAN_DONE",
		"+CIP:", "+LINK_CONN", "+IPD", "+MQTT:CONNECTED", "+MQTT:DISCONNECTED",
		"+MQTT:SUBRECV", "SEND CANCELLED", "+BLE:CONNECTED", "+BLE:DISCONNECTED",
		"+BLE:CONNPARAM", "+BLE:GATTREAD", "+BLE:GATTWRITE", "+BLE:SRV", "+BLE:SRVCHAR",
		"+BLE:INDICATION", "+BLE:NOTIFICATION", "+BLE:NOTIDATA", "+BLE:MTUSIZE",
		"+BLE:PAIRINGFAILED", "+BLE:PAIRINGCOMPLETED", "+BLE:PAIRINGCONFIRM",
		"+BLE:PASSKEYENTRY", "+BLE:PASSKEYDISPLAY", "+BLE:BASLEVEL", "+QUITT"
};

/* List of commands that expect serial data transfer. */
static const uint8_t num_serial_data_cmds = 9;
static const char *serial_data_cmds[] = {
		"AT+EFUSE-W", "AT+FLASH-W", "AT+OTASEND", "AT+CIPSEND", "AT+MQTTPUBRAW",
		"AT+BLEGATTSNTFY", "AT+BLEGATTSIND", "AT+BLEGATTSRD", "AT+BLEGATTCWR"
};

/* --- Public Functions ------------------------------------------------------ */

/**
 * @brief  Initialize the SPI interface and NCP, verify communication.
 * @param  cb: Callback for incoming reports.
 * @retval 0 on success, negative error code on failure.
 */
int32_t spi_iface_init(wifi_iface_report_cb_t cb)
{
	int32_t ret = 0;
	void *rx_requested = NULL;
	rdy_rising_edge = 0;

	wifi_iface_report_cb = cb;
	spi_iface_lock = 1;

	spi_port_init();

	/* Wait for NCP ready signal (RDY high) */
	if (spi_port_wait_for_rdy(1, SPI_IFACE_TIMEOUT_INIT_MS) < 0) { ret = -1; goto _err; }

	/* Receive initial 'ready' message from NCP */
	ret = spi_iface_txRx(NULL, 0, &rx_requested);
	if (ret < 0) {
		printf("Err: spi_iface_txRx = %d\n", (int)ret);
		goto _err;
	}

#if (SPI_IFACE_LOG_INCOMING > 0)
	printf("IN: %.*s", (int)ret, (char *)rx_requested);
#endif

	/* Check for expected ready message */
	ret = strncmp((const char *)rx_requested, "\r\nready\r\n", 9);
	if (ret < 0) {
		printf("Err: unexpected return from NCP (ready)\n");
		goto _err;
	}

	/* Wait for RDY pin to go low before next transfer */
	while (spi_port_is_ready() == 1) {}

	/* Send first 'AT' command to NCP */
#if (SPI_IFACE_LOG_OUTGOING > 0)
	printf("OUT: AT\r\n");
#endif

	char first_at[] = "AT\r\n";
	ret = spi_iface_txRx((const void *)first_at, 4, &rx_requested);
	if (ret < 0) {
		printf("Err: spi_iface_txRx = %d\n", (int)ret);
		goto _err;
	}
#if !defined (USE_HAL_DRIVER)
	LL_mDelay(1);
#else
	HAL_Delay(1);
#endif

	/* Wait for NCP to respond to 'AT' command */
	if (spi_port_wait_for_rdy(1, SPI_IFACE_TIMEOUT_DEF_MS) < 0) { ret = -1; goto _err; }

	ret = spi_iface_txRx(NULL, 0, &rx_requested);
	if (ret < 0) {
		printf("Err: spi_iface_txRx = %d\n", (int)ret);
		goto _err;
	}

#if (SPI_IFACE_LOG_INCOMING > 0)
	printf("IN: %.*s", (int)ret, (char *)rx_requested);
#endif

	/* Check for expected OK response */
	ret = strncmp((const char *)rx_requested, "\r\nOK\r\n", 6);
	if (ret) {
		printf("Err: unexpected return from NCP (AT)\n");
		goto _err;
	}

	_err:
	spi_iface_lock = 0;
	free(rx_requested);
	return ret;
}

void spi_iface_deinit(void)
{
	spi_port_deinit();
}

/**
 * @brief  Send a command to the NCP and wait for response.
 *         Response is allocated and must be freed by caller.
 * @param  cmd: Command string to send.
 * @param  resp: Pointer to response buffer pointer.
 * @retval Response length, or negative error code.
 */
int32_t spi_iface_command(const char *cmd, char **resp)
{
	int32_t ret = -1;
	void *rx_req = NULL, *cmd_ext = NULL;
	int32_t rx_len = 0, total_resp_len = 0;
	char *resp_buf = NULL;
	char *reports[MAX_REPORTS_DURING_CMD] = {0};
	uint8_t report_count = 0;

	uint32_t cmd_len = strlen(cmd);

#if (SPI_IFACE_ADD_TRAILING_RN > 0)
	cmd_ext = calloc(cmd_len + 2, 1);
#else
	cmd_ext = calloc(cmd_len, 1);
#endif
	memcpy(cmd_ext, cmd, cmd_len);
#if (SPI_IFACE_ADD_TRAILING_RN > 0)
	((uint8_t*)cmd_ext)[cmd_len] = '\r';
	((uint8_t*)cmd_ext)[cmd_len + 1] = '\n';
	cmd_len += 2;
#endif

	uint8_t send_data = is_serial_data_cmd(cmd, cmd_len);

	spi_iface_lock = 1;

#if (SPI_IFACE_LOG_OUTGOING > 0)
	printf("OUT: %.*s", cmd_len, (char *)cmd_ext);
#endif

	/* Ensure RDY is low before starting */
	if (spi_port_wait_for_rdy(0, SPI_IFACE_TIMEOUT_DEF_MS) < 0) { ret = -1; goto _err; }
	ret = spi_iface_txRx(cmd_ext, cmd_len, &rx_req);
	if (ret < 0) goto _err;

	ret = 0;

	while (1) {
		/* Wait for RDY rising edge (response ready) */
		if (wait_for_rdy_rising(SPI_IFACE_TIMEOUT_DEF_MS) < 0) { ret = -1; goto _err; }

		rx_len = spi_iface_txRx(NULL, 0, &rx_req);
		if (rx_len < 0) { ret = rx_len; goto _err; }

		/* Handle various response types */
		if (rx_len == 13 && memcmp((char*)rx_req, "\r\nbusy p...\r\n", 13) == 0) continue;
		else if (!send_data && rx_len == 6 && memcmp((char*)rx_req, "\r\nOK\r\n", 6) == 0) {
#if (SPI_IFACE_RESP_INCL_OK_ERR > 0)
			resp_buf = realloc(resp_buf, total_resp_len + rx_len + 1);
			memcpy(resp_buf + total_resp_len, rx_req, rx_len);
			total_resp_len += rx_len;
#endif

			ret = total_resp_len;
			break;
		}
		else if (send_data && rx_len == 9 && memcmp((char*)rx_req, "\r\nOK\r\n\r\n>", 9) == 0) {
#if (SPI_IFACE_RESP_INCL_OK_ERR > 0)
			resp_buf = realloc(resp_buf, total_resp_len + rx_len + 1);
			memcpy(resp_buf + total_resp_len, rx_req, rx_len);
			total_resp_len += rx_len;
#endif

			ret = total_resp_len;
			break;
		}
		else if (send_data && rx_len == 3 && memcmp((char*)rx_req, "\r\n>", 3) == 0) {
#if (SPI_IFACE_RESP_INCL_OK_ERR > 0)
			resp_buf = realloc(resp_buf, total_resp_len + rx_len + 1);
			memcpy(resp_buf + total_resp_len, rx_req, rx_len);
			total_resp_len += rx_len;
#endif

			ret = total_resp_len;
			break;
		}
		else if (rx_len == 9 && memcmp((char*)rx_req, "\r\nERROR\r\n", 9) == 0) {
#if (SPI_IFACE_RESP_INCL_OK_ERR > 0)
			resp_buf = realloc(resp_buf, total_resp_len + rx_len + 1);
			memcpy(resp_buf + total_resp_len, rx_req, rx_len);
			total_resp_len += rx_len;
#endif

			ret = -2;
			break;
		}
		else if (check_report_prefix(rx_req, rx_len)) {
			/* Store report for later callback */
			if (report_count < MAX_REPORTS_DURING_CMD) {
				reports[report_count] = calloc(rx_len + 1, 1);
				memcpy(reports[report_count], rx_req, rx_len);
				reports[report_count][rx_len] = 0;
				report_count++;
			}
		}
		else {
			/* Accumulate response data */
			resp_buf = realloc(resp_buf, total_resp_len + rx_len + 1);
			memcpy(resp_buf + total_resp_len, rx_req, rx_len);
			total_resp_len += rx_len;
		}
	}

	/* Call report callback for each report after the loop */
	for (int i = 0; i < report_count; i++) {
#if (SPI_IFACE_LOG_INCOMING > 0)
		printf("REPORT: %s", reports[i]);
#endif
		wifi_iface_report_cb(reports[i], strlen(reports[i]) + 1);
	}

	/* Null-terminate and return response */
	if (total_resp_len > 0) {
		resp_buf[total_resp_len] = 0;
		*resp = resp_buf;
	}


#if (SPI_IFACE_LOG_INCOMING > 0)
	if (total_resp_len > 0)	printf("IN: %s", (char *)*resp);
#endif

	_err:
	spi_iface_lock = 0;
	free(rx_req);
	free(cmd_ext);
	if (ret < 0 && resp_buf) free(resp_buf);
	return ret;
}

/**
 * @brief  Write data to the NCP after a corresponding command.
 *         Response is allocated and must be freed by caller.
 * @param  data: Data buffer to send.
 * @param  data_len: Length of data.
 * @param  resp: Pointer to response buffer pointer.
 * @retval Response length, or negative error code.
 */
int32_t spi_iface_data(const uint8_t *data, uint32_t data_len, char **resp)
{
	int32_t ret = -1;
	void *rx_req = NULL;
	int32_t rx_len = 0, total_resp_len = 0;
	char *resp_buf = NULL;

	spi_iface_lock = 1;

	/* Ensure RDY is low before starting */
	if (spi_port_wait_for_rdy(0, SPI_IFACE_TIMEOUT_DEF_MS) < 0) { ret = -1; goto _err; }
	/* Send data */
	ret = spi_iface_txRx((const void *)data, data_len, &rx_req);
	if (ret < 0) goto _err;

#if (SPI_IFACE_LOG_DATA_OUT > 0)
	printf("DATA: %.*s", data_len, (char *)data);
#endif

	while (1) {
		/* Wait for RDY rising edge (response ready) */
		if (wait_for_rdy_rising(SPI_IFACE_TIMEOUT_DEF_MS) < 0) { ret = -1; goto _err; }

		rx_len = spi_iface_txRx(NULL, 0, &rx_req);
		if (rx_len < 0) { ret = rx_len; goto _err; }

		/* Check for end of data transfer responses */
		if ((rx_len == 11 && memcmp((char*)rx_req, "\r\nSEND OK\r\n", 11) == 0) ||
				(rx_len == 13 && memcmp((char*)rx_req, "\r\nSEND FAIL\r\n", 13) == 0) ||
				(rx_len == 18 && memcmp((char*)rx_req, "\r\nSEND CANCELLED\r\n", 18) == 0)) {
			resp_buf = realloc(resp_buf, total_resp_len + rx_len + 1);
			memcpy(resp_buf + total_resp_len, rx_req, rx_len);
			total_resp_len += rx_len;
			break;
		}
		else {
			/* Handle incoming report */
			char *report = calloc(rx_len + 1, 1);
			memcpy(report, rx_req, rx_len);
			report[rx_len] = 0;

#if (SPI_IFACE_LOG_INCOMING > 0)
			printf("REPORT: %s", report);
#endif
			wifi_iface_report_cb(report, rx_len);
		}
	}

	/* Null-terminate and return response */
	if (total_resp_len > 0) {
		resp_buf[total_resp_len] = 0;
		*resp = resp_buf;
	}

	ret = total_resp_len;

#if (SPI_IFACE_LOG_INCOMING > 0)
	if (total_resp_len > 0)	printf("IN: %s", (char *)*resp);
#endif

	_err:
	spi_iface_lock = 0;
	free(rx_req);
	if (ret < 0 && resp_buf) free(resp_buf);
	return ret;
}

/**
 * @brief  Receive an out-of-band report (without command).
 *         Report is allocated and must be freed by caller.
 * @param  report: Pointer to report buffer pointer.
 * @retval Report length, or negative error code.
 */
int32_t spi_iface_receive_report(char **report)
{
	int32_t ret = -1, rx_len = 0;
	void *rx_req = NULL;

	rx_len = spi_iface_txRx(NULL, 0, &rx_req);
	if (rx_len < 0) { ret = rx_len; goto _err; }

	*report = calloc(rx_len + 1, 1);
	memcpy(*report, rx_req, rx_len);
	(*report)[rx_len] = 0;

	ret = rx_len + 1;

#if (SPI_IFACE_LOG_INCOMING > 0)
	printf("REPORT: %s", (char *)*report);
#endif

	_err:
	free(rx_req);
	return ret;
}

/**
 * @brief  Handler for NCP RDY pin rising edge (ready high).
 *         If not in communication, receive and process report.
 */
void spi_iface_ncp_ready_high(void)
{
	rdy_rising_edge = 1;

	if (spi_iface_lock == 0) {
		char *report = NULL;
		int32_t ret = spi_iface_receive_report(&report);

		if (ret > 0)
			wifi_iface_report_cb(report, ret);
#if (SPI_IFACE_LOG_INCOMING > 0)
		else
			printf("Report fail\n");
#endif
	}
}

/**
 * @brief  Handler for NCP RDY pin falling edge (ready low).
 *         (Currently unused, placeholder for future use.)
 */
void spi_iface_ncp_ready_low(void)
{
	/* No action required */
}

/* --- Private Function Definitions ------------------------------------------ */

/**
 * @brief  Perform SPI transfer with NCP.
 *         If data_len is 0, only header is sent.
 *         If data_len > 0, data is appended and padded to 4 bytes.
 * @param  tx_data: Data to send.
 * @param  data_len: Length of data to send.
 * @param  rx_requested: Pointer to received data buffer pointer.
 * @retval Length of received data, or negative error code.
 */
static int32_t spi_iface_txRx(const void *tx_data, uint32_t data_len, void **rx_requested)
{
	uint32_t buffer_len, rx_request_len, pad = 0;
	int32_t ret;
	void *tx = NULL, *rx = NULL;

	if (*rx_requested != NULL) { free(*rx_requested); *rx_requested = NULL; }

	if (data_len == 0)
		buffer_len = SPI_MIN_BUFFER_LEN;
	else {
		pad = (4 - (data_len % 4)) % 4;
		buffer_len = SPI_MIN_BUFFER_LEN + data_len + pad;
	}

	tx = calloc(buffer_len, 1);
	rx = calloc(buffer_len, 1);

	if (tx == NULL) { ret = -1; goto _err; }
	if (rx == NULL) { ret = -2; goto _err; }

	fill_buffer_header(tx, data_len);
	if (data_len != 0)
		fill_buffer_data(tx, tx_data, data_len, pad);

	set_cs(1);
	while (spi_port_is_ready() == 0) {}

	ret = spi_port_transfer(tx, rx, buffer_len);

	if (ret != 0) { ret = -3; goto _err; }

	rx_request_len = ((uint8_t*)(rx))[3];
	rx_request_len = (rx_request_len << 8) + ((uint8_t*)(rx))[2];
	if (rx_request_len > 0)
	{
		*rx_requested = calloc(rx_request_len, 1);

#if (SPI_IFACE_RXTX_WORKAROUND > 0)
		void *aux = calloc(rx_request_len, 1);
#endif

		if (*rx_requested == NULL)
		{
			ret = -4;
			goto _err;
		}

		pad = (4 - (rx_request_len % 4)) % 4;

#if (SPI_IFACE_RXTX_WORKAROUND > 0)
		// At high frequency, the RX-only function sometimes fails, so use workaround
		ret = spi_port_transfer(aux, *rx_requested, rx_request_len + pad);
		free(aux);
#else
		ret = spi_port_transfer(NULL, *rx_requested, rx_request_len);
#endif
		if (ret != 0)
		{
			ret = -5;
			goto _err;
		}
	}

	ret = rx_request_len;

	_err:
	set_cs(0);
	free(tx);
	free(rx);
	return ret;
}

/**
 * @brief  Fill the SPI buffer header with magic code and data length.
 * @param  buffer: Pointer to buffer to fill.
 * @param  data_len: Length of data to be sent.
 */
static void fill_buffer_header(void* buffer, uint16_t data_len)
{
	unsigned char* buf = (unsigned char*)buffer;
	buf[0] = SPI_HEADER_MAGIC_CODE & 0xFF;
	buf[1] = (SPI_HEADER_MAGIC_CODE >> 8) & 0xFF;
	buf[2] = data_len & 0xFF;
	buf[3] = (data_len >> 8) & 0xFF;
}

/**
 * @brief  Fill the SPI buffer with data and pad to 4-byte alignment.
 * @param  buffer: Pointer to buffer to fill.
 * @param  data: Pointer to data to copy.
 * @param  data_len: Length of data.
 * @param  pad: Number of padding bytes to add.
 */
static void fill_buffer_data(void* buffer, const void* data, uint16_t data_len, uint16_t pad)
{
	unsigned char* buf = (unsigned char*)buffer;
	memcpy(buf + SPI_MIN_BUFFER_LEN, data, data_len);
	memset(buf + SPI_MIN_BUFFER_LEN + data_len, 0x88, pad);
}

/**
 * @brief  Check if the received data starts with a known report prefix.
 * @param  data: Pointer to received data.
 * @param  data_len: Length of received data.
 * @retval Index+1 of matched prefix, or 0 if no match.
 */
static uint8_t check_report_prefix(const void *data, int32_t data_len)
{
	for (int i = 0; i < num_report_prefixes; i++) {
		size_t len = strlen(report_prefixes[i]);
		if (data_len >= len && memcmp(data, report_prefixes[i], len) == 0)
			return i + 1; // index of matched prefix
	}
	return 0; // no match
}

/**
 * @brief  Check if the command is a serial data command.
 * @param  data: Pointer to command data.
 * @param  data_len: Length of command data.
 * @retval Index+1 of matched command, or 0 if no match.
 */
static uint8_t is_serial_data_cmd(const void *data, uint16_t data_len)
{
	for (int i = 0; i < num_serial_data_cmds; i++) {
		size_t len = strlen(serial_data_cmds[i]);
		if (data_len >= len && memcmp(data, serial_data_cmds[i], len) == 0)
			return i + 1; // index of matched command
	}
	return 0; // no match
}

/**
 * @brief  Set the chip select (CS) line state.
 * @param  state: 1 to assert CS, 0 to deassert.
 * @retval Result of spi_port_set_cs.
 */
static int32_t set_cs(int32_t state)
{
	if (state == 0)
		rdy_rising_edge = 0;

	return spi_port_set_cs(state);
}

/**
 * @brief  Wait for the RDY pin to go high (rising edge) or timeout.
 * @param  timeout: Timeout in milliseconds.
 * @retval 0 if RDY rising edge detected, -1 if timeout.
 */
static int32_t wait_for_rdy_rising(uint32_t timeout)
{
#if !defined (USE_HAL_DRIVER)
	while (rdy_rising_edge == 0)
	{
		if (timeout == 0)
		{
#if (SPI_IFACE_LOG_INCOMING > 0)
			printf("SPI_IFACE_TIMEOUT\n");
#endif
			return -1;
		}
		LL_mDelay(1); // Wait 1 ms
		timeout--;
	}
#else
	uint32_t tickstart = HAL_GetTick();

	while (rdy_rising_edge == 0)
	{
		if ((HAL_GetTick() - tickstart) > timeout)
		{
#if (SPI_IFACE_LOG_INCOMING > 0)
			printf("SPI_IFACE_TIMEOUT\n");
#endif
			return -1;
		}
	}
#endif

	return 0;
}
