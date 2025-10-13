#ifndef HEARTRATE_COMMON_H
#define HEARTRATE_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef enum
{
  HRS_HRM_VALUE_FORMAT_UINT16 = 1,
  HRS_HRM_SENSOR_CONTACTS_PRESENT = 2,
  HRS_HRM_SENSOR_CONTACTS_SUPPORTED = 4,
  HRS_HRM_ENERGY_EXPENDED_PRESENT = 8,
  HRS_HRM_RR_INTERVAL_PRESENT = 0x10
} HRS_HrmFlags_t;

typedef enum
{
  HRS_BODY_SENSOR_LOCATION_OTHER = 0,
  HRS_BODY_SENSOR_LOCATION_CHEST = 1,
  HRS_BODY_SENSOR_LOCATION_WRIST = 2,
  HRS_BODY_SENSOR_LOCATION_FINGER = 3,
  HRS_BODY_SENSOR_LOCATION_HAND = 4,
  HRS_BODY_SENSOR_LOCATION_EAR_LOBE = 5,
  HRS_BODY_SENSOR_LOCATION_FOOT = 6
} HRS_BodySensorLocation_t;

typedef struct{
  uint16_t    MeasurementValue;
  uint16_t    EnergyExpended;
  uint16_t    aRRIntervalValues[2];
  uint8_t     NbreOfValidRRIntervalValues;
  uint8_t     Flags;
}HRS_MeasVal_t;

typedef struct
{
  HRS_BodySensorLocation_t BodySensorLocationChar;
  HRS_MeasVal_t MeasurementvalueChar;
  uint8_t ResetEnergyExpended;
  uint8_t TimerMeasurement_Id;
} HRSAPP_Context_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HEARTRATE_COMMON_H */
