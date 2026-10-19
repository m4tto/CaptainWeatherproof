/*
 Basic rtl_433_ESP example for OOK/ASK Devices

*/

#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#include <rtl_433_ESP.h>
#include "MQTTDisc.h"
#include "config.h"
#include "time.h"
#include "LogPrinterBuffer.h"
#include "SMA.hpp"
#include "RainStats.hpp"
#include "WundergroudSender.h"

#define FIRMWARE_VERSION "1.7"

#define WUNDG_INTERVAL_MS 60000 // min time between Wunderground uploads

#ifndef RF_MODULE_FREQUENCY
#  define RF_MODULE_FREQUENCY 433.92
#endif

#define JSON_MSG_BUFFER 512

#define HAS_DISPLAY U8X8_SSD1306_128X64_NONAME_HW_I2C // OLED-Display on board
#define OLED_RSTT   U8X8_PIN_NONE // Not reset pin
#define OLED_SDA    21 // ESP32 GPIO21 (Pin21) -- OLED SDA
#define OLED_SCL    22 // ESP32 GPIO22 (Pin22) -- OLED SCL
#define UI_SWITCH_TO_NETWORK_PAGE  23 // After this time (s), the ui shows the network status instead of the measurements

#ifdef HAS_DISPLAY
// OLED Display
#include <U8x8lib.h>
#define DISPLAYFPS 1 // [fps] -> 5 Frames per second ps = 200ms refreseh cycle
#endif



typedef struct {
  uint32_t id;
  uint8_t battery;
  float temperature;
  float humidity;
  float wind_avg_km_h;
  float wind_max_km_h;
  float wind_dir_deg;
  float rain_mm;
  uint32_t uv;
  uint32_t light_lux;
  int32_t rssi;
  uint32_t last_rx;
} DataSet_t;

char messageBuffer[JSON_MSG_BUFFER];

rtl_433_ESP rf; // use -1 to disable transmitter

const char* ssid = CFG_SSID;
const char* password = CFG_PASSWD;

const char* mqtt_host = CFG_MQTT_HOST;
const uint16_t mqtt_port = CFG_MQTT_PORT;
const char* mqtt_user = CFG_MQTT_USER;
const char* mqtt_passwd = CFG_MQTT_PASSWD;
const char* client_id = CFG_MQTT_CLIENT_ID;
const char* state_topic = CFG_MQTT_STATE_TOPIC;


const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

#ifdef HAS_DISPLAY
HAS_DISPLAY u8x8(OLED_RSTT, OLED_SCL, OLED_SDA);
#endif


volatile DataSet_t data;

WiFiClient net;
PubSubClient mqtt(net);
// The rtl_433 callback runs in the library's decoder task, while MQTT, the web
// server and the re-publish logic run in loop(). stateMutex guards everything
// shared between them: data, rainStats, sma, smaLight, stateDocument and
// statePending. MQTT is only ever touched from loop() (PubSubClient is not
// thread safe).
SemaphoreHandle_t stateMutex;
JsonDocument stateDocument; // last processed message, ready to be published
bool statePending = false;
RainStats rainStats;
WebServer server(80);
String logBuffer;
LogBufferPrinter logBufferPrinter;

 SimpleMovingAverage *sma;
 SimpleMovingAverage *smaLight;

const char* upload_form = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <title>Weather Station OTA Update</title>
    <style>
      body { font-family: Arial; padding: 20px; }

      .data-and-wind-container {
        display: flex;
        gap: 10px;
        align-items: stretch;
        margin-top: 10px;
        height: 300px;
      }

      .data-section {
        flex: 1;
        display: flex;
        flex-direction: column;
      }

      .data-section pre {
        flex: 1;
        margin: 0;
        overflow: auto;
        background: #f4f4f4;
        padding: 10px;
        border: 1px solid #ddd;
      }

      .wind-rose-container {
        flex: none;
        width: 300px;
        height: 300px;
        display: flex;
        align-items: center;
        justify-content: center;
        text-align: center;
      }

      .wind-rose-container canvas {
        border: 1px solid #ddd;
        background: #f4f4f4;
      }

      .progress { width: 100%; background: #eee; margin-top: 10px; }
      .bar { height: 20px; background: #4CAF50; width: 0%; }
      #status { margin-top: 5px; white-space: pre-wrap; }
      #logOutput { background:#000; color:#0f0; padding:10px; height:200px; overflow:auto; }
    </style>
  </head>
  <body>
    <h3>Weather Data</h3>
    <div class="data-and-wind-container">
      <div class="data-section">
        <pre id="dataOutput">Loading...</pre>
      </div>
      <div class="wind-rose-container">
        <canvas id="windRose" width="300" height="300"></canvas>
      </div>
    </div>

    <hr>

    <h3>Network Information</h3>
    <div class="network-info">
      <pre id="networkInfo">Loading...</pre>
    </div>

    <hr>

    <h3>OTA Update</h3>
    <form method="POST" action="/update" enctype="multipart/form-data" id="upload_form">
      <input type="file" name="firmware" required>
      <input type="submit" value="Upload">
    </form>
    <div class="progress"><div class="bar" id="bar"></div></div>
    <pre id="status"></pre>
    <p>Firmware Version: <span id="firmwareVersion">Loading...</span></p>

    <hr>

    <h3>Live Logs</h3>
    <pre id="logOutput"></pre>

    <script>
      const form = document.getElementById('upload_form');
      const bar = document.getElementById('bar');
      const status = document.getElementById('status');
      const logOutput = document.getElementById('logOutput');

      form.onsubmit = function(e) {
        e.preventDefault();
        const xhr = new XMLHttpRequest();
        xhr.upload.onprogress = function(e) {
          if (e.lengthComputable) {
            bar.style.width = (e.loaded / e.total * 100) + '%';
          }
        };
        xhr.onload = function() {
          status.textContent = 'Upload finished: ' + xhr.responseText + '. The page will refresh in 7 seconds';
          setTimeout(() => {
            window.location.reload();
          }, 7000);
        };
        xhr.open("POST", "/update", true);
        xhr.send(new FormData(form));
      };

      setInterval(() => {
        fetch('/logs')
          .then(r => r.text())
          .then(text => logOutput.textContent = text);
      }, 1000);

      setInterval(() => {
        fetch('/data')
          .then(r => r.json())
          .then(data => {
            document.getElementById('dataOutput').textContent =
              `Date: ${data.current_date}\n` +
              `ID: ${data.id}\n` +
              `Temperature: ${data.temperature} °C\n` +
              `Humidity: ${data.humidity} %\n` +
              `Wind Avg: ${data.wind_avg_km_h} km/h\n` +
              `Wind Max: ${data.wind_max_km_h} km/h\n` +
              `Wind Direction: ${data.wind_dir_deg} °\n` +
              `Wind Direction Avg: ${data.wind_dir_deg_avg} °\n` +
              `Rain: ${data.rain_mm} mm\n` +
              `Rain (Minute): ${data.rain_mm_m} mm\n` +
              `Rain (Hour): ${data.rain_mm_h} mm\n` +
              `Rain (Day): ${data.rain_mm_d} mm\n` +
              `UV Index: ${data.uv}\n` +
              `Light: ${data.light_lux} lux\n` +
              `Light Avg (1m): ${data.light_lux_avg} lux\n` +
              `RSSI: ${data.rssi} dBm\n` +
              `Last RX: ${data.last_rx} s ago`;
          });
      }, 1000);

      function drawWindRose(canvas, windSpeed, windDirection) {
        const ctx = canvas.getContext('2d');
        const cx = canvas.width / 2;
        const cy = canvas.height / 2;
        const r = Math.min(cx, cy) - 40;

        // Clear
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        // Draw outer circle
        ctx.beginPath();
        ctx.strokeStyle = '#000';
        ctx.lineWidth = 2;
        ctx.arc(cx, cy, r, 0, 2 * Math.PI);
        ctx.stroke();

        // Draw cardinal directions
        ctx.font = '14px Arial';
        ctx.fillStyle = '#000';
        ctx.textAlign = 'center';
        ctx.fillText('N', cx, cy - r + 15);
        ctx.fillText('S', cx, cy + r - 5);
        ctx.fillText('E', cx + r - 15, cy + 5);
        ctx.fillText('W', cx - r + 15, cy + 5);

        // Calculate arrow endpoints (draw diameter line)
        const angle = (windDirection - 90) * (Math.PI / 180);
        const opposite = angle + Math.PI;
        const startX = cx + r * Math.cos(opposite);
        const startY = cy + r * Math.sin(opposite);
        const endX = cx + r * Math.cos(angle);
        const endY = cy + r * Math.sin(angle);

        // Draw line through center
        // ctx.beginPath();
        // ctx.strokeStyle = '#4CAF50';
        // ctx.lineWidth = 3;
        // ctx.moveTo(startX, startY);
        // ctx.lineTo(endX, endY);
        // ctx.stroke();

        // Draw arrowhead at end
        const headLen = 40;
        const headAng = Math.PI / 40;
        const tipX = cx + (r-headLen) * Math.cos(angle);
        const tipY = cy + (r-headLen) * Math.sin(angle);
        const ax1 = cx + r * Math.cos(angle - headAng);
        const ay1 = cy + r * Math.sin(angle - headAng);
        const ax2 = cx + r * Math.cos(angle + headAng);
        const ay2 = cy + r * Math.sin(angle + headAng);
        ctx.beginPath();
        ctx.moveTo(tipX, tipY);
        ctx.lineTo(ax1, ay1);
        ctx.lineTo(ax2, ay2);
        ctx.closePath();
        ctx.fillStyle = '#4CAF50';
        ctx.fill();

        // Draw wind speed label
        ctx.font = '16px Arial';
        ctx.fillStyle = '#000';
        ctx.textAlign = 'center';
        ctx.fillText(`${windSpeed} km/h`, cx, cy + r + 20);
      }

      setInterval(() => {
        fetch('/data')
          .then(r => r.json())
          .then(d => drawWindRose(
            document.getElementById('windRose'),
            d.wind_avg_km_h,
            d.wind_dir_deg
          ));
      }, 1000);

      setInterval(() => {
        fetch('/data')
          .then(r => r.json())
          .then(data => {
            document.getElementById('networkInfo').textContent =
              `WiFi Status: ${data.wifi_status}\n` +
              `IP Address: ${data.ip_address}\n` +
              `MQTT Status: ${data.mqtt_status}`;
          });
      }, 1000);

      document.getElementById('firmwareVersion').textContent = )rawliteral" FIRMWARE_VERSION R"rawliteral(;
    </script>
  </body>
</html>
  )rawliteral";



// Runs in the rtl_433 decoder task.
void rtl_433_Callback(char* message) {
  JsonDocument jsondata;
  if (deserializeJson(jsondata, message)) {
    Log.error(F("Message discarded: invalid JSON" CR));
    return;
  }
  Log.notice(F("Received message : %s" CR), message);
  processMessage(jsondata);
}

// Sometimes the CRC is not enough and wrong data is passed through.
// This funtion should lower the occurance of publishing wrong messages
bool ConsistencyCheck(const DataSet_t &d){

  if(d.temperature < -40.0 || d.temperature > 60.0)
    return false;

  if(d.humidity < 0.0 || d.humidity > 100.0)
    return false;

  if(d.light_lux < 0 || d.light_lux > 200000)
    return false;

  if(d.uv < 0 || d.uv > 16)
    return false;

  if(d.rssi > 0 || d.rssi < -120)
    return false;

  return true;
}

void processMessage(JsonDocument &jsondata) {
  DataSet_t d = {};
  d.id = jsondata["id"];
  // d.battery = jsondata["battery_ok"];
  d.temperature = jsondata["temperature_C"];
  d.humidity = jsondata["humidity"];
  d.wind_avg_km_h = jsondata["wind_avg_km_h"];

  d.wind_max_km_h = jsondata["wind_max_km_h"];
  d.wind_dir_deg = jsondata["wind_dir_deg"];
  d.rain_mm = jsondata["rain_mm"];

  d.uv = jsondata["uv"];

  d.light_lux = jsondata["light_lux"];
  d.rssi = jsondata["rssi"];

  d.last_rx = xTaskGetTickCount();

  if (!ConsistencyCheck(d)) {
    Log.error(F("Message discarded for inconsistencies" CR));
    return;
  }

  xSemaphoreTake(stateMutex, portMAX_DELAY);

  if (d.id != 0) {
    uint32_t rebasesBefore = rainStats.filter.rebaseEvents();
    rainStats.update(d.id, d.rain_mm, millis(), time(nullptr));
    if (rainStats.filter.rebaseEvents() != rebasesBefore)
      Log.warning(F("Rain counter re-baselined to %F mm" CR), rainStats.filter.acceptedMm());
  }

  // Only the validated counter is exposed. Until it is known, the key is left
  // out so Home Assistant keeps its previous value (a temporary 0 would be
  // seen by total_increasing as a meter reset and turn into a spike).
  if (rainStats.filter.initialized()) {
    d.rain_mm = rainStats.filter.acceptedMm();
    jsondata["rain_mm"] = d.rain_mm;
  } else {
    jsondata.remove("rain_mm");
  }

  memcpy((void*)&data, &d, sizeof(DataSet_t));

  sma->update(d.wind_dir_deg);
  smaLight->update(d.light_lux);

  jsondata["msg_age"] = 0;

  jsondata["rain_mm_m"] = rainStats.minuteMm();
  jsondata["rain_mm_h"] = rainStats.hourMm();
  jsondata["rain_mm_d"] = rainStats.dayMm();
  jsondata["rain_mm_60m"] = rainStats.last60MinMm(time(nullptr));

  jsondata["wind_dir_deg_avg"] = sma->get();

  stateDocument = jsondata;
  statePending = true;

  xSemaphoreGive(stateMutex);
}

// Runs in loop(): publishes the latest processed message, or re-publishes the
// last one with an updated msg_age when resend is true.
void publishLatest(bool resend) {
  JsonDocument out;

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  if (!statePending && !resend) {
    xSemaphoreGive(stateMutex);
    return;
  }
  if (resend)
    stateDocument["msg_age"] = (xTaskGetTickCount() - data.last_rx) / 1000;
  statePending = false;
  out = stateDocument;
  xSemaphoreGive(stateMutex);

  publishState(mqtt, state_topic, out);
}

#ifdef CFG_WUNDG_STATIONID
// Runs in loop(): uploads the latest processed message, at most once every
// WUNDG_INTERVAL_MS and only when a new transmission arrived since the last
// upload. The HTTP call blocks loop() (and so rf.loop()), hence the short
// timeouts in sendWeatherData().
void sendWunderground() {
  static uint32_t lastSendTick = 0;
  static uint32_t lastSentRx = 0;
  static bool sentOnce = false;

  if (!WiFi.isConnected())
    return;
  if (sentOnce && xTaskGetTickCount() - lastSendTick < WUNDG_INTERVAL_MS)
    return;

  JsonDocument out;
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  bool fresh = data.id != 0 && data.last_rx != lastSentRx;
  if (fresh) {
    out = stateDocument;
    lastSentRx = data.last_rx;
  }
  xSemaphoreGive(stateMutex);
  if (!fresh)
    return;

  lastSendTick = xTaskGetTickCount();
  sentOnce = true;
  int code = sendWeatherData(out, CFG_WUNDG_STATIONID, CFG_WUNDG_PASSWORD);
  if (code != 200)
    Log.error(F("Wunderground upload failed: %d" CR), code);
}
#endif

void InitDisplay(const char* Productname, const char* Version) {
  uint8_t buf[32];
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.clear();
  u8x8.setFlipMode(0);
  u8x8.setInverseFont(1);
  u8x8.drawString(0, 0, Productname);
  u8x8.setInverseFont(0);
  u8x8.drawString(5, 0, Version);
  delay(1500);

}

void fill_rem(int row, int start, int size) {
  for (int i = start; i < size; i++)
    u8x8.drawString(i, row, " ");
}

void ui_loop(void* pvParameters) {
  static uint8_t steps = 0;
  static uint8_t page_switch = 0;
  while (1) {
    //u8x8.clear();
    if (data.id == 0) {
      Log.notice(F("Waiting first data..." CR));
      u8x8.setInverseFont(0);
      u8x8.drawString(0, 0, "Waiting data");
      for (int i = 0; i <= steps; i++)
        u8x8.drawString(12 + i, 0, ".");
      for (int i = steps + 1; i < 3; i++)
        u8x8.drawString(12 + i, 0, " ");
      steps = (++steps) % 3;
    } else {
      page_switch++;
      if(page_switch < UI_SWITCH_TO_NETWORK_PAGE){
        char row[17];
        int written;
        written = snprintf(row, sizeof(row), "id: %i", data.id);
        u8x8.drawString(0, 0, row);
        fill_rem(0, written, sizeof(row) - 1);

        written = snprintf(row, sizeof(row), "T:%.1f H:%.1f", data.temperature, data.humidity);
        u8x8.drawString(0, 1, row);
        fill_rem(1, written, sizeof(row) - 1);

        written = snprintf(row, sizeof(row), "W:%.1f WDIR:%.1f", data.wind_avg_km_h, data.wind_dir_deg);
        u8x8.drawString(0, 2, row);
        fill_rem(2, written, sizeof(row) - 1);

        written = snprintf(row, sizeof(row), "R: %.1f", data.rain_mm);
        u8x8.drawString(0, 3, row);
        fill_rem(3, written, sizeof(row) - 1);

        written = snprintf(row, sizeof(row), "L: %i UV:%i", data.light_lux, data.uv);
        u8x8.drawString(0, 4, row);
        fill_rem(4, written, sizeof(row) - 1);

        fill_rem(5, 0, sizeof(row) - 1);

        written = snprintf(row, sizeof(row), "rssi: %i", data.rssi);
        u8x8.drawString(0, 6, row);
        fill_rem(6, written, sizeof(row) - 1);

        written = snprintf(row, sizeof(row), "age: %is", (xTaskGetTickCount() - data.last_rx) / 1000);
        u8x8.drawString(0, 7, row);
        fill_rem(7, written, sizeof(row) - 1);
        // for(int i = written; i <= sizeof(row)-2; i++)
        // u8x8.drawString(i,0," ");
        }
      else if(page_switch < UI_SWITCH_TO_NETWORK_PAGE + 3){
        char row[17];
        int written;

        if(WiFi.isConnected()){
          written = snprintf(row, sizeof(row), "WiFi: OK");
          u8x8.drawString(0, 0, row);
          fill_rem(0, written, sizeof(row) - 1);

          written = snprintf(row, sizeof(row), "IP: %s", WiFi.localIP().toString().c_str());
          u8x8.drawString(0, 1, row);
          fill_rem(1, written, sizeof(row) - 1);
        }
        else{
          written = snprintf(row, sizeof(row), "WiFi: Not OK");
          u8x8.drawString(0, 0, row);
          fill_rem(0, written, sizeof(row) - 1);

          fill_rem(1, written, sizeof(row) - 1);
        }

        if(mqtt.connected()){
          written = snprintf(row, sizeof(row), "MQTT: OK");
          u8x8.drawString(0, 3, row);
          fill_rem(3, written, sizeof(row) - 1);
        }
        else{
          written = snprintf(row, sizeof(row), "MQTT: Not OK");
          u8x8.drawString(0, 3, row);
          fill_rem(3, written, sizeof(row) - 1);
        }

        fill_rem(2, 0, sizeof(row) - 1);
        fill_rem(4, 0, sizeof(row) - 1);
        fill_rem(5, 0, sizeof(row) - 1);
        fill_rem(6, 0, sizeof(row) - 1);
        fill_rem(7, 0, sizeof(row) - 1);
      }
      else{
        page_switch = 3;
        continue;
      }
    }

    vTaskDelay(1000 / DISPLAYFPS / portTICK_PERIOD_MS);
  }
}

void NetworkStartup(){

  if(WiFi.status() != WL_CONNECTED){
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    unsigned long t0 = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
      delay(500);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {


    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    unsigned long t0 = millis();
    while (!mqtt.connected() && millis() - t0 < 20000) {
      if (mqtt.connect(client_id, mqtt_user, mqtt_passwd)) {
        publishDiscovery(mqtt); // publish discovery info for homeassistant
      } else {
        delay(2000); // wait before retry
      }
    }
  }
}


void WiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Log.notice(F("Connected to AP successfully!" CR));
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  Log.notice(F("WiFi connected" CR));
  Log.notice(F("IP address: %s" CR), WiFi.localIP().toString().c_str());
}

// void WiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
//   Log.notice(F("Disconnected from WiFi" CR));
//   Log.notice(F("Trying to Reconnect" CR));
//   NetworkStartup();
// }

void handleRoot() {
  server.send(200, "text/html", upload_form);
}

void handleUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Log.notice("Update started: %s\n", upload.filename.c_str());
    if (!Update.begin()) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Log.notice("Update Success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

void handleUpdateFinished() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  delay(1000);
  ESP.restart();
}

void handleLogs() {
  server.send(200, "text/plain", logBufferPrinter.buffer);
}


void handleData() {
  JsonDocument jsonDoc;
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  jsonDoc["id"] = data.id;
  jsonDoc["temperature"] = data.temperature;
  jsonDoc["humidity"] = data.humidity;
  jsonDoc["wind_avg_km_h"] = data.wind_avg_km_h;
  jsonDoc["wind_max_km_h"] = data.wind_max_km_h;
  jsonDoc["wind_dir_deg"] = data.wind_dir_deg;
  jsonDoc["rain_mm"] = data.rain_mm;
  jsonDoc["uv"] = data.uv;
  jsonDoc["light_lux"] = data.light_lux;
  jsonDoc["rssi"] = data.rssi;
  jsonDoc["last_rx"] = (xTaskGetTickCount() - data.last_rx) / 1000;

  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  char formattedDate[20];
  strftime(formattedDate, sizeof(formattedDate), "%Y-%m-%d %H:%M:%S", timeinfo);

  jsonDoc["current_date"] = formattedDate;
  jsonDoc["rain_mm_m"] = rainStats.minuteMm();
  jsonDoc["rain_mm_h"] = rainStats.hourMm();
  jsonDoc["rain_mm_d"] = rainStats.dayMm();
  jsonDoc["wind_dir_deg_avg"] = sma->get();
  jsonDoc["light_lux_avg"] = smaLight->get();
  xSemaphoreGive(stateMutex);
  jsonDoc["wifi_status"] = WiFi.isConnected() ? "Connected" : "Disconnected";
  jsonDoc["ip_address"] = WiFi.isConnected() ? WiFi.localIP().toString() : "N/A";
  jsonDoc["mqtt_status"] = mqtt.connected() ? "Connected" : "Disconnected";

  String jsonString;
  serializeJson(jsonDoc, jsonString);
  server.send(200, "application/json", jsonString);
}

void printPrefix(Print* _logOutput) {
  printTimestamp(_logOutput);
}

void printTimestamp(Print* _logOutput) {

// Division constants
const unsigned long MSECS_PER_SEC       = 1000;
const unsigned long SECS_PER_MIN        = 60;
const unsigned long SECS_PER_HOUR       = 3600;
const unsigned long SECS_PER_DAY        = 86400;

// Total time
const unsigned long msecs               =  millis();
const unsigned long secs                =  msecs / MSECS_PER_SEC;

// Time in components
const unsigned long Seconds             =  secs  % SECS_PER_MIN ;
const unsigned long Minutes             = (secs  / SECS_PER_MIN) % SECS_PER_MIN;
const unsigned long Hours               = (secs  % SECS_PER_DAY) / SECS_PER_HOUR;

// Time as string
char timestamp[20];
sprintf(timestamp, "%02d:%02d:%02d ", Hours, Minutes, Seconds);
_logOutput->print(timestamp);
}



void setup() {
  Serial.begin(921600);
  delay(1000);
#ifndef LOG_LEVEL
  LOG_LEVEL_SILENT
#endif

  stateMutex = xSemaphoreCreateMutex();
  sma = new SimpleMovingAverage(15);
  smaLight = new SimpleMovingAverage(3); // 3 samples for 1-minute average

  Log.setPrefix(printPrefix); // set prefix similar to NLog
  Log.begin(LOG_LEVEL, &logBufferPrinter);
  Log.notice(F(" " CR));
  Log.notice(F("****** setup ******" CR));
  rf.initReceiver(RF_MODULE_RECEIVER_GPIO, RF_MODULE_FREQUENCY);
  rf.setCallback(rtl_433_Callback, messageBuffer, JSON_MSG_BUFFER);
  rf.setBitRate(250.0);
  rf.enableReceiver();
  rf.receiveDirect(); // Setting the bitrate puts the modem in idle state, we must restart the reception. 
  Log.notice(F("****** setup complete ******" CR));
  rf.getModuleStatus();

  InitDisplay("WST", FIRMWARE_VERSION);
  u8x8.setPowerSave(0);

  memset((void*)&data, 0, sizeof(DataSet_t));
  Log.notice(F("Starting main interface" CR));
  xTaskCreatePinnedToCore(ui_loop, "ui_loop", 2048, (void*)1, (5 | portPRIVILEGE_BIT), NULL, 1);

  WiFi.mode(WIFI_STA); //Optional

  WiFi.onEvent(WiFiConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  
  mqtt.setServer(mqtt_host, mqtt_port);
  mqtt.setBufferSize(8192);

  NetworkStartup();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_POST, handleUpdateFinished, handleUpload);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/data", HTTP_GET, handleData);
  server.begin();


  //WiFi.onEvent(WiFiDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

}

void loop() {

  // uint32_t now;

  rf.loop();
  if (!WiFi.isConnected() || !mqtt.connected()) {    
    NetworkStartup();                  
  }
  mqtt.loop();

  server.handleClient();

  publishLatest(false);
#ifdef CFG_WUNDG_STATIONID
  sendWunderground();
#endif

  uint32_t ticks = xTaskGetTickCount();
  uint32_t age = ticks - data.last_rx;

  // Restart the board if we cannot receive any message for more than 1 minute
  if(data.id != 0 && age > 60000){
    Log.error(F("Restarting after timeout" CR));
    ESP.restart();
  }

  // The station publish the data every 20s. If we miss a transmission, the last message is sent again with updated msg_age every 10s.
  static uint32_t lastResend = 0;
  if(data.id != 0 && age > 23000 && ticks - lastResend >= 10000){
    lastResend = ticks;
    publishLatest(true);
  }
}