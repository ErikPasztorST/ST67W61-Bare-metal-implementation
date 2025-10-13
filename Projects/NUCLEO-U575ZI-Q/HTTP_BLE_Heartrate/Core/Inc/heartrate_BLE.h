#ifndef HEARTRATE_BLE_H
#define HEARTRATE_BLE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define HEARTRATE_SERVICE_UUID "0000180dcc7a482a984a7f2ed5b3e58f"
#define HEARTRATE_MEASURMENT_CHAR_UUID "00002a378e2245419d4c21edae82ed19"
#define HEARTRATE_SENSOR_LOCATION_CHAR_UUID "00002a388e2245419d4c21edae82ed1a"
#define HEARTRATE_CONTROL_POINT_CHAR_UUID "00002a398e2245419d4c21edae82ed1b"

#define HEARTRATE_SERVICE_UUID_SHORT "180d"
#define HEARTRATE_MEASURMENT_CHAR_UUID_SHORT "2a37"
#define HEARTRATE_SENSOR_LOCATION_CHAR_UUID_SHORT "2a38"
#define HEARTRATE_CONTROL_POINT_CHAR_UUID_SHORT  "2a39"

#define USE_LONG_UUID (0)

char a_AdvData[36] =
{
  '0', 'F', /* Manuf data length */
  'F', 'F', /* Manuf data Flag */
  '3', '0', '0', '0', /*  */
  '0', '2', /* Blue ST SDK v2  */
  '9', 'A', /* Board ID */
  '8', '9', /* FW ID */
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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HEARTRATE_BLE_H */
