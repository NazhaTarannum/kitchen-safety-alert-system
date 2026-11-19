# Smart Kitchen Safety Alert System (ESP32 & Blynk IoT)

An IoT-enabled real-time kitchen hazard monitoring and automated mitigation system built on the **ESP32** microcontroller. Designed specifically for indoor environmental thresholds in Bangladesh, it monitors combustible gas leaks, open flames, ambient temperature, and humidity, calculating a live **Safety Score** and triggering automated hardware responses alongside mobile push alerts.

## Key Features
* **Multi-Sensor Hazard Detection:** Monitors LPG/combustible gases (`MQ-2`), infrared flame signatures (`Flame Sensor`), and high temperature/humidity (`DHT22`).
* **Automated Mitigation:** Automatically triggers an exhaust fan via a `2N2222` transistor driver when gas, heat, or humidity exceeds safe thresholds.
* **Smart Risk Scoring Engine:** Computes a real-time `0–100` Safety Score, assigns a risk tier (`SAFE`, `CAUTION`, `HIGH RISK`, `CRITICAL`), and pushes actionable recommendations to the mobile dashboard.
* **Blynk Cloud Integration:** Features live sensor telemetry, rate-limited push notifications (`gas_alert` and `fire_alert`), remote buzzer mute (`V6`), manual exhaust fan override (`V7`), and a 2-second hardware diagnostic self-test (`V8`).
* **Offline Fail-Safe Mode:** Continues local LED, buzzer, and exhaust fan protection even if Wi-Fi connectivity drops.

## Hardware Pinout (ESP32)
| Component | ESP32 Pin | Type | Purpose |
| :--- | :--- | :--- | :--- |
| **MQ-2 Gas Sensor** | `GPIO 32` | Digital Input | Detects gas leaks (`LOW` = Alert) |
| **Flame Sensor** | `GPIO 33` | Analog Input | Detects IR flame (`< 2000` = Alert) |
| **DHT22 Sensor** | `GPIO 4` | Digital I/O | Monitors Temp (`> 38.0°C`) & Humidity (`> 85.0%`) |
| **Exhaust Fan (2N2222)** | `GPIO 5` | Digital Output | Activates ventilation fan |
| **Status LED (Green)** | `GPIO 22` | Digital Output | Indicates normal operation |
| **Alert LED (Red)** | `GPIO 23` | Digital Output | Indicates active hazard |
| **Active Buzzer** | `GPIO 21` | Digital Output | Audible alarm |

## Repository Structure
* `/firmware` — ESP32 Arduino C++ source code and configuration template.
* `/docs` — Full Project Proposal and Technical Report.

## Setup Instructions
1. Clone this repository.
2. Duplicate `firmware/secrets.example.h` and rename it to `firmware/secrets.h`.
3. Enter your **Blynk Auth Token** and **2.4GHz Wi-Fi credentials** inside `firmware/secrets.h`.
4. Flash `firmware/kitchen_safety_system.ino` to your ESP32 using the Arduino IDE or PlatformIO.