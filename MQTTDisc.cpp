#include "MQTTDisc.h"
#include <pgmspace.h>
#include <ArduinoLog.h>
#include <string>
#include "config.h"

using namespace std;

//{"model":"Vevor-7in1",
  // "id":63735,
  // "channel":0,
  // "battery_ok":1,
  // "temperature_C":21.6,
  // "humidity":65,
  // "wind_avg_km_h":0,
  // "wind_max_km_h":0,
  // "wind_dir_deg":343,
  // "rain_mm":0.466,
  // "uv":0,
  // "light_lux":110,
  // "light_lux_avg":110
  // "mic":"CHECKSUM",
  // "protocol":"Vevor Wireless Weather Station 7-in-1",
  // "rssi":-52,
  //"duration":85000}

/**********************************************************************
 *  Retained discovery payload (lives in flash only)
 *********************************************************************/
const char DISCOVERY_JSON[] PROGMEM =
R"rawliteral(
{
  "dev": {
    "ids": [")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral("],
    "name": "Outdoor Weather Station",
    "mf":   "Vevor",
    "mdl":  "Vevor-7in1",
    "sw":   "1.0.0"
  },
  "o": { "name": "WST", "sw": "1.0.0" },

  "cmps": {
    "battery_ok": {
      "p": "binary_sensor",
      "device_class": "battery",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.battery_ok }}",
      "payload_on": 0,
      "payload_off": 1,
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_batt_ok)",
      "entity_category": "diagnostic"
    },

    "temperature": {
      "p": "sensor",
      "device_class": "temperature",
      "state_class": "measurement",
      "unit_of_measurement": "°C",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.temperature_C }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_temperature)"
    },

    "hum": {
      "p": "sensor",
      "device_class": "humidity",
      "state_class": "measurement",
      "unit_of_measurement": "%",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.humidity }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_humidity)"
    },

    "wind": {
      "p": "sensor",
      "device_class": "wind_speed",
      "state_class": "measurement",
      "unit_of_measurement": "km/h",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.wind_avg_km_h }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_wind)"
    },

    "wind_direction": {
      "p": "sensor",
      "device_class": "wind_direction",
      "state_class": "measurement_angle",
      "unit_of_measurement": "°",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.wind_dir_deg }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_wdir)"
    },

    "wind_direction_avg": {
      "p": "sensor",
      "name": "Wind direction average",
      "device_class": "wind_direction",
      "state_class": "measurement_angle",
      "unit_of_measurement": "°",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.wind_dir_deg_avg }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_wdir_avg)"
    },

    "rain": {
      "p": "sensor",
      "device_class": "precipitation",
      "state_class": "total_increasing",
      "unit_of_measurement": "mm",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.rain_mm }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_rain)"
    },

    "rain_mm_m": {
      "p": "sensor",
      "name": "Precipitation last minute",
      "device_class": "precipitation",
      "state_class": "measurement",
      "unit_of_measurement": "mm",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.rain_mm_m }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_rain_mm_m)"
    },

    "rain_mm_h": {
      "p": "sensor",
      "device_class": "precipitation",
      "name": "Precipitation last hour",
      "state_class": "measurement",
      "unit_of_measurement": "mm",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.rain_mm_h }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_rain_mm_h)"
    },

    "rain_mm_d": {
      "p": "sensor",
      "device_class": "precipitation",
      "name": "Precipitation last day",
      "state_class": "measurement",
      "unit_of_measurement": "mm",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.rain_mm_d }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_rain_mm_d)"
    },

    "lux": {
      "p": "sensor",
      "device_class": "illuminance",
      "state_class": "measurement",
      "unit_of_measurement": "lx",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.light_lux }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_lux)"
    },

    "lux_avg": {
      "p": "sensor",
      "device_class": "illuminance",
      "state_class": "measurement",
      "unit_of_measurement": "lx",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.light_lux_avg }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_lux_avg)"
    },

    "uv_index": {
      "p": "sensor",
      "state_class": "measurement",
      "unit_of_measurement": "UV index",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.uv }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_uv)"
    },

    "rssi": {
      "p": "sensor",
      "device_class": "signal_strength",
      "state_class": "measurement",
      "unit_of_measurement": "dBm",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.rssi }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_rssi)",
      "entity_category": "diagnostic"
    },

    "msg_age": {
      "p": "sensor",
      "device_class": "duration",
      "state_class": "measurement",
      "unit_of_measurement": "s",
      "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
      "value_template": "{{ value_json.msg_age }}",
      "unique_id": ")rawliteral" CFG_MQTT_CLIENT_ID R"rawliteral(_msg_age)",
      "entity_category": "diagnostic"
    }
  },

  "state_topic": ")rawliteral" CFG_MQTT_STATE_TOPIC R"rawliteral(",
  "qos": 2,
  "encoding": "utf-8"
}
)rawliteral";


void publishDiscovery(PubSubClient& mqtt) {
  mqtt.publish_P("homeassistant/device/" CFG_MQTT_CLIENT_ID "/config",
                 DISCOVERY_JSON, true /*retain*/);
}

void publishState(PubSubClient& mqtt, const char *state_topic, JsonDocument& jsondata){
  string JSONmessageBuffer; //[measureJson(jsondata) + 1];
  serializeJson(jsondata, JSONmessageBuffer);
  Log.notice(F("Publishing %s" CR),JSONmessageBuffer.c_str());
  mqtt.publish(state_topic, JSONmessageBuffer.c_str(), /*retain=*/false);
}