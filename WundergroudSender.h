// WundergroundSender.hpp

#pragma once

#include "config.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ArduinoLog.h>

#ifdef CFG_WUNDG_STATIONID

// Conversion: approximate lux to W/m² (sunlight)
#ifndef LUX_TO_WM2
#define LUX_TO_WM2 0.0079f
#endif

// Magnus constants for dew point calculation
auto constexpr MAGNUS_A = 17.27f;
auto constexpr MAGNUS_B = 237.7f;  // °C

/**
 * calculateDewPoint
 * @param tempC        ambient temperature in °C
 * @param humidity     relative humidity in %
 * @return dew point in °C
 */
static float calculateDewPointC(float tempC, float humidity) {
  // Magnus formula
  float alpha = (MAGNUS_A * tempC) / (MAGNUS_B + tempC) + logf(humidity / 100.0f);
  float dewC  = (MAGNUS_B * alpha) / (MAGNUS_A - alpha);
  return dewC;
}

// URL-encode helper
static String urlencode(const char* str) {
  String encoded;
  char c;
  char buf[5];
  for (size_t i = 0; str[i]; ++i) {
    c = str[i];
    if (isalnum(c)) {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      sprintf(buf, "%%%02X", (uint8_t)c);
      encoded += buf;
    }
  }
  return encoded;
}

/**
 * sendWeatherData
 * @param jsonDoc          JSON document containing weather fields
 *                         temperature (°C), humidity (%), wind_avg_km_h, wind_max_km_h,
 *                         wind_dir_deg, rain_mm, rain_mm_h, rain_mm_d, uv, light_lux,
 *                         current_date ("YYYY-MM-DD HH:MM:SS", optional)
 * @param stationID        Your WU station ID
 * @param stationPassword  Your WU station password
 * @param serverURL        WU endpoint URL (default Weather Underground)
 * @return HTTP status code (negative on error)
 */
static int sendWeatherData(const JsonDocument& jsonDoc,
                           const char* stationID,
                           const char* stationPassword,
                           const char* serverURL = "http://weatherstation.wunderground.com/weatherstation/updateweatherstation.php") {
  // Required: temperature and humidity
  float tempC = jsonDoc["temperature_C"].is<float>() ? jsonDoc["temperature_C"].as<float>() : NAN;
  int   humidity = jsonDoc["humidity"].is<int>()    ? jsonDoc["humidity"].as<int>()    : -1;
  if (isnan(tempC) || humidity < 0) return -1;

  // Convert to imperial
  float tempF = tempC * 9.0 / 5.0 + 32.0;

   // Calculate dew point
  float dewC = calculateDewPointC(tempC, (float)humidity);
  float dewF = dewC * 9.0f / 5.0f + 32.0f;

  // Timestamp
  const char* dateutc = jsonDoc.containsKey("current_date")
                        ? jsonDoc["current_date"].as<const char*>()
                        : "now";

  // Build base URL
  String url = String(serverURL) +
               "?ID="       + stationID +
               "&PASSWORD=" + stationPassword +
               "&dateutc="  + urlencode(dateutc) +
               "&tempf="    + String(tempF, 1) +
               "&dewptf="   + String(dewF, 1) +
               "&humidity=" + String(humidity);

  // Wind: convert km/h to mph
  if (jsonDoc.containsKey("wind_dir_deg_avg")) {
    url += "&winddir=" + String(jsonDoc["wind_dir_deg_avg"].as<int>());
  }
  if (jsonDoc.containsKey("wind_avg_km_h")) {
    float windAvgMph = jsonDoc["wind_avg_km_h"].as<float>() * 0.621371f;
    url += "&windspeedmph=" + String(windAvgMph, 1);
  }
  if (jsonDoc.containsKey("wind_max_km_h")) {
    float windGustMph = jsonDoc["wind_max_km_h"].as<float>() * 0.621371f;
    url += "&windgustmph=" + String(windGustMph, 1);
  }

  // Rain: mm to inches
  if (jsonDoc.containsKey("rain_mm_h")) {
    float rainIn = jsonDoc["rain_mm_h"].as<float>() / 25.4f;
    url += "&rainin=" + String(rainIn, 2);
  }
  if (jsonDoc.containsKey("rain_mm_d")) {
    float dailyIn = jsonDoc["rain_mm_d"].as<float>() / 25.4f;
    url += "&dailyrainin=" + String(dailyIn, 2);
  }

  // UV index remains same
  if (jsonDoc.containsKey("uv")) {
    url += "&UV=" + String(jsonDoc["uv"].as<int>());
  }

 // Solar radiation: convert lux to W/m2 using LUX_TO_WM2
  if (jsonDoc.containsKey("light_lux")) {
    float lux      = jsonDoc["light_lux"].as<float>();
    float solWm2   = lux * LUX_TO_WM2;
    url += "&solarradiation=" + String(solWm2, 1);
  }

  // Finalize
  url += "&action=updateraw";

  Log.notice(F("Wunderground: %s" CR), url.c_str());

  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  yield();
  http.end();
  return code;
}

#endif