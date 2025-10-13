#ifndef HEARTRATE_H
#define HEARTRATE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <main.h>

bool HRSAPP_Init(void);
void HRSAPP_OnConnection(bool ble);
void HRSAPP_OnDisconnection(bool ble);
void HRSAPP_Measurement(void);
bool HRSAPP_Http_Request(uint32_t socket_id, uint32_t data_len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HEARTRATE_H */
