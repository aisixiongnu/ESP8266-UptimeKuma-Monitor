<div align="center">

# ESP8266 Uptime Kuma Monitor

[简体中文](README.md) | English

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Platform: ESP8266](https://img.shields.io/badge/Platform-ESP8266-green.svg)](https://www.espressif.com/en/products/socs/esp8266)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino-00979D.svg)](https://www.arduino.cc/)
[![Display: SSD1306 OLED](https://img.shields.io/badge/Display-SSD1306%20OLED-blueviolet.svg)](#)

</div>

---

> **Project Introduction**  
> This repository provides an open-source monitor/clock terminal based on the ESP8266 development board and a 0.96-inch SSD1306 OLED display. It supports real-time polling of Uptime Kuma status pages, and combines NTP time synchronization with a third-party API to display a rotating carousel of the current time and lunar calendar dates.

## Features

- **Status Display**: Supports displaying monitor service names, real-time status (graphical indicators `√`/`×`/`?`), and 24-hour uptime rate.
- **Calendar Clock**: Large time display + Gregorian date with weekday + lunar date.
- **Auto Carousel**: Automatically switches between two pages every 15 seconds to extend OLED lifespan and prevent burn-in.
- **High Availability Design**: Automatic time sync via NTP, with data caching and fallback mechanism for network failures.
- **Extreme Stability**: JSON parsing is deeply optimized for ESP8266's limited RAM (~80KB) to prevent frequent restarts.

---
## Project Preview

|<img src="images/photo-1.jpg" width="280"/>|<img src="images/photo-2.jpg" width="280"/>|<img src="images/photo-3.jpg" width="280"/>|
|:---:|:---:|:---:|
| Boot Animation | Uptime Kuma Monitor | Clock & Lunar Calendar |
---

## Hardware Wiring

| ESP8266 (NodeMCU/WeMos D1 mini) | 0.96" OLED (SSD1306 I2C) |
| :------------------------------ | :------------------------- |
| `3.3V`                          | `VCC`                      |
| `GND`                           | `GND`                      |
| `D2` (GPIO4)                    | `SDA`                      |
| `D1` (GPIO5)                    | `SCL`                      |

### Wiring Diagram
![Wiring Diagram](images/ESP8266-UptimeKuma-Monitor_wiring-diagram.jpg)

### Pinout Reference
|<img src="images/wiring-1.jpg" width="400"/>|<img src="images/wiring-2.jpg" width="400"/>|
|:---:|:---:|
| ESP8266 Board Pinout | OLED Display Pinout |
---

## Dependencies & Compilation Environment

> Installation: Arduino IDE -> Tools -> Manage Libraries -> Search and install the corresponding versions.

| Library / Board Core | Recommended Version | Description |
| :--- | :--- | :--- |
| **ESP8266 Board Core** | `2.7.4` | Install via Board Manager (ESP8266 Core) |
| **U8g2** | `2.36.19` | OLED graphics rendering and dot-matrix font support |
| **ArduinoJson** | `7.2.0` | **Must use V7** (code written for V7, V6 incompatible) |

---

### How to Configure Status Page and Get Monitor IDs (`monitorIds`)?

Before configuring the code, you need to create a public status page in Uptime Kuma and extract the monitor IDs.

#### Step 1: Create a Status Page in Uptime Kuma
1. Log in to your **Uptime Kuma** admin panel.
2. Click **"Status Page"** in the top navigation bar -> Click **"Add New Status Page"**.
3. Fill in the title and set the path Slug.
4. In the edit area, click **"Add Monitor"** and select the services you want to display (**recommended max 3**).
5. Click **"Save"** at the bottom right.

---

### Step 2: Extract Monitor IDs (`monitorIds`) and Names (`monitorNames`)

1. After creation, run the `curl` command in your terminal (or directly access the API link in your browser):

```bash
curl -k https://your-domain.com/api/status-page/heartbeat/your-slug
```

2. In the returned JSON data, find the **`publicGroupList`** -> **`monitorList`** node. You will see the list of monitors you added along with their corresponding `id` and `name`:

```json
Example

"monitorList": [
  {"id": 4, "name": "Blog1", ...},
  {"id": 3, "name": "Blog2", ...},
  {"id": 5, "name": "Blog3", ...}
]
```

3. Extract the numeric IDs and fill them into the `monitorIds` array in the code, and fill the corresponding names (Only English letters and numbers are supported) into the `monitorNames` array:

```cpp
Example

const char* monitorIds[MONITOR_COUNT]   = {"4", "3", "5"};
const char* monitorNames[MONITOR_COUNT] = {"Blog1", "Blog2", "Blog3"};
```

---


### How to Get the Juhe Data API Key

1. Visit [Juhe Data](https://www.juhe.cn) to register an account and complete real-name verification.
2. Search for "生辰助手" (Birthday Helper) and apply for the API service.
3. Find the AppKey in "My API" and paste it into the code.
> Why use Juhe Data? <br> It was the first one I tried and it worked immediately, so I didn't want to bother signing up for another platform.
---

## Configuration & Flashing

Before flashing the code to the ESP8266, modify the **`User Configuration Area`** at the top of the code according to your environment:

```cpp
// ========== User Configuration Area ==========
const char* ssid = "YOUR_WIFI_SSID";           // Fill in 2.4G WiFi SSID
const char* password = "YOUR_WIFI_PASSWORD";   // Enter WiFi password
const char* JUHE_KEY = "YOUR_JUHE_KEY";        // Fill in Juhe Data API Key
const char* STATUS_API = "https://your-domain.com/api/status-page/heartbeat/your-slug"; // Uptime Kuma status page API

// Monitor list (max 3 recommended, screen cannot fit more)
const int MONITOR_COUNT = 3;
const char* monitorIds[MONITOR_COUNT]   = {"4", "3", "5"};       // Corresponding Uptime Kuma monitor IDs 
const char* monitorNames[MONITOR_COUNT] = {"Blog1", "Blog2", "Blog3"}; // Names displayed on OLED (English or numbers only)
// ================================
```

---

## Why Are Monitor Items Hardcoded?

**To maximize saving ESP8266's precious RAM (~80KB)**. Dynamically fetching the monitor list requires additional HTTP requests and JSON parsing, which can easily lead to memory exhaustion or system crashes. This design fixes 3 monitor items for maximum stability and reliability.

## Notes

- Chinese monitor names are not supported (only "农历月" is drawn via dot-matrix, all other text must be English or numbers).
- Maximum 3 monitor items can be displayed. If you need more, you must modify the code yourself (but may exceed screen space or memory).

## License

This project is open-sourced under the [GPLv3](LICENSE) license.

Welcome for personal learning and technical exchange. **Any secondary development, derivative works, or commercial distribution based on this project must remain open-source and be released under the same GPLv3 license.**

---

*This document was translated by AI.*