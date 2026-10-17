# Esp32-Wilcoxon-883M
Firmware for Industrial IoT Vibration Monitoring using ESP32 and Wilcoxon 883M sensors.

The dashboard now includes RS485 sensor health, velocity and temperature trends,
configurable delayed alarms, capture history with CSV export, and verified GitHub
firmware installation. All settings pages and simulated update progress are
available in the [hardware-free demo](demo/README.md). Run
`node demo/mock_server.js` to preview locally. Alarm settings persist on the real
gateway; trend and capture history reset at reboot. GitHub updates apply to the
ESP32 gateway, not the Wilcoxon sensor firmware.

# 🏭 Industrial IoT Vibration Monitor (ESP32 + Wilcoxon 883M)

![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Framework](https://img.shields.io/badge/framework-Arduino-orange)
![Version](https://img.shields.io/badge/version-3.11.0-blueviolet)
![Status](https://img.shields.io/badge/status-Production%20Ready-success)

> **A professional-grade IoT gateway firmware for monitoring industrial machinery vibration using the Wilcoxon 883M (RS485) sensor and the Cytron Maker Feather AIoT S3.**

---

## 📋 Project Overview

This firmware transforms a standard Modbus RTU vibration sensor into a modern **Industrial IoT Node**. It provides real-time monitoring of critical machinery health metrics (Velocity, Acceleration, Displacement) via a web dashboard, MQTT, and Modbus TCP.

### ✨ Key Features (v4.8.0)
* **📊 Real-Time Web Dashboard:** Hosted directly on the ESP32. Visualizes live metrics and high-frequency waveform/spectrum graphs using Chart.js.
* **☁️ MQTT Integration:** Publishes health status, metrics, and raw data chunks to any MQTT broker (TLS supported).
* **🔗 Modbus TCP Gateway:** Acts as a bridge, allowing SCADA systems (Ignition, Node-RED) to query the sensor over Wi-Fi.
* **🆔 UUID & Identity:** Auto-detects sensor Serial Number, Firmware Version, and Unique Device UUID.
* **🕹️ PLC Remote Trigger:** New register allows PLCs to trigger data captures remotely.
* **🚨 Smart Alerts:** Local buzzer alarms and LED indicators for Wi-Fi loss or sensor errors.
* **💾 CSV Export:** Download internal metric logs directly from the browser.


### 🆕 Latest Update (v4.8.0)

**Code restructure** – `src/main.cpp` was split into one folder per module:

| Folder | Purpose |
|---|---|
| `config/` | pins, Modbus register addresses, constants |
| `core/` | shared types and global state, `modules.h` umbrella include |
| `utils/` | timestamp, logging, spectrum peak finder |
| `hardware/` | LED status, RS485 direction control |
| `modbus/` | RTU sensor reads (`modbus_rtu`) and PLC register map (`modbus_tcp`) |
| `capture/` | waveform/spectrum download state machine, metrics log |
| `mqtt/` | publishers and command handler |
| `ota/` | OTA and firmware upload |
| `tasks/` | FreeRTOS web and background tasks |
| `web/` | dashboard, settings, system, Modbus pages and routes |

**Fixes**
* MQTT waveform/spectrum chunks now carry **200 points** (was 600, which overflowed the JSON buffer and silently dropped data). Chunk counts are now 67 (waveform) and 31 (spectrum).
* Spectrum peaks are now true local maxima (previously the top 5 bins were neighbours of one peak).
* Capture triggers (web and MQTT) are refused while a download is in progress, preventing mixed-axis waveform/spectrum data.
* Timestamps no longer end in `Z`, because they are local time, not UTC.
* Failed sensor triggers/timeouts back off until the next interval instead of retrying every loop.
* MQTT client ID is unique per device (`esp32-ila-<MAC>`), so several gateways can share a broker.
* `/update` now reports failure when the flash update fails.
* Input validation on `/set_intervals` (>= 10 s) and `/save_rs485` (slave ID 1-247, valid baud).
* Displacement RMS temporaries are initialised (the register is still not read - see TODO in `capture` metrics block).

**New**
* `demo/mock_server.js` - run `node demo/mock_server.js` and open http://localhost:8080 to try the dashboard in a browser with simulated data (no hardware needed).
* `.claude/agents/` - debugging subagents for firmware, Modbus and MQTT.

**Known issues / TODO:** no authentication on web/OTA endpoints, displacement RMS register not read, `platformio.ini` board/platform pinning.
---

## 🛠️ Hardware Requirements

| Component | Description | Notes |
| :--- | :--- | :--- |
| **MCU** | [Cytron Maker Feather AIoT S3](https://www.cytron.io/) | ESP32-S3 with built-in RGB LED & Buzzer |
| **Sensor** | [Wilcoxon 883M](https://wilcoxon.com/) | Digital Vibration Sensor (RS485 Modbus RTU) |
| **Power** | 12V-24V DC PSU | Required to power the 883M sensor |
| **Module** | RS485-to-TTL UART | For ESP32 to communicate with the sensor |

### 🔌 Pin Configuration
| ESP32 Pin | Function |
| :--- | :--- |
| **GPIO 16** | RS485 RX |
| **GPIO 15** | RS485 TX |
| **GPIO 12** | Buzzer (Alarm) |
| **GPIO 3** | User Button (Hold 5s for AP Mode) |
| **GPIO 46** | RGB Status LED |

---
Required Libraries:

- 📡 **PubSubClient** (by Nick O'Leary) – MQTT Client  
- 🧩 **ArduinoJson** (by Benoit Blanchon) – JSON parser (v6 recommended)  
- ⚙️ **ModbusMaster** (by Doc Walker) – RS485 Modbus RTU  
- 🌐 **ModbusTCP** (by Matt Fryer) – Modbus TCP network
---

## 🚀 Installation & Setup

### 1. Flash the Firmware
You can use **PlatformIO** (recommended) or Arduino IDE.
1.  Clone this repository.
2.  Open the folder in VS Code with the PlatformIO extension installed.
3.  Connect your ESP32 via USB.
4.  Click **Upload** (➡️).

### 2. Connect to Wi-Fi
1.  On first boot, the device will create a Wi-Fi Access Point:
    * **SSID:** `ILA_Sensor_AP`
    * **Password:** `12345678`
2.  Connect your phone/laptop to this network.
3.  Open a browser to `http://192.168.4.1`.
4.  Go to **Settings > Wi-Fi** to scan and save your local network credentials.

---

## 📡 API & Communication

### 🟢 Status LED Codes
* 🟢 **Green:** System Healthy (Idle)
* 🟡 **Yellow (Blinking):** Sensor Busy (Downloading Waveform)
* 🔴 **Red:** Sensor Error / Timeout
* ⚪ **White:** Wi-Fi Disconnected
* 🔵 **Cyan:** Metrics Recording Active

### 🏭 Modbus TCP Server Map
The ESP32 acts as a Modbus TCP Slave on **Port 502**. This allows SCADA/PLC systems to read cached data.

#### 1. Scalar Metrics (Standard Input Registers)
Read these registers to get the latest processed values.

| Reg | Description | Scale | Unit |
| :--- | :--- | :--- | :--- |
| **0** | X Accel RMS | x100 | g |
| **1** | X Accel Peak | x100 | g |
| **2** | X Accel RMS (10-5k Hz) | x100 | g |
| **3** | X Accel Peak (10-5k Hz) | x100 | g |
| **4** | X Velocity RMS | x10 | mm/s |
| **5** | X Velocity Peak | x10 | mm/s |
| **6** | X Displacement RMS | x1 | µm |
| **7** | X Displacement Peak | x1 | µm |
| **8** | X True Peak | x100 | g |
| **9** | X Crest Factor | x100 | - |
| **20** | Y Accel RMS | x100 | g |
| **40** | Z Accel RMS | x100 | g |
| **60** | Temperature | x10 | °C |
| **95** | **PLC Trigger Command** | Write `1` | Triggers a new capture |

#### 2. Large Data Access (Waveform & Spectrum)
The Gateway maps the latest captured Waveform and Spectrum data to direct address ranges for fast reading.

| Reg Start | Size | Content | Format |
| :--- | :--- | :--- | :--- |
| **1000** | 13,334 | Time Waveform (Latest Axis) | Int16 (x0.001 g) |
| **20000** | 6,145 | FFT Spectrum (Latest Axis) | UInt16 (x0.001 g) |

---

## ☁️ MQTT Topics & Commands

The device connects to your configured MQTT Broker (supports TLS/SSL) and interacts via JSON payloads.

**Default Base Topic:** `883M` (Configurable in Settings)

### 📤 Published Topics (Device -> Cloud)

| Topic | Payload Type | Description |
| :--- | :--- | :--- |
| `883M/status` | JSON | **Health Check:** Publishes every 5 minutes.<br>Contains: Uptime, RSSI, IP, Serial Number, **UUID**, Free RAM. |
| `883M` | JSON | **Metrics:** Object containing scalar values (RMS, Peak, Temp).<br>**Waveform:** Chunked arrays of raw time data.<br>**Spectrum:** Chunked arrays of frequency data. |

### 📥 Command Topic (Cloud -> Device)

**Topic:** `883M/command`

> **⚠️ CRITICAL REQUIREMENT:**
> Every command **MUST** include the `serial_number` field matching the device's specific serial number.

#### 1. Trigger Data Capture (Single Axis)
```json
{
  "serial_number": "ENTER_DEVICE_SERIAL_HERE",
  "command": "trigger_capture",
  "axis": "X"
}
```
(Valid axis values: "X", "Y", "Z")

### 2. Set Auto-Capture Interval
Updates how often the device automatically cycles through X, Y, and Z axes.

```json

{
  "serial_number": "ENTER_DEVICE_SERIAL_HERE",
  "command": "set_interval",
  "interval_ms": 600000
}
(Example: 600000 = 10 Minutes. Set to 2147483647 to disable auto-capture.)
```

### 3. Remote Reboot
Forces the ESP32 to restart.

```json

{
  "serial_number": "ENTER_DEVICE_SERIAL_HERE",
  "command": "reboot"
}
```


### 4. Request Immediate Metrics
Forces the device to publish the latest scalar metrics (RMS/Peak) to MQTT immediately without waiting for the next interval.

```json
{
  "serial_number": "ENTER_DEVICE_SERIAL_HERE",
  "command": "get_metrics"
}
```
---
### MQTT Message 
  Message type :metrics
  
```text
{
"object
model: "Wilcoxon 883M"            
serial_number: "24120017"           
vendor_url: "Unknown"
timestamp: "2026-01-08T14:55:40Z"
type: "metrics"
data: object
accelRms: object
x: 0
y: 0
z: 0
accelPeak: object
accelRms2: object
accelPeak2: object
accelMetricRms: object
accelMetricPeak: object
accelMetricRms2: object
accelMetricPeak2: object
veloRms: object
veloPeak: object
veloWideRms: object
veloWidePeak: object
displRms: object
displPeak: object
displLowRms: object
displLowPeak: object
truePeak: object
crestFactor: object
stdDeviation: object
temperature: 29.9"
}
-END-
```
 Message type :waveform_chunk

```text

 msg.payload : Object
object
model: "Wilcoxon 883M"
serial_number: "24120017"
timestamp: "2026-01-08T16:07:45Z"
type: "waveform_chunk"  
axis: "Z"          
capture_id: "666179"
chunk_index: 0
total_chunks: 67
data: array[200]
[0 … 9]
0: 0
1: 0.013
2: -0.013
3: -0.002
4: -0.001
5: -0.006
6: -0.01
7: -0.004
8: -0.007
9: -0.012
[10 … 19]
[20 … 29]
[30 … 39]
[40 … 49]
[50 … 59]
[60 … 69]
[70 … 79]
[80 … 89]
[90 … 99]
[100 … 109]
[110 … 119]
[120 … 129]
[130 … 139]
[140 … 149]
[150 … 159]
[160 … 169]
[170 … 179]
[180 … 189]
[190 … 199]
[200 … 209]
[210 … 219]
[220 … 229]
[230 … 239]
[240 … 249]
[250 … 259]
[260 … 269]
[270 … 279]
[280 … 289]
[290 … 299]
[300 … 309]
[310 … 319]
[320 … 329]
[330 … 339]
[340 … 349]
[350 … 359]
[360 … 369]
[370 … 379]
[380 … 389]
[390 … 399]
[400 … 409]
[410 … 419]
[420 … 429]
[430 … 439]
[440 … 449]
[450 … 459]
[460 … 469]
[470 … 479]
[480 … 489]
[490 … 499]
[500 … 509]
[510 … 519]
[520 … 529]
[530 … 539]
[540 … 549]
[550 … 559]
[560 … 569]
[570 … 579]
[580 … 589]
[590 … 599]
```
### How to DRAW the graph
What you plot
X-axis (horizontal) → Time
Y-axis (vertical) → Vibration level


Each number is one vibration measurement taken very fast (thousands per second).

```text
Index : Value (g)
0 :  0
1 :  0.013
2 : -0.013
3 : -0.002
4 : -0.001
5 : -0.006
6 : -0.010
7 : -0.004
8 : -0.007
9 : -0.012
```
The graph looks like this conceptually:
Vibration (g)
```text
  |
0.02 |        ●
  |     ●
  |         ●
0.00 |●-------------------- Time
  |        ●
  |     ●
-0.02|         ●
```
 👉 The line goes up and down around zero
 👉 That up-down motion is machine vibration
 Positive vs Negative (important clarification)
 Positive g → movement in one direction
 Negative g → movement in the opposite direction
⚠️ Not good or bad — just direction

 Message type :spectrum
 
```text
object
model: "Wilcoxon 883M"
serial_number: "24120017"
timestamp: "2026-01-08T16:07:39Z"
type: "spectrum_chunk"
axis: "Y"
capture_id: "659112"
chunk_index: 10
total_chunks: 31
data: array[145]
[0 … 9]
0: 0
1: 0
2: 0
3: 0
4: 0
5: 0
6: 0
7: 0
8: 0
9: 0
[10 … 19]
[20 … 29]
[30 … 39]
[40 … 49]
[50 … 59]
[60 … 69]
[70 … 79]
[80 … 89]
[90 … 99]
[100 … 109]
[110 … 119]
[120 … 129]
[130 … 139]
[140 … 144]
```
1️⃣ What you are drawing

This data is from a SPECTRUM (FFT)

Index : Value
0 : 0
1 : 0
2 : 0
3 : 0
4 : 0
5 : 0
6 : 0
7 : 0
8 : 0
9 : 0

Meaning before drawing

Index → Frequency band (Hz)

Value → Vibration level (g)

2️⃣ How to draw it (basic method – paper / Excel / software)
Step 1: Draw the AXES

X-axis (horizontal) → Frequency

Y-axis (vertical) → Vibration level (g)
```text
Vibration (g)
  |
  |
  |
  |________________________ Frequency (Hz)
```
Step 2: Plot each point
You plot one point per index:

| Frequency Bin | Vibration (g) |
|--------------|---------------|
| 0            | 0             |
| 1            | 0             |
| 2            | 0             |
| 3            | 0             |
| 4            | 0             |
| 5            | 0             |
| 6            | 0             |
| 7            | 0             |
| 8            | 0             |
| 9            | 0             |

👉 All points sit on the zero line
Step 3: Connect or bar-plot
Resulting graph looks like this:
```text
Vibration (g)
0.1 |
0.05|
0.00|■■■■■■■■■■________________ Frequency
     0 1 2 3 4 5 6 7 8 9

OR as a flat line:

Vibration (g)
0.00|——————————————— Frequency
```
