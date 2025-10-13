#include "heartrate.h"
#include "heartrate_common_types.h"
#include "heartrate_WiFi.h"
#include "heartrate_BLE.h"
#include "html_pages.h"
#include "spi_iface.h"

#define WIFI_SSID "mySSID"
#define WIFI_PASSWORD "myPassword"

// Global flag to wait for IP report after WiFi connection
extern uint8_t wait_for_ip_report;

// Application context for heart rate service
HRSAPP_Context_t HRSAPP_Context;

// BLE connection status flag
bool ble_connected = false;

// Function prototypes for WiFi and BLE initialization and HTTP handling
bool WiFi_Init_Http(void);
bool WiFi_Init_General(void);
char *Http_Receive_Request(uint32_t socket_id, uint32_t data_len);
char *Http_Process_Response(char *request);
bool Http_Send_Response(uint32_t socket_id, char *response);

bool BLE_Init_General(void);
bool BLE_Init_Service(void);
void BLE_Send_Measurement(void);
bool BLE_Update_Characteristic(uint8_t srv_idx, uint8_t char_idx, uint8_t *data, uint8_t data_len, bool notify);

/**
 * @brief  Initialize the Heart Rate Service Application
 * @retval true if initialization is successful
 */
bool HRSAPP_Init(void)
{
	// Set default flags and values for the heart rate measurement characteristic
	HRSAPP_Context.MeasurementvalueChar.Flags = (
			HRS_HRM_VALUE_FORMAT_UINT16      |
			HRS_HRM_SENSOR_CONTACTS_PRESENT  |
			HRS_HRM_SENSOR_CONTACTS_SUPPORTED|
			HRS_HRM_ENERGY_EXPENDED_PRESENT  |
			HRS_HRM_RR_INTERVAL_PRESENT
	);

	HRSAPP_Context.MeasurementvalueChar.EnergyExpended = 10;
	HRSAPP_Context.MeasurementvalueChar.NbreOfValidRRIntervalValues = 1;
	HRSAPP_Context.MeasurementvalueChar.aRRIntervalValues[0] = 1024;
	HRSAPP_Context.MeasurementvalueChar.MeasurementValue = 60;

	HRSAPP_Context.BodySensorLocationChar = HRS_BODY_SENSOR_LOCATION_HAND;

	// Initialize WiFi and BLE modules
	WiFi_Init_General();
	WiFi_Init_Http();

	BLE_Init_General();
	BLE_Init_Service();

	return true;
}

/**
 * @brief  Simulate and update heart rate measurement, send over BLE if connected
 */
void HRSAPP_Measurement(void)
{
	static uint32_t simulated_hr_values[6] = {60, 65, 70, 65, 60, 55};
	static uint8_t sim_idx = 0;

	uint32_t measurement = simulated_hr_values[sim_idx++];
	if (sim_idx == 6) sim_idx = 0;

	HRSAPP_Context.MeasurementvalueChar.MeasurementValue = measurement;

	// Update energy expended value or reset if requested
	if (HRSAPP_Context.ResetEnergyExpended == 0)
		HRSAPP_Context.MeasurementvalueChar.EnergyExpended += 5;
	else if (HRSAPP_Context.ResetEnergyExpended == 1)
		HRSAPP_Context.ResetEnergyExpended = 0;

	printf("Measure HRM=%lu, EE=%u\n", measurement, HRSAPP_Context.MeasurementvalueChar.EnergyExpended);

	// Send measurement over BLE if connected
	if (ble_connected)
		BLE_Send_Measurement();
}

/**
 * @brief  Handle BLE connection event
 * @param  is_ble: true if BLE connection event
 */
void HRSAPP_OnConnection(bool is_ble)
{
	if (is_ble)
		ble_connected = true;
}

/**
 * @brief  Handle BLE disconnection event
 * @param  is_ble: true if BLE disconnection event
 */
void HRSAPP_OnDisconnection(bool is_ble)
{
	if (is_ble)
		ble_connected = false;
}

/**
 * @brief  Handle incoming HTTP request on the specified socket
 * @param  socket_id: Socket identifier
 * @param  data_len: Length of incoming data
 * @retval true if request handled successfully
 */
bool HRSAPP_Http_Request(uint32_t socket_id, uint32_t data_len)
{
	char *http_req = NULL;
	char *http_resp = NULL;

	http_req = Http_Receive_Request(socket_id, data_len);
	if (!http_req)
		return false;

	http_resp = Http_Process_Response(http_req);
	if (!http_resp) {
		free(http_req);
		return false;
	}

	if (!Http_Send_Response(socket_id, http_resp)) {
		free(http_req);
		free(http_resp);
		return false;
	}

	free(http_req);
	free(http_resp);

	return true;
}

/**
 * @brief  Initialize BLE module and set advertising data
 * @retval true if initialization is successful
 */
bool BLE_Init_General(void)
{
	char at_cmd[64];
	char *resp = NULL;
	uint32_t data_index = 0, addr_i = 10; // "+BLEADDR:" prefix is 10 chars

	// Initialize BLE stack
	spi_iface_command("AT+BLEINIT=2", &resp);
	free(resp);

	// Set BLE device name
	spi_iface_command("AT+BLENAME=\"ST67_Min_BLE\"", &resp);
	free(resp);

	// Get BLE address and update advertising data
	spi_iface_command("AT+BLEADDR?", &resp);
	while (data_index < 6) {
		a_AdvData[20 + (2 * data_index)] = resp[addr_i];
		a_AdvData[20 + (2 * data_index) + 1] = resp[addr_i + 1];
		addr_i += 3;
		data_index++;
	}
	free(resp);

	// Set advertising data
	snprintf(at_cmd, sizeof(at_cmd), "AT+BLEADVDATA=\"%s\"", a_AdvData);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	// Set advertising parameters, TX power, security, and start advertising
	spi_iface_command("AT+BLEADVPARAM=128,160,0,7", &resp);
	free(resp);
	spi_iface_command("AT+BLETXPWR=0", &resp);
	free(resp);
	spi_iface_command("AT+BLESECPARAM=1", &resp);
	free(resp);
	spi_iface_command("AT+BLEADVSTART", &resp);
	free(resp);

	return true;
}

/**
 * @brief  Initialize BLE GATT service and characteristics
 * @retval true if initialization is successful
 */
bool BLE_Init_Service(void)
{
	char at_cmd[64];
	char *resp = NULL;

	// Create Heart Rate Service
	snprintf(at_cmd, sizeof(at_cmd), "AT+BLEGATTSSRVCRE=%d,\"%s\",1,%d",
			HEARTRATE_SERVICE_IDX, HEARTRATE_SERVICE_UUID_SHORT, USE_LONG_UUID*2);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	// Create Heart Rate Measurement Characteristic
	snprintf(at_cmd, sizeof(at_cmd), "AT+BLEGATTSCHARCRE=%d,%d,\"%s\",%d,%d,%d",
			HEARTRATE_SERVICE_IDX, HEARTRATE_MEASURMENT_CHAR_IDX,
			HEARTRATE_MEASURMENT_CHAR_UUID_SHORT, HEARTRATE_MEASURMENT_CHAR_PROP,
			HEARTRATE_MEASURMENT_CHAR_PERM, USE_LONG_UUID*2);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	// Create Body Sensor Location Characteristic
	snprintf(at_cmd, sizeof(at_cmd), "AT+BLEGATTSCHARCRE=%d,%d,\"%s\",%d,%d,%d",
			HEARTRATE_SERVICE_IDX, HEARTRATE_SENSOR_LOCATION_CHAR_IDX,
			HEARTRATE_SENSOR_LOCATION_CHAR_UUID_SHORT, HEARTRATE_SENSOR_LOCATION_CHAR_PROP,
			HEARTRATE_SENSOR_LOCATION_CHAR_PERM, USE_LONG_UUID*2);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	// Create Control Point Characteristic
	snprintf(at_cmd, sizeof(at_cmd), "AT+BLEGATTSCHARCRE=%d,%d,\"%s\",%d,%d,%d",
			HEARTRATE_SERVICE_IDX, HEARTRATE_CONTROL_POINT_CHAR_IDX,
			HEARTRATE_CONTROL_POINT_CHAR_UUID_SHORT, HEARTRATE_CONTROL_POINT_CHAR_PROP,
			HEARTRATE_CONTROL_POINT_CHAR_PERM, USE_LONG_UUID*2);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	// Register GATT service
	spi_iface_command("AT+BLEGATTSREGISTER=1", &resp);
	free(resp);

	// Update Body Sensor Location characteristic with initial value
	BLE_Update_Characteristic(HEARTRATE_SERVICE_IDX, HEARTRATE_SENSOR_LOCATION_CHAR_IDX,
			(uint8_t *)&HRSAPP_Context.BodySensorLocationChar,
			sizeof(HRSAPP_Context.BodySensorLocationChar),
			false);

	return true;
}

/**
 * @brief  Prepare and send heart rate measurement over BLE
 */
void BLE_Send_Measurement(void)
{
	uint8_t hrm_value[7];
	uint8_t hrm_char_length = 1;

	// Set flags
	hrm_value[0] = HRSAPP_Context.MeasurementvalueChar.Flags;

	// Add Heart Rate Measurement Value (8 or 16 bits)
	if (HRSAPP_Context.MeasurementvalueChar.Flags & HRS_HRM_VALUE_FORMAT_UINT16) {
		hrm_value[hrm_char_length++] = (uint8_t)(HRSAPP_Context.MeasurementvalueChar.MeasurementValue & 0xFF);
		hrm_value[hrm_char_length++] = (uint8_t)(HRSAPP_Context.MeasurementvalueChar.MeasurementValue >> 8);
	} else {
		hrm_value[hrm_char_length++] = (uint8_t)HRSAPP_Context.MeasurementvalueChar.MeasurementValue;
	}

	// Add Energy Expended if present
	if (HRSAPP_Context.MeasurementvalueChar.Flags & HRS_HRM_ENERGY_EXPENDED_PRESENT) {
		hrm_value[hrm_char_length++] = (uint8_t)(HRSAPP_Context.MeasurementvalueChar.EnergyExpended & 0xFF);
		hrm_value[hrm_char_length++] = (uint8_t)(HRSAPP_Context.MeasurementvalueChar.EnergyExpended >> 8);
	}

	// Add RR Interval values if present
	if (HRSAPP_Context.MeasurementvalueChar.Flags & HRS_HRM_RR_INTERVAL_PRESENT) {
		uint8_t rr_count = HRSAPP_Context.MeasurementvalueChar.NbreOfValidRRIntervalValues;
		// Limit RR intervals if necessary
		if ((HRSAPP_Context.MeasurementvalueChar.Flags & HRS_HRM_VALUE_FORMAT_UINT16) &&
				!(HRSAPP_Context.MeasurementvalueChar.Flags & HRS_HRM_ENERGY_EXPENDED_PRESENT) &&
				rr_count > 8) {
			rr_count = 8;
		}
		for (uint8_t i = 0; i < rr_count; i++) {
			hrm_value[hrm_char_length++] = (uint8_t)(HRSAPP_Context.MeasurementvalueChar.aRRIntervalValues[i] & 0xFF);
			hrm_value[hrm_char_length++] = (uint8_t)(HRSAPP_Context.MeasurementvalueChar.aRRIntervalValues[i] >> 8);
		}
	}

	// Update the characteristic value and notify if required
	BLE_Update_Characteristic(HEARTRATE_SERVICE_IDX, HEARTRATE_MEASURMENT_CHAR_IDX,
			hrm_value, hrm_char_length, true);
}

/**
 * @brief  Update BLE characteristic value and optionally notify
 * @param  srv_idx: Service index
 * @param  char_idx: Characteristic index
 * @param  data: Pointer to data buffer
 * @param  data_len: Length of data
 * @param  notify: true to send notification, false for read
 * @retval true if update is successful
 */
bool BLE_Update_Characteristic(uint8_t srv_idx, uint8_t char_idx, uint8_t *data, uint8_t data_len, bool notify)
{
	char at_cmd[64];
	char *resp = NULL;

	// Send notification or read command
	if (notify)
		snprintf(at_cmd, sizeof(at_cmd), "AT+BLEGATTSNTFY=%d,%d,%d", srv_idx, char_idx, data_len);
	else
		snprintf(at_cmd, sizeof(at_cmd), "AT+BLEGATTSRD=%d,%d,%d", srv_idx, char_idx, data_len);

	spi_iface_command(at_cmd, &resp);
	free(resp);

	// Send the actual data
	spi_iface_data((const uint8_t *)data, data_len, &resp);
	free(resp);

	return true;
}

/**
 * @brief  Initialize WiFi HTTP server settings
 * @retval true if initialization is successful
 */
bool WiFi_Init_Http(void)
{
	char *resp = NULL;

	// Configure WiFi module for HTTP server operation
	spi_iface_command("AT+CIPMUX=1", &resp);         // Enable multiple connections
	free(resp);
	spi_iface_command("AT+CIPRECVMODE=1", &resp);    // Set receive mode
	free(resp);
	spi_iface_command("AT+CIPDINFO=1", &resp);       // Enable detailed info
	free(resp);
	spi_iface_command("AT+CIPRECVBUF=0,4608", &resp);// Set receive buffer size for each connection
	free(resp);
	spi_iface_command("AT+CIPRECVBUF=1,4608", &resp);
	free(resp);
	spi_iface_command("AT+CIPRECVBUF=2,4608", &resp);
	free(resp);
	spi_iface_command("AT+CIPRECVBUF=3,4608", &resp);
	free(resp);
	spi_iface_command("AT+CIPRECVBUF=4,4608", &resp);
	free(resp);
	spi_iface_command("AT+CIPSERVERMAXCONN=5", &resp);// Set max server connections
	free(resp);
	spi_iface_command("AT+CIPSERVER=1,80,\"TCP\",0,1000", &resp);// Start TCP server on port 80
	free(resp);

	return true;
}

/**
 * @brief  Initialize general WiFi settings and connect to AP
 * @retval true if initialization is successful
 */
bool WiFi_Init_General(void)
{
	char at_cmd[64];
	char *resp = NULL;

	// Set WiFi mode, disable auto-connect, set country, hostname, etc.
	spi_iface_command("AT+CWMODE=1,0", &resp);
	free(resp);
	spi_iface_command("AT+CWAUTOCONN=0", &resp);
	free(resp);
	spi_iface_command("AT+CWCOUNTRY=0,\"00\"", &resp);
	free(resp);
	spi_iface_command("AT+CWHOSTNAME=\"ST67_Min_WiFi\"", &resp);
	free(resp);
	spi_iface_command("AT+SLCLDTIM", &resp);
	free(resp);
	spi_iface_command("AT+CWDHCP=1,3", &resp);
	free(resp);
	spi_iface_command("AT+CIPDNS=0", &resp);
	free(resp);
	spi_iface_command("AT+CWRECONNCFG=0,0", &resp);
	free(resp);

	// Connect to WiFi access point and wait for IP assignment
	wait_for_ip_report = 1;
	snprintf(at_cmd, sizeof(at_cmd), "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_PASSWORD);
	spi_iface_command(at_cmd, &resp);
	free(resp);
	while (wait_for_ip_report) {}

	// Print assigned IP address
	spi_iface_command("AT+CIPSTA?", &resp);
	printf("%s", resp);
	free(resp);

	return true;
}

/**
 * @brief  Receive HTTP request data from socket
 * @param  socket_id: Socket identifier
 * @param  data_len: Length of data to receive
 * @retval Pointer to received request buffer (must be freed by caller)
 */
char *Http_Receive_Request(uint32_t socket_id, uint32_t data_len)
{
	char at_cmd[32];
	char *at_resp = NULL;
	char *recv_buf = NULL;
	uint32_t total_received = 0;

	// Receive data in chunks if necessary
	while (data_len > 0) {
		uint32_t to_receive = (data_len > HTTP_MAX_BYTES_TO_RECEIVE) ? HTTP_MAX_BYTES_TO_RECEIVE : data_len;
		snprintf(at_cmd, sizeof(at_cmd), "AT+CIPRECVDATA=%lu,%lu", socket_id, to_receive);
		if (spi_iface_command(at_cmd, &at_resp) < 0) {
			free(at_resp);
			free(recv_buf);
			return NULL;
		}

		// Remove prefix up to and including first comma
		char *data_ptr = strchr(at_resp, ',');
		if (data_ptr) data_ptr++;
		else data_ptr = at_resp;

		recv_buf = realloc(recv_buf, total_received + to_receive + 1);
		memcpy(recv_buf + total_received, data_ptr, to_receive);
		total_received += to_receive;
		data_len -= to_receive;
		free(at_resp);
		at_resp = NULL;
	}
	if (recv_buf)
		recv_buf[total_received] = 0;
	return recv_buf;
}

/**
 * @brief  Process HTTP request and prepare response
 * @param  request: Pointer to HTTP request string
 * @retval Pointer to response buffer (must be freed by caller)
 */
char *Http_Process_Response(char *request)
{
	HttpServer_response_e response_type = UNKNOWN_RESPONSE;
	char *response_data = NULL;
	char response_hrm[250] =
			"HTTP/1.1 200 OK\r\n"
			"Server: U5\r\n"
			"Access-Control-Allow-Origin: * \r\n"
			"Cache-Control: no-cache\r\n"
			"Keep-Alive: timeout=2, max=2\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html; charset=utf-8\r\n";

	// Determine response type based on request
	for (uint32_t i = 0; i < sizeof(http_server_responses) / sizeof(http_server_responses[0]); i++) {
		if (strncmp(request, http_server_responses[i].request, strlen(http_server_responses[i].request)) == 0) {
			response_type = http_server_responses[i].response_type;
			response_data = (char *)http_server_responses[i].response;
			break;
		}
	}

	// Handle unknown requests (404), heart rate measurement, sensor location, and button state
	if (response_type == UNKNOWN_RESPONSE) {
		// Request not recognized, return 404 error page
		response_data = (char *)response_error_404_html;
	} else if (response_type == HEARTRATE_MEASUREMENT) {
		// Handle GET request for Heart Rate Measurement
		char data[150];
		snprintf(data, sizeof(data),
				"{\"HRM\":%d,\"EE\":%d,\"BSL\":%d}",
				HRSAPP_Context.MeasurementvalueChar.MeasurementValue,
				HRSAPP_Context.MeasurementvalueChar.EnergyExpended,
				(uint8_t)HRSAPP_Context.BodySensorLocationChar);

		snprintf(&response_hrm[strlen(response_hrm)], sizeof(response_hrm) - strlen(response_hrm),
				"Content-Length: %d\r\n\r\n%s", (int)strlen(data), data);

		response_data = response_hrm;
	} else if (response_type == HEARTRATE_SENSOR_LOCATION) {
		// Handle GET request for Body Sensor Location
		char data[50];
		snprintf(data, sizeof(data),
				"{\"BodySensorLocation\":%d}",
				HRSAPP_Context.BodySensorLocationChar);

		snprintf(&response_hrm[strlen(response_hrm)], sizeof(response_hrm) - strlen(response_hrm),
				"Content-Length: %d\r\n\r\n%s", (int)strlen(data), data);

		response_data = response_hrm;
	} else if (response_type == BUTTON_STATE) {
		// Handle GET request for button and LED state
		char data[70];
		snprintf(data, sizeof(data),
				"{\"LedGreenPin\":%d,\"LedRedPin\":%d,\"BtnPin\":%d,\"HR\":%d}",
				0, 1, 1, 60);

		snprintf(&response_hrm[strlen(response_hrm)], sizeof(response_hrm) - strlen(response_hrm),
				"Content-Length: %d\r\n\r\n%s", (int)strlen(data), data);

		response_data = response_hrm;
	}

	// Allocate and return a copy of the response
	char *ret = calloc(strlen(response_data) + 1, 1);
	if (ret) {
		strcpy(ret, response_data);
	}
	return ret;
}

/**
 * @brief  Send HTTP response data over the specified socket
 * @param  socket_id: Socket identifier
 * @param  response: Pointer to response data
 * @retval true if response sent successfully
 */
bool Http_Send_Response(uint32_t socket_id, char *response)
{
	char at_cmd[32];
	uint32_t resp_len = strlen(response);
	uint32_t sent = 0;
	char *at_resp = NULL;

	// Send response in chunks if necessary
	while (sent < resp_len) {
		uint32_t to_send = (resp_len - sent > HTTP_MAX_BYTES_TO_SEND) ? HTTP_MAX_BYTES_TO_SEND : (resp_len - sent);

		snprintf(at_cmd, sizeof(at_cmd), "AT+CIPSEND=%lu,%lu", socket_id, to_send);
		if (spi_iface_command(at_cmd, &at_resp) < 0) {
			free(at_resp);
			return false;
		}
		free(at_resp);

		if (spi_iface_data((const uint8_t *)response, to_send, &at_resp) < 0) {
			free(at_resp);
			return false;
		}
		free(at_resp);

		sent += to_send;
		response += to_send;
	}

	// Close the socket after sending the response
	snprintf(at_cmd, sizeof(at_cmd), "AT+CIPCLOSE=%lu", socket_id);
	if (spi_iface_command(at_cmd, &at_resp) < 0) {
		free(at_resp);
		return false;
	}
	free(at_resp);

	return true;
}
