#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_http_server.h"

const char* ssid = "H";
const char* password = "heheheha";

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define TRIGGER_PIN 12
#define ECHO_PIN 13

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<html>
 <head>
    <title>iwi</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        height: 100vh;
        font-family: Arial, sans-serif;
      }
      .button {
        margin: 10px;
        padding: 10px 20px;
        font-size: 16px;
        cursor: pointer;
      }
      #photo {
        max-width: 100%;
        height: auto;
      }
    </style>
 </head>
 <body>
    <h1>ESP-Projekt: Auto mit ESP32 Cam</h1>
    <img src="" id="photo">
    <div>
      <button class="button" id="forward">Vor</button>
      <button class="button" id="left">Links</button>
      <button class="button" id="right">Rechts</button>
      <button class="button" id="backward">Zurück</button>
      <button class="button" id="stop">Aus</button>
    </div>
    <div id="ultrasonic-data">
      Distance: <span id="distance">Laden...</span> cm
    </div>
    <script>
  var ipAddress = "192.168.96.96";
  var port = "80";

  function toggleCheckbox(direction, action) {
    var a = new XMLHttpRequest();
    a.open("GET", "http://" + ipAddress + ":" + port + "/action?go=" + direction + "&action=" + action, true);
    a.send();
  }

  function handleKeyPress(event) {
    switch (event.key) {
      case 'w':
      case 'W':
        toggleCheckbox('forward', 'start');
        break;
      case 'a':
      case 'A':
        toggleCheckbox('left', 'start');
        break;
      case 's':
      case 'S':
        toggleCheckbox('backward', 'start');
        break;
      case 'd':
      case 'D':
        toggleCheckbox('right', 'start');
        break;
      case 'f':
        toggleCheckbox('stop', 'stop');
        break;
    }
  }

  function handleKeyRelease(event) {
    switch (event.key) {
      case 'w':
      case 'W':
        toggleCheckbox('forward', 'stop');
        break;
      case 'a':
      case 'A':
        toggleCheckbox('left', 'stop');
        break;
      case 's':
      case 'S':
        toggleCheckbox('backward', 'stop');
        break;
      case 'd':
      case 'D':
        toggleCheckbox('right', 'stop');
        break;
    }
  }

  function updateUltrasonicData() {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
      if (xhr.readyState === XMLHttpRequest.DONE) {
        if (xhr.status === 200) {
          var distance = xhr.responseText;
          document.getElementById("distance").innerText = distance;
        }
      }
    };
    xhr.open("GET", "/ultrasonic", true);
    xhr.send();
  }

  updateUltrasonicData();
  setInterval(updateUltrasonicData, 1000);

  document.getElementById("photo").src = window.location.href.slice(0, -1) + ":81/stream";
  document.addEventListener('keydown', handleKeyPress);
  document.addEventListener('keyup', handleKeyRelease);
</script>

 </body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123123123");
  if(res != ESP_OK){
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, "\r\n--123123123\r\n", strlen("\r\n--123123123\r\n"));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
  }
  return res;
}

static esp_err_t ultrasonic_handler(httpd_req_t *req){
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance= duration*0.034/2;

  char distanceStr[10];
  snprintf(distanceStr, sizeof(distanceStr), "%d", distance);  
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, distanceStr, strlen(distanceStr));

  return ESP_OK;
}


void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t ultrasonic_uri = {
    .uri       = "/ultrasonic",
    .method    = HTTP_GET,
    .handler   = ultrasonic_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
  } else {
    return;
  }

  config.server_port += 1;
  config.ctrl_port += 1;

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  } else {
    httpd_stop(&camera_httpd);
    return;
  }

  config.server_port += 1;
  config.ctrl_port += 1;

  if (httpd_register_uri_handler(camera_httpd, &ultrasonic_uri) != ESP_OK) {
    Serial.println("Error registering ultrasonic URI handler");
    httpd_stop(&camera_httpd);
    httpd_stop(&stream_httpd);
    return;
  }
}


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(false);

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 20;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());

  startCameraServer();
}

void loop() {}
