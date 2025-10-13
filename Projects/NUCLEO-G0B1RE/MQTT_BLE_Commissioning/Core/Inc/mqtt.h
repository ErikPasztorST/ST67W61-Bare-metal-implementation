#ifndef MQTT_H
#define MQTT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <main.h>

#define MQTT_CLIENT_ID "ST67_Min"
#define MQTT_TOPIC_RSSI "/sensors/ST67_Min"
#define MQTT_TOPIC_LED "/devices/ST67_Min/control"

#define MQTT_RSSI_MSG "{ \"state\": { \"reported\": {    \"rssi\": %d  } }}"
#define MQTT_LEVEL_MSG "{\"Level\":%d}"
#define MQTT_COLOR_MSG "{\"R\":%d, \"G\":%d, \"B\":%d}"

#define MQTT_QOS 1

bool MQTT_Init(void);
bool MQTT_Connect(void);
bool MQTT_Disconnect(void);
bool Wifi_Init(void);
bool Wifi_Autoconnect(void);
bool MQTT_Publish(const char *topic, const char *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MQTT_H */
