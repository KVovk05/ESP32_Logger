# ESP32-S3 IoT Environmental Logger (`ESP32_Logger`)

An ESP-IDF environmental monitoring and telemetry logger built for the **ESP32-S3**. The system collects real-time temperature, humidity, and atmospheric pressure data from a **BME280** sensor over a shared **I2C** bus, tracks UTC time via an external **DS1307 RTC** (automatically seeded via **SNTP**), renders a live local dashboard on an **SSD1306 OLED** display, monitors an analog temperature threshold knob using **ADC Continuous (DMA)** mode, drives an alarm/status **LED** with local threshold logic and remote **MQTT** override (with a dead-man timeout), and publishes JSON telemetry over Wi-Fi.

## 🎥 Demonstration

👉 **[Watch the Project Demonstration Video (Google Drive)](https://drive.google.com/file/d/1KoVMoHIxemjJGaRPyYhGdOW2p2S7CCxY/view?usp=sharing)**

---

## ✨ Key Features

- **Shared Multi-Device I2C Bus (`i2c_bus`)**:
  - Uses the ESP-IDF `i2c_master` driver on a single shared bus (`I2C_NUM_0`) serving the DS1307 RTC (`0x68`), SSD1306 OLED (`0x3C`), and BME280 environmental sensor (`0x76`).
- **BME280 Environmental Sensing (`env_i2c`, `bme280_compensate`)**:
  - Forced-mode burst reads of pressure, temperature, and humidity registers (`0xF7..0xFE`) over I2C (`0x76`).
  - Bosch fixed-point 32-bit/64-bit compensation formulas (`t_fine`, `Q24.8` pressure, `Q22.10` humidity) using factory calibration coefficients read from NVM (`0x88..0xA1`, `0xE1..0xE7`).
- **Real-Time Clock with SNTP Auto-Seeding (`rtc_ds1307`)**:
  - Reads and writes BCD-encoded time on a Tiny RTC DS1307 module.
  - Automatically synchronizes system time with `pool.ntp.org` over SNTP once Wi-Fi connects and seeds the hardware RTC with UTC time.
- **OLED Boot Animation & Live Dashboard (`oled_display`)**:
  - Powered by `u8g2` with a custom ESP-IDF `i2c_master` HAL bridge (`u8g2_hal`).
  - Displays a 5-second blinking wolf boot animation (`32x32` XBM frames) on startup before transitioning to a live 128x64 dashboard showing UTC time, sensor readings, temperature threshold, and LED state/source.
- **DMA-Driven Potentiometer Threshold Control (`ctrl_adc`)**:
  - Uses `adc_continuous` with DMA (`20 kHz` sampling, 12-bit resolution, `12 dB` attenuation) and FreeRTOS task notifications (`vTaskNotifyGiveFromISR`) to average sample frames in the background without blocking the main loop.
  - Maps raw ADC readings (`0..4095`) linearly to a temperature alarm threshold between **15.0 °C and 35.0 °C**.
- **Smart LED Alarm with Remote MQTT Override & Dead-Man Switch (`led_ctrl`, `app_main`)**:
  - Locally activates the alarm LED when the measured temperature exceeds the potentiometer threshold.
  - Supports remote `ON` / `OFF` override via MQTT (`logger/control/led`) with a **60-second dead-man timeout** (`MQTT_OVERRIDE_TIMEOUT_US`) that automatically returns control to the local potentiometer threshold if no new remote command is received.
- **SoftAP Wi-Fi Provisioning (`wifi_prov`)**:
  - Uses `wifi_provisioning` manager in SoftAP mode (`PROV_iot_logger_XXXX`, PoP: `iot_logger_pop`) on first boot and stores credentials in NVS.
  - Automatically resets provisioning and reboots into SoftAP mode if stored credentials fail to connect within 5 minutes.
- **Startup Hardware Self-Test**:
  - Runs an automated diagnostic report (`run_wiring_self_test`) on boot to verify the I2C bus, BME280 sensor, OLED display, DS1307 RTC response, LED blinking, and potentiometer ADC threshold.

---

## 🔌 Hardware Pinout (ESP32-S3)

| Peripheral / Bus | Signal | ESP32-S3 GPIO | Notes / Address |
| :--- | :--- | :--- | :--- |
| **Shared I2C Bus (`I2C_NUM_0`)** | `SDA` | `GPIO 8` | 100 kHz, internal + external 4.7 kΩ pull-ups |
| | `SCL` | `GPIO 9` | Shared by OLED, RTC, and BME280 |
| **SSD1306 OLED (128x64)** | I2C | `GPIO 8 / 9` | I2C Address: `0x3C` |
| **Tiny RTC (DS1307)** | I2C | `GPIO 8 / 9` | I2C Address: `0x68` |
| **BME280 Sensor** | I2C | `GPIO 8 / 9` | I2C Address: `0x76` (`SDO` -> `GND`) |
| **Potentiometer (`ADC1_CH3`)** | Analog In | `GPIO 4` | Maps `0..4095` to `15.0 °C .. 35.0 °C` |
| **Alarm / Status LED** | Digital Out | `GPIO 6` | Active-High output (`led_ctrl.c`) |

---

## 📂 Project Architecture

```text
ESP32_Logger/
├── CMakeLists.txt
├── main/
│   ├── app_main.c                 # Self-test, main 1 Hz telemetry loop, dead-man timer, JSON builder
│   ├── CMakeLists.txt
│   └── Kconfig.projbuild          # Menuconfig options for MQTT broker URI & Client ID
└── components/
    ├── bme280_compensate/         # Bosch BME280 calibration parser & T/P/H compensation math
    ├── ctrl_adc/                  # Continuous ADC + DMA background task for potentiometer threshold
    ├── env_i2c/                   # BME280 driver over shared I2C bus (0x76)
    ├── i2c_bus/                   # Shared I2C master bus initialization (GPIO 8 / GPIO 9)
    ├── led_ctrl/                  # GPIO output driver for the indicator LED (GPIO 6)
    ├── mqtt_link/                 # SNTP time sync + ESP-MQTT telemetry publisher & command subscriber
    ├── oled_display/              # u8g2 SSD1306 driver, I2C HAL callbacks, wolf animation & dashboard
    ├── rtc_ds1307/                # DS1307 RTC driver (burst read & SNTP UTC seeding)
    └── wifi_prov/                 # SoftAP Wi-Fi provisioning with NVS storage & fallback timeout
```

---

## 📡 MQTT Telemetry & Remote Control

### Topics

| Direction | Topic | QoS | Payload Format |
| :--- | :--- | :---: | :--- |
| **Publish (1 Hz)** | `logger/telemetry` | `1` | Unformatted JSON object |
| **Subscribe** | `logger/control/led` | `1` | Plain string: `"ON"` or `"OFF"` |

### Example Telemetry Payload (`logger/telemetry`)

```json
{
  "utc_time": "14:22:09",
  "i2c_ok": true,
  "temp_i2c_c": 23.4,
  "hum_i2c_rh": 45.2,
  "press_i2c_hpa": 1013.2,
  "threshold_c": 26.5,
  "adc_raw": 2355,
  "led_on": false,
  "led_source": "local"
}
```

### Testing with Mosquitto CLI

Subscribe to live telemetry:
```bash
mosquitto_sub -h <BROKER_IP> -t "logger/telemetry" -v
```

Remotely override the LED (activates `"mqtt"` mode for 60 seconds):
```bash
mosquitto_pub -h <BROKER_IP> -t "logger/control/led" -m "ON"
mosquitto_pub -h <BROKER_IP> -t "logger/control/led" -m "OFF"
```

---

## 🚀 Getting Started

### 1. Prerequisites
- **ESP-IDF v5.x** installed and configured.
- **ESP32-S3** development board wired according to the pinout table above.
- An MQTT broker (e.g., **Mosquitto**) reachable on the local Wi-Fi network.
- **ESP SoftAP Provisioning** mobile app (Android / iOS) for initial Wi-Fi setup.

### 2. Configure MQTT Broker
Run `menuconfig` and navigate to **`IoT Logger — MQTT Configuration`**:
```bash
idf.py menuconfig
```
- **MQTT broker URI**: e.g. `mqtt://192.168.1.10:1883`
- **MQTT Client ID**: default `esp32s3-iot-logger-01`

### 3. Build, Flash, and Monitor
```bash
idf.py build
idf.py -p <PORT> flash monitor
```

### 4. First-Boot Wi-Fi Provisioning
1. On initial boot (or after NVS erase), the board starts a SoftAP named `PROV_iot_logger_XXXX`.
2. Connect using the **ESP Provisioning** app with Proof of Possession (PoP): `iot_logger_pop`.
3. Select your 2.4 GHz Wi-Fi network and enter the password.
4. Once connected, the logger synchronizes UTC time via SNTP, updates the DS1307 RTC, connects to the MQTT broker, and begins streaming telemetry every second.

---

## 📄 License

Code in this repository is in the Public Domain (or CC0 licensed, at your option). See [LICENSE](file:///d:/espIDE/workspace/ESP32_Logger/LICENSE) for details.
