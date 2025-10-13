#ifndef HEARTRATE_WIFI_H
#define HEARTRATE_WIFI_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "html_pages.h"

/** Maximum bytes to receive in one step */
#define HTTP_MAX_BYTES_TO_RECEIVE  4096
/** Maximum bytes to send in one step */
#define HTTP_MAX_BYTES_TO_SEND  4096

typedef enum
{
  FAVICON_SVG,
  ST_LOGO_SVG,
  INDEX_HTML,
  HEARTRATE_SENSOR_LOCATION,
  HEARTRATE_CONTROL_POINT,
  HEARTRATE_MEASUREMENT,
  BUTTON_STATE,
  ERROR_404_HTML,
  UNKNOWN_RESPONSE
} HttpServer_response_e;

typedef struct
{
  HttpServer_response_e response_type;
  const char *request;
  const char *response;
} HttpServer_response_t;


/** Response with OK content */
static const char response_ok_html[] =
{
  "HTTP/1.1 200 OK\r\n"
  "Server: U5\r\n"
  "Access-Control-Allow-Origin: * \r\n"
  "Cache-Control: no-cache\r\n"
  "Keep-Alive: timeout=2, max=2\r\n"
  "Connection: close\r\n"
  "Content-Type: text/html; charset=utf-8\r\n"
  "Content-Length: 2\r\n\r\n"
  "OK"
};

/** Response content depending on the request */
HttpServer_response_t http_server_responses[] =
{
  {INDEX_HTML,      "GET / ",                                     response_index_html},
  {FAVICON_SVG,     "GET /favicon.ico",                           response_favicon_svg},
  {ST_LOGO_SVG,     "GET /ST_logo_2020_white_no_tagline_rgb.svg", response_st_logo_svg},
  {HEARTRATE_MEASUREMENT,  "GET /Heartrate_measure",                     NULL},
  {BUTTON_STATE,    "GET /pins_status",                           NULL},
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HEARTRATE_WIFI_H */
