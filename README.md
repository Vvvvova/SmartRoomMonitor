# 🏠 SmartRoomMonitor (ACM-1)

**An intelligent ESP32-based climate monitor with smart ventilation detection, mold risk alerts, and Telegram integration.**

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Framework](https://img.shields.io/badge/Framework-Arduino-00979D.svg)](https://www.arduino.cc/)
[![PlatformIO](https://img.shields.io/badge/Build%20System-PlatformIO-orange.svg)](https://platformio.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## ✨ Features

- 📊 **Real-time monitoring** — Temperature, humidity, and dew point displayed on OLED and web dashboard
- 🪟 **Smart window detection** — Automatically detects when you open/close windows based on climate changes
- 🧠 **Physics-based analysis** — Uses Absolute Humidity (g/m³) for accurate ventilation efficiency tracking
- 🍄 **Mold risk alerts** — Warns when dew point margin becomes dangerously low
- 🤖 **Telegram bot** — Get notifications and check status from anywhere
- 🌐 **Web dashboard** — Beautiful dark-themed interface with live charts (Chart.js)
- ⛅ **Weather API** — Integrates outdoor weather data from open-meteo.com for context-aware advice
- 🌙 **Night mode** — OLED display turns off automatically (22:00–09:00)

---

## 📸 Screenshots

| Web Dashboard | OLED Display | Telegram Bot |
|:-------------:|:------------:|:------------:|
| ![Dashboard](docs/images/dashboard.png) | ![OLED](docs/images/oled.png) | ![Telegram](docs/images/telegram.png) |

> 📝 *Add your screenshots to `docs/images/` folder*

---

## 🔧 Hardware

| Component | Model | Connection |
|-----------|-------|------------|
| Microcontroller | ESP32 DevKit V1 | — |
| Temperature/Humidity Sensor | DHT22 (AM2302) | GPIO 14 |
| Display | 0.96" OLED SSD1306 (128×64) | I2C (SDA: 21, SCL: 22) |

### ⚠️ Important Note: Sensor Calibration

This project was built with all components housed in a single enclosed case. The ESP32 generates heat during operation, which affects the DHT22 temperature readings. 

**Calibration offsets are applied in software:**
- Temperature: **−2.0°C** (compensates for chip self-heating)
- Humidity: **+10.9%** (sensor-specific correction)

> 💡 If you use a different enclosure or mount the sensor externally, you may need to adjust these values in `Settings.h`.

---

## 🛠️ Installation

### Prerequisites

- [PlatformIO](https://platformio.org/install) (VS Code extension recommended)
- ESP32 development board
- DHT22 sensor + OLED display (see Hardware section)

### Steps

1. **Clone the repository**
   ```bash
   git clone https://github.com/YOUR_USERNAME/SmartRoomMonitor.git
   cd SmartRoomMonitor
   ```

2. **Create your settings file**
   ```bash
   cp include/Settings.h.example include/Settings.h
   ```

3. **Edit `include/Settings.h`** with your credentials:
   - WiFi SSID & password
   - Telegram bot token (get from [@BotFather](https://t.me/BotFather))
   - Your Telegram Chat ID (get from [@userinfobot](https://t.me/userinfobot))
   - Your location coordinates for weather API

4. **Build & Upload**
   ```bash
   pio run --target upload
   ```

5. **Monitor serial output** (optional)
   ```bash
   pio device monitor
   ```

---

## 🧠 How It Works

### Smart State Machine (v5.2)

The system uses a 4-state machine based on **Absolute Humidity physics**:

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│     ┌──────────┐                                                │
│     │  STABLE  │ ◄────────────────────────────────────┐         │
│     └────┬─────┘                                      │         │
│          │ Humidity drop >3% OR                       │         │
│          │ Temp drop >0.5°C                           │         │
│          ▼                                            │         │
│   ┌──────────────┐                                    │         │
│   │ VENTILATING  │ ─────────────────────────────────► │         │
│   └───────┬──────┘  Window closed detected            │         │
│           │                                           │         │
│     ┌─────┴─────┐                                     │         │
│     ▼           ▼                                     │         │
│ ┌────────────┐ ┌─────────────┐                        │         │
│ │ TARGET_MET │ │ INEFFICIENT │ ───────────────────────┘         │
│ └────────────┘ └─────────────┘                                  │
│  Goal reached   Humidity stopped                                │
│  (≤50% or -15%) dropping (plateau)                              │
└─────────────────────────────────────────────────────────────────┘
```

### Key Algorithms

- **Window Open Detection**: Monitors rapid drops in humidity (>3%) or temperature (>0.5°C)
- **Window Close Detection (Rebound v2.0)**: Tracks temperature recovery trend (+0.15°C over 2 minutes)
- **Plateau Detection v2.0**: Uses a 6-point sliding window (3 min) to detect when drying efficiency drops below threshold
- **Mold Risk**: Alerts when temp-to-dewpoint margin < 3°C

---

## 📁 Project Structure

```
SmartRoomMonitor/
├── include/
│   ├── Settings.h.example   # Template for credentials
│   ├── ClimateMath.h        # Dew point & absolute humidity formulas
│   ├── SensorManager.h      # Sensor & state machine interface
│   ├── DisplayManager.h     # OLED driver interface
│   ├── WebManager.h         # HTTP server interface
│   ├── WeatherManager.h     # Weather API interface
│   └── TelegramManager.h    # Telegram bot interface
├── src/
│   ├── main.cpp             # Entry point & main loop
│   ├── SensorManager.cpp    # Core logic & state machine
│   ├── DisplayManager.cpp   # OLED rendering
│   ├── WebManager.cpp       # Web dashboard & API
│   ├── WeatherManager.cpp   # Weather fetching
│   └── TelegramManager.cpp  # Telegram notifications
├── docs/
│   ├── images/              # Screenshots go here
│   └── ...
├── documentation.md         # Detailed technical docs (Russian)
├── platformio.ini           # Build configuration
└── README.md                # This file
```

---

## 📖 Documentation

For detailed technical documentation including:
- Line-by-line code explanations
- State machine logic in depth
- Thread safety considerations
- Changelog history

📄 **[documentation.md](documentation.md)** — English version  
📄 **[documentation_ru.md](documentation_ru.md)** — Русская версия (Original)

---

## 📡 API Endpoints

| Endpoint | Description |
|----------|-------------|
| `GET /` | Web dashboard (HTML) |
| `GET /api/status` | Current readings + advice (JSON) |
| `GET /api/history` | Historical data for charts (JSON, chunked streaming) |

---

## 🤖 Telegram Commands

| Command | Description |
|---------|-------------|
| `/start` | Subscribe to notifications |
| 🌡️ Статус | Get current temperature & humidity |
| 🔇 / 🔊 | Toggle notification sounds |
| 🔗 Веб-панель | Get link to web dashboard |

> *Note: The Telegram interface is in Russian, as this was built for personal home use.*

---

## 📦 Dependencies

All dependencies are automatically managed by PlatformIO:

- `Adafruit SSD1306` — OLED driver
- `Adafruit GFX` — Graphics primitives
- `DHT sensor library` — DHT22 sensor
- `ArduinoJson` — JSON parsing/serialization
- `ESPAsyncWebServer` — Non-blocking HTTP server
- `AsyncTCP` — TCP library for async server
- `UniversalTelegramBot` — Telegram Bot API

---

## 🎓 About This Project

This project was developed as a personal learning exercise in embedded systems and IoT, using an ESP32 DevKit left over from a university course. It demonstrates:

- FreeRTOS multi-threading on ESP32
- State machine design for real-world sensor applications
- Thread-safe data handling with mutexes
- Responsive web UI with Chart.js
- Telegram bot integration for remote monitoring
- Weather API integration for context-aware recommendations

---

## 📄 License

This project is open source and available under the [MIT License](LICENSE).

---

## 🙏 Acknowledgments

- Weather data provided by [Open-Meteo](https://open-meteo.com/) (free, no API key required)
- Built with [PlatformIO](https://platformio.org/)
- Charts powered by [Chart.js](https://www.chartjs.org/)

