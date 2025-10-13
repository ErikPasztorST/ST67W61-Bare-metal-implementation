#ifndef HEARTRATE_H
#define HEARTRATE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <main.h>

#define USE_LONG_UUID (1)


#define HEARTRATE_SERVICE_UUID "0000180dcc7a482a984a7f2ed5b3e58f"
#define HEARTRATE_MEASURMENT_CHAR_UUID "00002a378e2245419d4c21edae82ed19"
#define HEARTRATE_SENSOR_LOCATION_CHAR_UUID "00002a388e2245419d4c21edae82ed1a"
#define HEARTRATE_CONTROL_POINT_CHAR_UUID "00002a398e2245419d4c21edae82ed1b"

#define HEARTRATE_SERVICE_UUID_SHORT "180d"
#define HEARTRATE_MEASURMENT_CHAR_UUID_SHORT "2a37"
#define HEARTRATE_SENSOR_LOCATION_CHAR_UUID_SHORT "2a38"
#define HEARTRATE_CONTROL_POINT_CHAR_UUID_SHORT  "2a39"

#define HEARTRATE_SERVICE_IDX 0
#define HEARTRATE_MEASURMENT_CHAR_IDX 0
#define HEARTRATE_SENSOR_LOCATION_CHAR_IDX 1
#define HEARTRATE_CONTROL_POINT_CHAR_IDX 2

//2: characteristic read property
//4: characteristic write without response property
//8: characteristic write with response property
//16: characteristic notify property
//32: characteristic indicate property
#define HEARTRATE_MEASURMENT_CHAR_PROP 16
#define HEARTRATE_SENSOR_LOCATION_CHAR_PROP 2
#define HEARTRATE_CONTROL_POINT_CHAR_PROP 8

//1: read permissions
//2: write permissions
#define HEARTRATE_MEASURMENT_CHAR_PERM 0
#define HEARTRATE_SENSOR_LOCATION_CHAR_PERM 0
#define HEARTRATE_CONTROL_POINT_CHAR_PERM 0



#define WIFI_COMMISSIONING_SERVICE_UUID          "0000FF9Acc7a482a984a7f2ed5b3e58f"
#define WIFI_CONTROL_CHAR_UUID                   "0000FE9B8e2245419d4c21edae82ed19"
#define WIFI_CONFIGURE_CHAR_UUID                 "0000FE9C8e2245419d4c21edae82ed19"
#define WIFI_AP_LIST_CHAR_UUID                   "0000FE9D8e2245419d4c21edae82ed19"
#define WIFI_MONITORING_CHAR_UUID                "0000FE9E8e2245419d4c21edae82ed19"

#define WIFI_COMMISSIONING_SERVICE_INDEX         0
#define WIFI_CONTROL_CHAR_INDEX                  0
#define WIFI_CONFIGURE_CHAR_INDEX                1
#define WIFI_AP_LIST_CHAR_INDEX                  2
#define WIFI_MONITORING_CHAR_INDEX               3

//2: characteristic read property
//4: characteristic write without response property
//8: characteristic write with response property
//16: characteristic notify property
//32: characteristic indicate property
#define WIFI_CONTROL_CHAR_PROP 8
#define WIFI_CONFIGURE_CHAR_PROP 8
#define WIFI_AP_LIST_CHAR_PROP 16
#define WIFI_MONITORING_CHAR_PROP (16+2)

//1: read permissions
//2: write permissions
#define WIFI_CONTROL_CHAR_PERM 2
#define WIFI_CONFIGURE_CHAR_PERM 2
#define WIFI_AP_LIST_CHAR_PERM 1
#define WIFI_MONITORING_CHAR_PERM (1+2)

#define CONTROL_ACTION_START_SCAN                0x1
#define CONTROL_ACTION_CONNECT                   0x3
#define CONTROL_ACTION_DISCONNECT                0x4
#define CONTROL_ACTION_PING                      0x5

#define CONFIGURE_TYPE_SSID                      0x1
#define CONFIGURE_TYPE_PWD                       0x2
#define CONFIGURE_TYPE_SECURITY_FLAG             0x5

#define MONITORING_TYPE_CONNECTING               0x3
#define MONITORING_TYPE_CONNECTION_DONE          0x4
#define MONITORING_TYPE_PING_RESPONSE            0x5
#define MONITORING_TYPE_ERROR                    0x6

#define MONITORING_DATA_CONNECTION_TIMEOUT       0x1


bool BLE_Init(void);
bool BLE_Handle_Write(char *report);
void Wifi_Scan_End(void);
void Wifi_Scan_Report(char *report);
void Wifi_Connected(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HEARTRATE_H */
