# FSK Weather Station Receiver

This is a comprehensive weather station receiver implementation based on the [rtl_433_ESP library](https://github.com/NorthernMan54/rtl_433_ESP), designed to receive and process FSK wireless weather sensor data from Vevor 7-in-1 weather station (YT60234) using the LILYGO® TTGO LoRa32 V2.1_1.6 **868 MHz** (SX127X) transceiver module. With MQTT auto-discovery, it seamlessly integrates with Home Assistant without any manual configuration.

## Project Overview

This project creates a complete weather data acquisition system that:
- Receives wireless weather sensor data at 868MHz from Vevor 7-in-1 weather station (YT60234)
- Processes FSK-modulated weather station protocol
- Provides real-time web interface with weather data visualization
- Publishes data via MQTT for home automation integration
- **Implements Home Assistant auto-discovery** for seamless integration
- Displays data on local OLED screen
- Supports Over-The-Air (OTA) firmware updates
- Calculates rain accumulation statistics (per minute, hour, day)
- Includes data consistency checking and validation
- Supports Wunderground weather service integration

## Key Features

### 🌡️ Weather Data Collection
- **Temperature**: Celsius measurements with 0.1°C precision
- **Humidity**: Percentage readings (0-100%)
- **Wind Speed**: Average and maximum speeds in km/h
- **Wind Direction**: Degrees (0-360°) with moving average calculation
- **Rainfall**: Total accumulation with time-based statistics
- **UV Index**: Solar radiation measurements
- **Light Level**: Lux measurements with automatic scaling
- **Signal Quality**: RSSI monitoring for reception quality

### 🌐 Connectivity & Integration
- **WiFi Connectivity**: Automatic connection management with reconnection
- **MQTT Publishing**: Home Assistant auto-discovery support
- **Web Interface**: Real-time data dashboard with wind rose visualization
- **NTP Synchronization**: Accurate timestamping for data logs

### 📊 Data Processing & Analytics
- **Rain Statistics**: Minute, hourly, and daily accumulation tracking
- **Moving Averages**: Wind direction smoothing over configurable periods
- **Data Validation**: Temperature, humidity, and sensor range checking
- **Overflow Handling**: Automatic counter overflow detection and correction
- **Message Age Tracking**: Staleness detection by displaying elapsed time from last received transmission

### 🖥️ User Interface
- **Web Dashboard**: Modern responsive interface with live data updates
- **OLED Display**: Local 128x64 pixel status display
- **Wind Rose Visualization**: Real-time wind direction and speed graphics
- **Network Status**: WiFi and MQTT connection monitoring
- **Live Logging**: Real-time system log viewing via web interface

### 🔧 System Management
- **OTA Updates**: Wireless firmware updates via web interface
- **Configuration Management**: Template-based configuration system

## Building and implementing the sample

### Prerequisites
- Visual Studio Code with PlatformIO extension
- LilyGo LoRa32 V2.1 ESP32 development board
- Vevor 7-in-1 weather station (868MHz FSK)

### Quick Start
1. **Clone the repository**
   ```bash
   git clone https://github.com/NorthernMan54/rtl_433_ESP
   ```

2. **Open in VS Code**
   - Start Visual Studio Code
   - Open the `rtl_433_ESP/example/FSK_Receiver` directory

3. **Configure the project**
   - Copy `config.h.template` to `config.h`
   - Edit `config.h` with your WiFi and MQTT settings:
     ```cpp
     #define CFG_SSID "YourWiFiSSID"
     #define CFG_PASSWD "YourWiFiPassword"  
     #define CFG_MQTT_HOST "192.168.1.100"
     #define CFG_MQTT_PORT 1883
     #define CFG_MQTT_USER "mqtt_username"
     #define CFG_MQTT_PASSWD "mqtt_password"
     #define CFG_MQTT_CLIENT_ID "weather_station"

     #define CFG_WUNDG_STATIONID "STATION_ID"   
     #define CFG_WUNDG_PASSWORD "API_KEY"    
     ```

   **Note**: Comment out these lines (add `//` at the beginning) if you want to disable Weather Underground integration:
   ```cpp
   // #define CFG_WUNDG_STATIONID "STATION_ID"    // Disabled Weather Underground
   // #define CFG_WUNDG_PASSWORD "API_KEY"        // Disabled Weather Underground
   ```

4. **Select build environment**
   - Use the `esp32_lilygo_868` environment for LilyGo LoRa32 V2.1 board (868MHz)

5. **Build and upload**
   - Use PlatformIO: Build → Upload
   - Monitor serial output for debugging

## Hardware Setup

### Supported ESP32 Board
- **LilyGo LoRa32 V2.1** - Built-in SX127X transceiver and OLED display (868MHz)

### Display Support
- **128x64 OLED Display** (SSD1306) for local data viewing
- **Real-time data display** with automatic page switching
- **Network status monitoring** on display
- **Connection status indicators**

## LilyGo LoRa32 V2.1 Board Specifications

The LilyGo LoRa32 V2.1 is an integrated ESP32 board with built-in SX127X transceiver and OLED display, specifically configured for 868MHz operation.


### Pin Configuration (Pre-wired on board)
| Function | ESP32 GPIO | Notes |
| -------- | ---------- | ----- |
| LED | 25 | Built-in LED indicator |
| LoRa DIO0 | 26 | SX127X interrupt pin |
| LoRa RST | 14 | SX127X reset pin |
| LoRa CS | 18 | SPI chip select |
| LoRa MOSI | 27 | SPI data output |
| LoRa MISO | 19 | SPI data input |
| LoRa SCK | 5 | SPI clock |
| OLED SDA | 21 | I2C data for display |
| OLED SCL | 22 | I2C clock for display |

**Note**: All connections are pre-wired on the LilyGo board - no additional wiring required.


## Web Interface & API

### Web Dashboard
Access the device via `http://[ESP32_IP_ADDRESS]` to view:

- **Live Weather Data**: Real-time sensor readings with auto-refresh
- **Wind Rose Visualization**: Interactive wind direction and speed display
- **Rain Statistics**: Minute/hourly/daily rainfall accumulation
- **Network Status**: WiFi and MQTT connection monitoring
- **System Information**: Firmware version, signal quality
- **OTA Update Interface**: Wireless firmware update capability
- **Live System Logs**: Real-time debugging and system status

### REST API Endpoints

#### GET `/data` - Current Weather Data
Returns comprehensive JSON weather data:
```json
{
  "id": 12345,
  "temperature": 23.5,
  "humidity": 65,
  "wind_avg_km_h": 8.2,
  "wind_max_km_h": 12.1,
  "wind_dir_deg": 180,
  "wind_dir_deg_avg": 175,
  "rain_mm": 152.3,
  "rain_mm_m": 0.0,
  "rain_mm_h": 2.1,
  "rain_mm_d": 15.7,
  "uv": 6,
  "light_lux": 45000,
  "light_lux_avg": 45000,
  "rssi": -65,
  "last_rx": 12,
  "current_date": "2025-10-15 14:30:45",
  "wifi_status": "Connected",
  "ip_address": "192.168.1.100",
  "mqtt_status": "Connected"
}
```

#### GET `/logs` - System Logs
Returns recent system log entries as plain text.

#### POST `/update` - OTA Firmware Update
Upload new firmware binary for wireless updates.

## MQTT Integration

### Home Assistant Auto-Discovery
The system automatically publishes Home Assistant discovery messages for seamless integration. Simply configure your MQTT broker settings, and all weather sensors will appear automatically in Home Assistant without any manual YAML configuration required.

**Key Benefits:**
- **Zero Configuration**: Devices appear automatically in Home Assistant
- **Complete Device Info**: Proper device classes, units, and icons
- **Real-time Updates**: Live data streaming with proper state updates
- **Device Grouping**: All sensors grouped under single weather station device
- **Automatic Entities**: Temperature, humidity, wind, rain, UV, light sensors created instantly

### MQTT Topics
- **State Topic**: `/{CLIENT_ID}/state` - All weather data in JSON format
- **Discovery**: `homeassistant/sensor/{CLIENT_ID}_*/config` - Auto-discovery payloads
- **Availability**: `/{CLIENT_ID}/status` - Device online/offline status

## Supported Weather Station

This system is specifically designed for:

### Vevor 7-in-1 Weather Station
- **Model**: Vevor Wireless Weather Station 7-in-1
- **Frequency**: 868MHz FSK modulation
- **Manufacturer**: Fujian Youtong Industries Co., Ltd. (rebrand under Vevor name)
- **Reference Models**: YT60231, R53/R56 Fujian Youtong Industries

### Measured Parameters
- **Temperature**: -40°C to +60°C with 0.1°C precision
- **Humidity**: 0-100% RH with 1% precision
- **Wind Speed**: Average and maximum speeds in km/h
- **Wind Direction**: 0-360° with directional accuracy
- **Rainfall**: Total accumulation with 0.233mm resolution
- **UV Index**: 0-16 UV index measurement
- **Light Level**: Lux measurements with automatic scaling
- **Battery Status**: Low battery indication

## Rain Data Processing

### Accumulation Tracking
The system provides sophisticated rain measurement:

- **Total Rainfall**: Cumulative counter from sensor (mm)
- **Minute Rainfall**: Current minute accumulation
- **Hourly Rainfall**: Current hour accumulation  
- **Daily Rainfall**: Current day accumulation
- **Overflow Handling**: Automatic detection and correction for counter overflows

### Rain Validation
- **Monotonic Checking**: Ensures rain values only increase (except overflows)
- **Overflow Detection**: Handles sensor counter resets at maximum values
- **Rate Limiting**: Validates realistic rainfall rates

## Data Quality & Validation

### Enhanced Data Reliability
While the Vevor weather station includes CRC checksums in its payload, CRC validation alone is not sufficient as corrupted data can sometimes pass through. This firmware significantly increases data reliability by performing additional consistency checking on the received data beyond basic CRC validation. Invalid data is discarded before processing/publishing.

### Consistency Checking
Built-in data validation ensures reliable measurements:

```cpp
// Temperature validation - realistic range checking
if (temperature < -40.0°C || temperature > 60.0°C) reject_data();

// Humidity validation - physical limits enforcement
if (humidity < 0% || humidity > 100%) reject_data();

// Signal quality validation - RF signal integrity
if (rssi > 0 dBm || rssi < -120 dBm) reject_data();

// Range validation for UV and light sensors
if (uv_index < 0 || uv_index > 16) reject_data();
if (light_lux < 0 || light_lux > 200000) reject_data();
```

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for:
- Additional weather sensor protocol support
- Web interface improvements
- Documentation updates  
- Bug fixes and optimizations

## License

This project is based on the rtl_433_ESP library and follows the same licensing terms. See the original repository for license details.

## Acknowledgments

- **[rtl_433_ESP Project](https://github.com/NorthernMan54/rtl_433_ESP)**: Core RF signal processing library
- **[rtl_433 Community](https://github.com/merbanan/rtl_433)**: Extensive weather sensor protocol database
- **[PlatformIO](https://platformio.org/)**: Development platform and library management
- **ESP32 Community**: Hardware platform and extensive library support
