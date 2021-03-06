#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include "generic_list.h"
#include "shell.h"
#include "io_driver.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "imu_task.h"
#include "mpu9250.h"

#include "sdkconfig.h"
#include "mongoose.h"

const static char* TAG = "webserver";

const static struct mg_str _imu_raw = MG_MK_STR("/imu/raw");
const static struct mg_str _imu_real = MG_MK_STR("/imu/real");
const static struct mg_str _imu_orientation = MG_MK_STR("/imu/orientation");
const static struct mg_str _imu_mag_data = MG_MK_STR("/imu/mag_data");
const static struct mg_str _imu_debug = MG_MK_STR("/imu/debug");

const static struct mg_str _imu_mag_calibrate = MG_MK_STR("/imu/mag_calibrate");

static char _str_buf[512];

static inline void
webapi_not_found(struct mg_connection* nc, struct http_message* hm)
{
  mg_printf(nc, "%s",
      "HTTP/1.1 404 Not Found\r\n"
      "Content-Length: 0\r\n\r\n");
}

static inline void
webapi_imu_raw(struct mg_connection* nc, struct http_message* hm)
{
  imu_sensor_data_t raw, calibrated;
  imu_data_t        data;
  imu_mode_t        mode;

  imu_task_get_raw_and_data(&mode, &raw, &calibrated, &data);

  mg_printf(nc, "%s",
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/json\r\n"
      "Transfer-Encoding: chunked\r\n\r\n");

  mg_printf_http_chunk(nc, "{\"data\": {");

  mg_printf_http_chunk(nc, "accel_raw: [ %d,%d,%d ],",
      raw.accel[0],
      raw.accel[1],
      raw.accel[2]);

  mg_printf_http_chunk(nc, "gyro_raw: [ %d,%d,%d ],",
      raw.gyro[0],
      raw.gyro[1],
      raw.gyro[2]);

  mg_printf_http_chunk(nc, "mag_raw: [ %d,%d,%d ]",
      raw.mag[0],
      raw.mag[1],
      raw.mag[2]);

  mg_printf_http_chunk(nc, "}}");

  mg_send_http_chunk(nc, "", 0);
}

static inline void
webapi_imu_real(struct mg_connection* nc, struct http_message* hm)
{
  imu_sensor_data_t raw, calibrated;
  imu_data_t        data;
  imu_mode_t        mode;

  imu_task_get_raw_and_data(&mode, &raw, &calibrated, &data);

  mg_printf(nc, "%s",
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/json\r\n"
      "Transfer-Encoding: chunked\r\n\r\n");

  mg_printf_http_chunk(nc, "{\"data\": {");
  mg_printf_http_chunk(nc, "accel: [%.2f, %.2f, %.2f],",
      data.accel[0],
      data.accel[1],
      data.accel[2]);
  mg_printf_http_chunk(nc, "gyro: [%.2f, %.2f, %.2f],",
      data.gyro[0],
      data.gyro[1],
      data.gyro[2]);
  mg_printf_http_chunk(nc, "mag: [%.2f, %.2f, %.2f]",
      data.mag[0],
      data.mag[1],
      data.mag[2]);

  mg_printf_http_chunk(nc, "}}");

  mg_send_http_chunk(nc, "", 0);
}

static inline void
webapi_imu_orientation(struct mg_connection* nc, struct http_message* hm)
{
  imu_sensor_data_t raw, calibrated;
  imu_data_t        data;
  imu_mode_t        mode;

  imu_task_get_raw_and_data(&mode, &raw, &calibrated, &data);

  mg_printf(nc, "%s",
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/json\r\n"
      "Transfer-Encoding: chunked\r\n\r\n");

  mg_printf_http_chunk(nc, "{\"data\": {");
  mg_printf_http_chunk(nc, "\"roll\": %.2f, ", data.orientation[0]);
  mg_printf_http_chunk(nc, "\"pitch\": %.2f, ", data.orientation[1]);
  mg_printf_http_chunk(nc, "\"yaw\": %.2f,", data.orientation[2]);
  mg_printf_http_chunk(nc, "\"ax\": %.2f,", data.accel[0]);
  mg_printf_http_chunk(nc, "\"ay\": %.2f,", data.accel[1]);
  mg_printf_http_chunk(nc, "\"az\": %.2f,", data.accel[2]);
  mg_printf_http_chunk(nc, "\"gx\": %.2f,", data.gyro[0]);
  mg_printf_http_chunk(nc, "\"gy\": %.2f,", data.gyro[1]);
  mg_printf_http_chunk(nc, "\"gz\": %.2f,", data.gyro[2]);
  mg_printf_http_chunk(nc, "\"mx\": %.2f,", data.mag[0]);
  mg_printf_http_chunk(nc, "\"my\": %.2f,", data.mag[1]);
  mg_printf_http_chunk(nc, "\"mz\": %.2f", data.mag[2]);
  mg_printf_http_chunk(nc, "}}");

  mg_send_http_chunk(nc, "", 0);
}

static inline void
webapi_imu_mag_data(struct mg_connection* nc, struct http_message* hm)
{
  imu_mode_t        mode;
  int16_t           raw[3];
  int16_t           calibrated[3];
  int16_t           mag_bias[3];

  imu_task_get_mag_calibration(&mode, raw, calibrated, mag_bias);

  mg_printf(nc, "%s",
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/json\r\n"
      "Transfer-Encoding: chunked\r\n\r\n");

  mg_printf_http_chunk(nc, "{\"raw\": {");
  mg_printf_http_chunk(nc, "\"mx\": %d,", raw[0]);
  mg_printf_http_chunk(nc, "\"my\": %d,", raw[1]);
  mg_printf_http_chunk(nc, "\"mz\": %d", raw[2]);
  mg_printf_http_chunk(nc, "},");

  mg_printf_http_chunk(nc, "\"cal\": {");
  mg_printf_http_chunk(nc, "\"mx\": %d,", calibrated[0]);
  mg_printf_http_chunk(nc, "\"my\": %d,", calibrated[1]);
  mg_printf_http_chunk(nc, "\"mz\": %d", calibrated[2]);
  mg_printf_http_chunk(nc, "},");

  mg_printf_http_chunk(nc, "\"mag_bias\": {");
  mg_printf_http_chunk(nc, "\"mx\": %d,", mag_bias[0]);
  mg_printf_http_chunk(nc, "\"my\": %d,", mag_bias[1]);
  mg_printf_http_chunk(nc, "\"mz\": %d", mag_bias[2]);
  mg_printf_http_chunk(nc, "},");

  mg_printf_http_chunk(nc, "\"mode\": %d", mode);

  mg_printf_http_chunk(nc, "}");

  mg_send_http_chunk(nc, "", 0);
}

static inline void
webapi_imu_debug(struct mg_connection* nc, struct http_message* hm)
{
  imu_sensor_data_t raw, calibrated;
  imu_data_t        data;
  imu_mode_t        mode;

  imu_task_get_raw_and_data(&mode, &raw, &calibrated, &data);

  mg_printf(nc, "%s",
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/json\r\n"
      "Transfer-Encoding: chunked\r\n\r\n");

  mg_printf_http_chunk(nc, "{\"data\": {");
  mg_printf_http_chunk(nc, "roll: %.2f, ", data.orientation[0]);
  mg_printf_http_chunk(nc, "pitch: %.2f, ", data.orientation[1]);
  mg_printf_http_chunk(nc, "yaw: %.2f, ", data.orientation[2]);
  mg_printf_http_chunk(nc, "gyro: [%.2f, %.2f, %.2f]",
      data.gyro[0],
      data.gyro[1],
      data.gyro[2]);
  mg_printf_http_chunk(nc, "}}");

  mg_send_http_chunk(nc, "", 0);
}

static inline void
webapi_imu_mag_calibrate(struct mg_connection* nc, struct http_message* hm)
{
  imu_task_do_mag_calibration();

  mg_printf(nc, "%s",
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/json\r\n"
      "Content-Length: 0\r\n\r\n");
}

////////////////////////////////////////////////////////////////
//
// websocket based api implementation
//
////////////////////////////////////////////////////////////////
static inline void
webapi_imu_raw_websocket(struct mg_connection* nc)
{
  imu_sensor_data_t raw, calibrated;
  imu_data_t        data;
  imu_mode_t        mode;

  imu_task_get_raw_and_data(&mode, &raw, &calibrated, &data);

  sprintf(_str_buf, "{\"data\": {"
                    "accel_raw: [ %d,%d,%d ],"
                    "gyro_raw: [ %d,%d,%d ],"
                    "mag_raw: [ %d,%d,%d ]"
                    "}}",
                    raw.accel[0],
                    raw.accel[1],
                    raw.accel[2],
                    raw.gyro[0],
                    raw.gyro[1],
                    raw.gyro[2],
                    raw.mag[0],
                    raw.mag[1],
                    raw.mag[2]);

  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, _str_buf, strlen(_str_buf));
}

static inline void
webapi_imu_real_websocket(struct mg_connection* nc)
{
  imu_sensor_data_t raw, calibrated;
  imu_data_t        data;
  imu_mode_t        mode;

  imu_task_get_raw_and_data(&mode, &raw, &calibrated, &data);

  sprintf(_str_buf, "{\"data\": {"
                    "accel: [%.2f, %.2f, %.2f],"
                    "gyro: [%.2f, %.2f, %.2f],"
                    "mag: [%.2f, %.2f, %.2f]"
                    "}}",
                    data.accel[0],
                    data.accel[1],
                    data.accel[2],
                    data.gyro[0],
                    data.gyro[1],
                    data.gyro[2],
                    data.mag[0],
                    data.mag[1],
                    data.mag[2]);

  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, _str_buf, strlen(_str_buf));
}

static inline void
webapi_imu_orientation_websocket(struct mg_connection* nc)
{
  imu_sensor_data_t raw, calibrated;
  imu_data_t        data;
  imu_mode_t        mode;

  imu_task_get_raw_and_data(&mode, &raw, &calibrated, &data);

  sprintf(_str_buf, "{\"data\": {"
                    "\"roll\": %.2f, "
                    "\"pitch\": %.2f, "
                    "\"yaw\": %.2f,"
                    "\"ax\": %.2f,"
                    "\"ay\": %.2f,"
                    "\"az\": %.2f,"
                    "\"gx\": %.2f,"
                    "\"gy\": %.2f,"
                    "\"gz\": %.2f,"
                    "\"mx\": %.2f,"
                    "\"my\": %.2f,"
                    "\"mz\": %.2f"
                    "}}",
                    data.orientation[0],
                    data.orientation[1],
                    data.orientation[2],
                    data.accel[0],
                    data.accel[1],
                    data.accel[2],
                    data.gyro[0],
                    data.gyro[1],
                    data.gyro[2],
                    data.mag[0],
                    data.mag[1],
                    data.mag[2]);

  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, _str_buf, strlen(_str_buf));
}

static inline void
webapi_imu_mag_data_websocket(struct mg_connection* nc)
{
  imu_mode_t        mode;
  int16_t           raw[3];
  int16_t           calibrated[3];
  int16_t           mag_bias[3];

  imu_task_get_mag_calibration(&mode, raw, calibrated, mag_bias);

  sprintf(_str_buf, "{\"raw\": {"
                    "\"mx\": %d,"
                    "\"my\": %d,"
                    "\"mz\": %d"
                    "},"
                    "\"cal\": {"
                    "\"mx\": %d,"
                    "\"my\": %d,"
                    "\"mz\": %d"
                    "},"
                    "\"mag_bias\": {"
                    "\"mx\": %d,"
                    "\"my\": %d,"
                    "\"mz\": %d"
                    "},"
                    "\"mode\": %d"
                    "}",
                    raw[0],
                    raw[1],
                    raw[2],
                    calibrated[0],
                    calibrated[1],
                    calibrated[2],
                    mag_bias[0],
                    mag_bias[1],
                    mag_bias[2],
                    mode);

  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, _str_buf, strlen(_str_buf));
}

static inline void
webapi_imu_debug_websocket(struct mg_connection* nc)
{
  // FIXME
}

static inline void
webapi_imu_mag_calibrate_websocket(struct mg_connection* nc)
{
  imu_task_do_mag_calibration();
  webapi_imu_mag_data_websocket(nc);
}

static inline void
webapi_imu_handle_web_socket(struct mg_connection* nc, const struct mg_str msg)
{
  // mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, buf, strlen(buf));
  if(mg_strcmp(msg, _imu_raw) == 0)
  {
    webapi_imu_raw_websocket(nc);
  }
  else if(mg_strcmp(msg, _imu_real) == 0)
  {
    webapi_imu_real_websocket(nc);
  }
  else if(mg_strcmp(msg, _imu_orientation) == 0)
  {
    webapi_imu_orientation_websocket(nc);
  }
  else if(mg_strcmp(msg, _imu_mag_data) == 0)
  {
    webapi_imu_mag_data_websocket(nc);
  }
  else if(mg_strcmp(msg, _imu_debug) == 0)
  {
    webapi_imu_debug_websocket(nc);
  }
  else if(mg_strcmp(msg, _imu_mag_calibrate) == 0)
  {
    webapi_imu_mag_calibrate_websocket(nc);
  }
  else
  {
    // FIXME error response
  }
}

static void
mg_ev_handler(struct mg_connection* nc, int ev, void* ev_data)
{
  struct http_message* hm = (struct http_message*)ev_data;

  switch(ev)
  {
  case MG_EV_HTTP_REQUEST:
    if(mg_vcmp(&hm->method, "GET") == 0)
    {
      if(mg_strcmp(hm->uri, _imu_raw) == 0)
      {
        webapi_imu_raw(nc, hm);
      }
      else if(mg_strcmp(hm->uri, _imu_real) == 0)
      {
        webapi_imu_real(nc, hm);
      }
      else if(mg_strcmp(hm->uri, _imu_orientation) == 0)
      {
        webapi_imu_orientation(nc, hm);
      }
      else if(mg_strcmp(hm->uri, _imu_mag_data) == 0)
      {
        webapi_imu_mag_data(nc, hm);
      }
      else if(mg_strcmp(hm->uri, _imu_debug) == 0)
      {
        webapi_imu_debug(nc, hm);
      }
      else
      {
        webapi_not_found(nc, hm);
      }
    }
    if(mg_vcmp(&hm->method, "POST") == 0)
    {
      if(mg_strcmp(hm->uri, _imu_mag_calibrate) == 0)
      {
        webapi_imu_mag_calibrate(nc, hm);
      }
      else
      {
        webapi_not_found(nc, hm);
      }
    }
    else
    {
      webapi_not_found(nc, hm);
    }
    break;

  case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
    ESP_LOGI(TAG, "websocket handshake done");
    break;

  case MG_EV_WEBSOCKET_FRAME:
    {
      struct websocket_message *wm = (struct websocket_message *) ev_data;
      struct mg_str msg = {(char *) wm->data, wm->size};

      ESP_LOGI(TAG, "websocket frame");

      webapi_imu_handle_web_socket(nc, msg);
    }
    break;

  default:
    break;
  }
}


static void
webserver_task(void* pvParameters)
{
  struct mg_mgr           mgr;
  struct mg_connection*   nc;

  mg_mgr_init(&mgr, NULL);

  nc = mg_bind(&mgr, "80", mg_ev_handler);
  if(nc == NULL)
  {
    ESP_LOGE(TAG, "mg_bind failed");
  }

  mg_set_protocol_http_websocket(nc);

  ESP_LOGI(TAG, "entering main loop");

  while(1)
  {
    mg_mgr_poll(&mgr, 1000);
    // vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  mg_mgr_free(&mgr);
}

void
webserver_init(void)
{
  ESP_LOGI(TAG, "initialing webserver");

  xTaskCreate(webserver_task, "webserver_task", 4096, NULL, 5, NULL);
}
