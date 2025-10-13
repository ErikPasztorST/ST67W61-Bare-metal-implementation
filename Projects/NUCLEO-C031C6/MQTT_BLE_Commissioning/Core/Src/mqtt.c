#include "mqtt.h"
#include "spi_iface.h"

// Global flag to wait for IP report after WiFi connection
extern uint8_t wait_for_ip_report;

bool MQTT_Publish(const char *topic, const char *data)
{
	char at_cmd[64];
	char *resp = NULL;

	snprintf(at_cmd, sizeof(at_cmd), "AT+MQTTPUBRAW=0,\"%s\",%d,%d,0", topic, strlen(data), MQTT_QOS);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	spi_iface_data((const uint8_t *)data, strlen(data), &resp);
	free(resp);

	return true;
}

bool MQTT_Init(void)
{
    char *resp = NULL;

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

bool MQTT_Connect(void)
{
	char at_cmd[64];
    char *resp = NULL;
    int32_t ret;

    snprintf(at_cmd, sizeof(at_cmd), "AT+MQTTUSERCFG=0,0,\"%s\",\"MyName\",\"MyPsw\",\"\",\"\",\"\"", MQTT_CLIENT_ID);
	spi_iface_command(at_cmd, &resp);
	free(resp);
    spi_iface_command("AT+MQTTCONNCFG=0,120,0,\"\",\"\",0,0", &resp);
	free(resp);
	spi_iface_command("AT+MQTTSNI=0,\"broker.hivemq.com\"", &resp);
	free(resp);
    ret = spi_iface_command("AT+MQTTCONN=0,\"broker.hivemq.com\",1883,0", &resp);
	free(resp);
	if (ret < 0) return false;

	snprintf(at_cmd, sizeof(at_cmd), "AT+MQTTSUB=0,\"%s\",%d", MQTT_TOPIC_RSSI, MQTT_QOS);
	spi_iface_command(at_cmd, &resp);
	free(resp);
	snprintf(at_cmd, sizeof(at_cmd), "AT+MQTTSUB=0,\"%s\",%d", MQTT_TOPIC_LED, MQTT_QOS);
	spi_iface_command(at_cmd, &resp);
	free(resp);

    return true;
}

bool MQTT_Disconnect(void)
{
	char at_cmd[64];
    char *resp = NULL;

	snprintf(at_cmd, sizeof(at_cmd), "AT+MQTTUNSUB=0,\"%s\"", MQTT_TOPIC_RSSI);
	spi_iface_command(at_cmd, &resp);
	free(resp);
	snprintf(at_cmd, sizeof(at_cmd), "AT+MQTTUNSUB=0,\"%s\"", MQTT_TOPIC_LED);
	spi_iface_command(at_cmd, &resp);
	free(resp);

	spi_iface_command("AT+MQTTCLEAN=0", &resp);
	free(resp);

    return true;
}



bool Wifi_Init(void)
{
    char *resp = NULL;

    // Set WiFi mode, disable auto-connect, set country, hostname, etc.
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

    // Connect with the last known SSID and pwd
	spi_iface_command("AT+CWMODE=1,1", &resp);
	free(resp);
	spi_iface_command("AT+CWAUTOCONN=1", &resp);
	free(resp);

    return true;
}

