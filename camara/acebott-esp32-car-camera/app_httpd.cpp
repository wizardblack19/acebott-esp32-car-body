#include <Arduino.h>
#include "esp_http_server.h"
#include "esp_camera.h"

static esp_err_t home(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, "<script>location.href='http://'+location.hostname+':81/'</script>", HTTPD_RESP_USE_STRLEN);
}
static esp_err_t capture(httpd_req_t *req) {
  camera_fb_t *frame = esp_camera_fb_get();
  if (!frame) return httpd_resp_send_500(req);
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t result = httpd_resp_send(req, (const char *)frame->buf, frame->len);
  esp_camera_fb_return(frame);
  return result;
}
static esp_err_t stream(httpd_req_t *req) {
  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  for (;;) {
    camera_fb_t *frame = esp_camera_fb_get();
    if (!frame) return ESP_FAIL;
    char header[96];
    int length = snprintf(header, sizeof(header), "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", unsigned(frame->len));
    esp_err_t result = httpd_resp_send_chunk(req, header, length);
    if (result == ESP_OK) result = httpd_resp_send_chunk(req, (const char *)frame->buf, frame->len);
    esp_camera_fb_return(frame);
    if (result != ESP_OK) return result;
    vTaskDelay(pdMS_TO_TICKS(60));
  }
}
static void route(httpd_handle_t server, const char *path, esp_err_t (*handler)(httpd_req_t *)) {
  httpd_uri_t uri = {};
  uri.uri = path; uri.method = HTTP_GET; uri.handler = handler;
  httpd_register_uri_handler(server, &uri);
}
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.send_wait_timeout = 2;
  httpd_handle_t server = nullptr;
  if (httpd_start(&server, &config) == ESP_OK) {
    route(server, "/", home); route(server, "/capture", capture);
    route(server, "/Stream", stream); // legacy app URL
  }
  config.server_port = 82; config.ctrl_port += 1;
  if (httpd_start(&server, &config) == ESP_OK) {
    route(server, "/stream", stream); route(server, "/Stream", stream);
  }
}
