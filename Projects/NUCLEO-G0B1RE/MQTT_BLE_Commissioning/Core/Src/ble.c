#include "ble.h"
#include "spi_iface.h"
#include "mqtt.h"

// Global flag to wait for IP report after WiFi connection
extern uint8_t wait_for_ip_report;

// BLE connection status flag
bool ble_connected = false;

char a_AdvData[36] =
{
		'0', 'F', /* Manuf data length */
		'F', 'F', /* Manuf data Flag */
		'3', '0', '0', '0', /*  */
		'0', '2', /* Blue ST SDK v2  */
		'9', 'A', /* Board ID */
		'F', 'E', /* FW ID */
		'0', '0', /* FW data */
		'0', '0', /* FW data */
		'0', '0', /* FW data */
		'0', '0', /* BD Address MSB */
		'0', '0', /*  */
		'0', '0', /*  */
		'0', '0', /*  */
		'0', '0', /*  */
		'0', '0', /* BD Address LSB */
};


#define SCAN_MAX_APS 32
#define SCAN_MAX_SSID_SIZE 32

typedef struct
{
	uint8_t SSID_len;
	uint8_t Channel;
	int16_t RSSI;
	uint32_t Security;
	uint8_t SSID[SCAN_MAX_SSID_SIZE + 1];
} AP_info_t ;

typedef struct
{
	uint8_t SSID_len;
	uint8_t SSID[SCAN_MAX_SSID_SIZE + 1];
	uint8_t password_len;
	uint8_t password[SCAN_MAX_SSID_SIZE + 1];
} AP_connect_t ;

static AP_info_t ap_scan_list[SCAN_MAX_APS];
static AP_connect_t ap_to_connect;
static int ap_scan_count = 0;

static bool notify_ap_connected = false;


bool BLE_Init_General(void);
bool BLE_Init_Service(void);
bool BLE_Update_Characteristic(uint8_t srv_idx, uint8_t char_idx, uint8_t *data, uint8_t data_len, bool notify);

bool start_wifi_scan();
bool connect_to_ap();
uint32_t parse_gatt_write(char *report, uint8_t *srv_idx, uint8_t *char_idx, char *data);

/**
 * @brief  Initialize the Heart Rate Service Application
 * @retval true if initialization is successful
 */
bool BLE_Init(void)
{
	BLE_Init_General();
	BLE_Init_Service();

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
	char at_cmd[128];
	char *resp = NULL;

	snprintf(at_cmd, sizeof(at_cmd), "AT+BLEGATTSSRVCRE=%d,\"%s\",1,%d",
			WIFI_COMMISSIONING_SERVICE_INDEX, WIFI_COMMISSIONING_SERVICE_UUID, USE_LONG_UUID*2);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	snprintf(at_cmd, sizeof(at_cmd), "AT+BLEGATTSCHARCRE=%d,%d,\"%s\",%d,%d,%d",
			WIFI_COMMISSIONING_SERVICE_INDEX, WIFI_CONTROL_CHAR_INDEX,
			WIFI_CONTROL_CHAR_UUID, WIFI_CONTROL_CHAR_PROP,
			WIFI_CONTROL_CHAR_PERM, USE_LONG_UUID*2);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	snprintf(at_cmd, sizeof(at_cmd), "AT+BLEGATTSCHARCRE=%d,%d,\"%s\",%d,%d,%d",
			WIFI_COMMISSIONING_SERVICE_INDEX, WIFI_CONFIGURE_CHAR_INDEX,
			WIFI_CONFIGURE_CHAR_UUID, WIFI_CONFIGURE_CHAR_PROP,
			WIFI_CONFIGURE_CHAR_PERM, USE_LONG_UUID*2);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	snprintf(at_cmd, sizeof(at_cmd), "AT+BLEGATTSCHARCRE=%d,%d,\"%s\",%d,%d,%d",
			WIFI_COMMISSIONING_SERVICE_INDEX, WIFI_AP_LIST_CHAR_INDEX,
			WIFI_AP_LIST_CHAR_UUID, WIFI_AP_LIST_CHAR_PROP,
			WIFI_AP_LIST_CHAR_PERM, USE_LONG_UUID*2);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	snprintf(at_cmd, sizeof(at_cmd), "AT+BLEGATTSCHARCRE=%d,%d,\"%s\",%d,%d,%d",
			WIFI_COMMISSIONING_SERVICE_INDEX, WIFI_MONITORING_CHAR_INDEX,
			WIFI_MONITORING_CHAR_UUID, WIFI_MONITORING_CHAR_PROP,
			WIFI_MONITORING_CHAR_PERM, USE_LONG_UUID*2);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	// Register GATT service
	spi_iface_command("AT+BLEGATTSREGISTER=1", &resp);
	free(resp);

	return true;
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

bool BLE_Handle_Write(char *report)
{
	uint8_t srv_idx, char_idx;
	char data[SCAN_MAX_SSID_SIZE];
	uint32_t len;

	len = parse_gatt_write(report, &srv_idx, &char_idx, data);

	if (srv_idx == WIFI_COMMISSIONING_SERVICE_INDEX)
	{
		if (char_idx == WIFI_CONTROL_CHAR_INDEX)
		{
			if (len == 1 && data[0] == CONTROL_ACTION_START_SCAN)
			{
				start_wifi_scan();
				// expect individual reports in Wifi_Scan_Report
				// take further action in Wifi_Scan_End
			}
			else if (len == 1 && data[0] == CONTROL_ACTION_CONNECT)
			{
				connect_to_ap();
			}
		}
		else if (char_idx == WIFI_CONFIGURE_CHAR_INDEX)
		{
			if (data[0] == CONFIGURE_TYPE_SSID)
			{
				strncpy((char *)ap_to_connect.SSID, (data+1), SCAN_MAX_SSID_SIZE);
				ap_to_connect.SSID_len = strlen((char *)ap_to_connect.SSID);
			}
			else if (data[0] == CONFIGURE_TYPE_PWD)
			{
				strncpy((char *)ap_to_connect.password, (data+1), SCAN_MAX_SSID_SIZE);
				ap_to_connect.password_len = strlen((char *)ap_to_connect.password);
			}
		}

	}

	return true;
}

bool start_wifi_scan()
{
	char at_cmd[64];
	char *resp = NULL;

	ap_scan_count = 0;

	snprintf(at_cmd, sizeof(at_cmd), "AT+CWLAPOPT=1,23,-100,255,%d", SCAN_MAX_APS);
	spi_iface_command(at_cmd, &resp);
	free(resp);
	spi_iface_command("AT+CWLAP=0,,,0", &resp);
	free(resp);

	return true;
}

bool connect_to_ap()
{
	char at_cmd[128];
	char *resp = NULL;

	MQTT_Disconnect();

	snprintf(at_cmd, sizeof(at_cmd), "AT+CWJAP=\"%s\",\"%s\"", ap_to_connect.SSID, ap_to_connect.password);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	notify_ap_connected = true;
	return true;
}

void Wifi_Connected(void)
{
	char *resp = NULL;
	uint8_t data[SCAN_MAX_SSID_SIZE + 1] = {0};
	data[0] = MONITORING_TYPE_CONNECTION_DONE;

	spi_iface_command("AT+CWJAP?", &resp);

	char *start = strchr(resp, '"');
	char *end = strchr(start + 1, '"');
	size_t len = end - start - 1;
	memcpy(data+1, start + 1, len);
	printf("%s\n", data+1);

	free(resp);

	BLE_Update_Characteristic(WIFI_COMMISSIONING_SERVICE_INDEX,
			WIFI_MONITORING_CHAR_INDEX, data,
			strlen((const char *)data), true);
}

void Wifi_Scan_End(void)
{
	ap_scan_count--;
	while (ap_scan_count >= 0)
	{
		BLE_Update_Characteristic(WIFI_COMMISSIONING_SERVICE_INDEX,
				WIFI_AP_LIST_CHAR_INDEX, (uint8_t *)&ap_scan_list[ap_scan_count],
				sizeof(AP_info_t ), true);

		ap_scan_count--;
	}
}

void Wifi_Scan_Report(char *report)
{
	int sec, rssi, ch;
	char ssid[SCAN_MAX_SSID_SIZE + 1] = {0};

	sscanf(report, "+CWLAP:(%d,\"%32[^\"]\",%d,%d)", &sec, ssid, &rssi, &ch);

	ap_scan_list[ap_scan_count].Security = sec;
	ap_scan_list[ap_scan_count].RSSI = rssi;
	ap_scan_list[ap_scan_count].Channel = ch;
	ap_scan_list[ap_scan_count].SSID_len = SCAN_MAX_SSID_SIZE;
	strncpy((char *)ap_scan_list[ap_scan_count].SSID, ssid, SCAN_MAX_SSID_SIZE);
	ap_scan_list[ap_scan_count].SSID[SCAN_MAX_SSID_SIZE] = '\0';

	ap_scan_count++;
}

/**
 * @brief Parse a BLE GATT write report string.
 * @param report   Input string (e.g., "+BLE:GATTWRITE:0,0,1,6,TOMCAT")
 * @param srv_idx  Output: pointer to service index (uint8_t)
 * @param char_idx Output: pointer to characteristic index (uint8_t)
 * @param data     Output: pointer to buffer for data (must be large enough)
 * @return         Length of data parsed, or 0 on error
 */
uint32_t parse_gatt_write(char *report, uint8_t *srv_idx, uint8_t *char_idx, char *data)
{
	// Example: +BLE:GATTWRITE:0,0,1,6,TOMCAT
	// Fields:  [0] +BLE:GATTWRITE:
	//          [1] 0 (connection idx, can be ignored)
	//          [2] srv_idx
	//          [3] char_idx
	//          [4] data_len
	//          [5] data

	char *token;
	uint32_t data_len = 0;
	char *saveptr = NULL;

	// Make a copy to avoid modifying the original string
	char temp[128];
	strncpy(temp, report, sizeof(temp) - 1);
	temp[sizeof(temp) - 1] = '\0';

	token = strtok_r(temp, ":", &saveptr); // "+BLE"
	token = strtok_r(NULL, ":", &saveptr); // "GATTWRITE"
	token = strtok_r(NULL, ",", &saveptr); // "0" (connection idx, skip)
	if (!token) return 0;

	token = strtok_r(NULL, ",", &saveptr); // srv_idx
	if (!token) return 0;
	*srv_idx = (uint8_t)atoi(token);

	token = strtok_r(NULL, ",", &saveptr); // char_idx
	if (!token) return 0;
	*char_idx = (uint8_t)atoi(token);

	token = strtok_r(NULL, ",", &saveptr); // data_len
	if (!token) return 0;
	data_len = (uint32_t)atoi(token);

	token = strtok_r(NULL, ",", &saveptr); // data (may contain commas, so use the rest)
	if (!token) return 0;

	// Copy the data (may contain commas, so use the rest of the string)
	strncpy(data, token, data_len);
	data[data_len] = '\0'; // Null-terminate

	return data_len;
}
