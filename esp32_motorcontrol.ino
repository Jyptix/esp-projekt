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

#define MOTOR_1_PIN_1    33
#define MOTOR_1_PIN_2    32

#define MOTOR_2_PIN_1    26
#define MOTOR_2_PIN_2    25

#define MOTOR_3_PIN_1    14
#define MOTOR_3_PIN_2    27

#define MOTOR_4_PIN_1    18
#define MOTOR_4_PIN_2    19

httpd_handle_t camera_httpd = NULL;

void motor1Forward() {
    digitalWrite(MOTOR_1_PIN_1, HIGH);
    digitalWrite(MOTOR_1_PIN_2, LOW);
    Serial.println("Vor Motor 1");
}

void motor1Backward() {
    digitalWrite(MOTOR_1_PIN_1, LOW);
    digitalWrite(MOTOR_1_PIN_2, HIGH);
    Serial.println("Zurueck Motor 1");
}

void motor1Stop() {
    digitalWrite(MOTOR_1_PIN_1, LOW);
    digitalWrite(MOTOR_1_PIN_2, LOW);
    Serial.println("Stop Motor 1");
}

void motor2Forward() {
    digitalWrite(MOTOR_2_PIN_1, HIGH);
    digitalWrite(MOTOR_2_PIN_2, LOW);
    Serial.println("Vor Motor 2");
}

void motor2Backward() {
    digitalWrite(MOTOR_2_PIN_1, LOW);
    digitalWrite(MOTOR_2_PIN_2, HIGH);
    Serial.println("Zurueck Motor 2");
}

void motor2Stop() {
    digitalWrite(MOTOR_2_PIN_1, LOW);
    digitalWrite(MOTOR_2_PIN_2, LOW);
    Serial.println("Stop Motor 2");
}

void motor3Forward() {
    digitalWrite(MOTOR_3_PIN_1, HIGH);
    digitalWrite(MOTOR_3_PIN_2, LOW);
    Serial.println("Vor Motor 3");
}

void motor3Backward() {
    digitalWrite(MOTOR_3_PIN_1, LOW);
    digitalWrite(MOTOR_3_PIN_2, HIGH);
    Serial.println("Zurueck Motor 3");
}

void motor3Stop() {
    digitalWrite(MOTOR_3_PIN_1, LOW);
    digitalWrite(MOTOR_3_PIN_2, LOW);
    Serial.println("Stop Motor 3");
}

void motor4Forward() {
    digitalWrite(MOTOR_4_PIN_1, HIGH);
    digitalWrite(MOTOR_4_PIN_2, LOW);
    Serial.println("Vor Motor 4");
}

void motor4Backward() {
    digitalWrite(MOTOR_4_PIN_1, LOW);
    digitalWrite(MOTOR_4_PIN_2, HIGH);
    Serial.println("Zurueck Motor 4");
}

void motor4Stop() {
    digitalWrite(MOTOR_4_PIN_1, LOW);
    digitalWrite(MOTOR_4_PIN_2, LOW);
    Serial.println("Stop Motor 4");
}

void stopAll() {
    motor1Stop();
    motor2Stop();
    motor3Stop();
    motor4Stop();
}

static esp_err_t motor_control(httpd_req_t *req){
    char variable[32] = {0,};
    esp_err_t err;
    size_t variable_len = sizeof(variable) - 1;
    const char* uri = req->uri;
    char* query_string = strchr(uri, '?');
    if (query_string != NULL) {
        query_string++;
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    err = httpd_query_key_value(query_string, "go", variable, variable_len);
    if (err != ESP_OK) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char action[32] = {0,};
    size_t action_len = sizeof(action) - 1;
    err = httpd_query_key_value(query_string, "action", action, action_len);
    if (err != ESP_OK) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    if (strcmp(action, "start") == 0) {
        if (strcmp(variable, "forward") == 0) {
            motor1Forward();
            motor2Forward();
            motor3Forward();
            motor4Forward();
        } else if (strcmp(variable, "left") == 0) {
            motor1Forward();
            motor2Forward();
            motor3Backward();
            motor4Backward();
        } else if (strcmp(variable, "right") == 0) {
            motor3Forward();
            motor4Forward();
            motor1Backward();
            motor2Backward();
        } else if (strcmp(variable, "backward") == 0) {
            motor1Backward();
            motor2Backward();
            motor3Backward();
            motor4Backward();
        }
    } else if (strcmp(action, "stop") == 0) {
        stopAll();
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t motor_uri = {
    .uri       = "/action",
    .method    = HTTP_GET,
    .handler   = motor_control,
    .user_ctx  = NULL
  };
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &motor_uri);
  }
  config.server_port += 1;
  config.ctrl_port += 1;
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  pinMode(MOTOR_1_PIN_1, OUTPUT);
  pinMode(MOTOR_1_PIN_2, OUTPUT);
  pinMode(MOTOR_2_PIN_1, OUTPUT);
  pinMode(MOTOR_2_PIN_2, OUTPUT);

  pinMode(MOTOR_3_PIN_1, OUTPUT);
  pinMode(MOTOR_3_PIN_2, OUTPUT);
  pinMode(MOTOR_4_PIN_1, OUTPUT);
  pinMode(MOTOR_4_PIN_2, OUTPUT);

  Serial.begin(115200);
  Serial.setDebugOutput(false);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());

  startCameraServer();
}

void loop() {
  
}
