# 📚 SmartRoomMonitor (ACM-1) — Technical Documentation

## 🎯 Project Purpose

**SmartRoomMonitor** is an autonomous climate monitor based on the ESP32 microcontroller. The device measures indoor temperature and humidity, automatically detects when a window is open, provides smart ventilation recommendations, and warns about mold formation risk. The system integrates with a weather API for context-aware advice (e.g., in summer it won't recommend opening windows if outdoor humidity is higher than indoors).

---

## 📋 Table of Contents

1. [Project Architecture](#️-project-architecture)
2. [Module Descriptions](#-module-descriptions)
   - [Settings.h](#1️⃣-settingsh--configuration-center)
   - [ClimateMath.h](#2️⃣-climatemathh--mathematical-formulas)
   - [main.cpp](#3️⃣-maincpp--entry-point-and-orchestration)
   - [SensorManager](#4️⃣-sensormanager--heart-of-the-system)
   - [DisplayManager](#5️⃣-displaymanager--oled-screen)
   - [WeatherManager](#6️⃣-weathermanager--weather-api)
   - [TelegramManager](#7️⃣-telegrammanager--notifications)
   - [WebManager](#8️⃣-webmanager--http-api-and-web-interface)
3. [Threads and Synchronization](#-threads-and-synchronization)
4. [Important Features](#️-important-features)
5. [Dependencies](#️-dependencies)
6. [Changelog](#-changelog)

---

## 🏗️ PROJECT ARCHITECTURE

### File Structure

The project is organized using a modular approach. The **include** folder contains header files (module interfaces and settings), while the **src** folder contains logic implementations.

**Header Files (include/):**
- **Settings.h** — central configuration hub: pins, WiFi, thresholds, calibrations
- **ClimateMath.h** — mathematical formulas for dew point and absolute humidity calculations
- **SensorManager.h** — sensor manager interface and state machine
- **DisplayManager.h** — OLED display management interface
- **WebManager.h** — HTTP server and web panel interface
- **WeatherManager.h** — internet weather retrieval interface
- **TelegramManager.h** — Telegram bot notification interface

**Source Files (src/):**
- **main.cpp** — entry point, all module initialization, main loop
- **SensorManager.cpp** — sensor reading, data filtering, climate state machine
- **DisplayManager.cpp** — OLED screen rendering
- **WebManager.cpp** — HTTP API and embedded web dashboard
- **WeatherManager.cpp** — requests to open-meteo.com weather API
- **TelegramManager.cpp** — notification sending and bot command handling

### Inter-Module Connections

The main.cpp file creates instances of all managers. WebManager and TelegramManager receive a pointer to SensorManager to access current readings. SensorManager receives a pointer to WeatherManager to get outdoor weather when generating advice.

---

## 📄 MODULE DESCRIPTIONS

---

### 1️⃣ Settings.h — Configuration Center

This file contains all project settings in one place.

**Hardware Configuration:**
- DHT22 sensor connected to GPIO pin 14
- OLED display has 128x64 pixel resolution and is connected via I2C at address 0x3C
- I2C uses standard pins: SDA on GPIO 21, SCL on GPIO 22

**Telegram:**
- Stores bot token (obtained from @BotFather)
- Stores admin Chat ID for initial notifications

**Network Connection:**
- Home WiFi network name and password
- NTP server address for time synchronization
- Timezone: UTC+1 (Germany) with daylight saving time support

**Sensor Logic:**
- History buffer size: 500 records (sufficient for approximately 18-25 hours depending on activity)
- Temperature calibration: minus 2 degrees from raw value
- Humidity calibration: plus 10.9% to raw value
- Maximum allowed temperature jump between readings: 2 degrees (larger is considered anomaly)
- EMA smoothing coefficient: 0.2 (80% old value, 20% new)

**Advice Thresholds:**
- Humidity above 60% is considered elevated (risk)
- Humidity below 40% is considered low (dry)
- Humidity drop greater than 2% per interval indicates active ventilation

**Night Mode:**
- Display turns off from 22:00 to 09:00 to save energy and avoid lighting up the room at night

**Weather API:**
- Request URL to open-meteo.com with city coordinates

---

### 2️⃣ ClimateMath.h — Mathematical Formulas

This module contains two key formulas used for climate analysis.

**Absolute Humidity Calculation:**
Takes temperature in Celsius and relative humidity in percent. Returns grams of water per cubic meter of air. This allows correct comparison of indoor and outdoor humidity regardless of temperature. For example, outdoors may have 80% humidity at 5°C, but in absolute terms this is less than 50% humidity at 22°C indoors.

**Dew Point Calculation:**
Takes temperature and relative humidity. Returns the temperature at which water will start condensing from the air. If the difference between wall temperature and dew point is less than 3 degrees — there's risk of condensation and mold formation.

---

### 3️⃣ main.cpp — Entry Point and Orchestration

This file manages system startup and coordinates all modules' operation.

#### Global Objects

At program start, five managers are created: SensorManager (sensors), DisplayManager (screen), WebManager (web server), WeatherManager (weather), TelegramManager (notifications). WebManager and TelegramManager receive a pointer to SensorManager upon creation.

#### Connectivity Check Function

Called every 30 seconds. If WiFi is lost — initiates reconnection without blocking the program. If WiFi is OK — checks if time is synchronized, and if not — requests synchronization with NTP server.

#### Reboot Reason Function

Returns a human-readable string with the last ESP32 restart reason. Useful for diagnostics: shows whether it was normal power-on, software reset, core crash, watchdog trigger, brownout, and other causes.

#### Initialization Procedure (setup)

**Step 1: Power Saving.** CPU frequency is reduced from 240 MHz to 80 MHz. This is sufficient for all tasks and reduces heat and power consumption.

**Step 2: Serial Port.** Initialized at 115200 baud for debug messages.

**Step 3: Display.** OLED screen is initialized and shows "CONNECTING..." message so user sees the system is starting.

**Step 4: WiFi Connection.** Controller is put into client mode and connects to home network. Wait up to 10 seconds (20 attempts at 500 ms each). This is the only blocking wait in the program.

**Step 5: Time Synchronization.** If WiFi is connected, current time is requested from NTP server. Wait up to 5 seconds. Correct time is important for history timestamps.

**Step 6: Weather Fetch.** Initial request to weather API is made. Up to 5 retries if first request fails.

**Step 7: Module Startup.** DHT22 sensor is initialized and a separate FreeRTOS task is started for reading it on Core 1. HTTP server starts on port 80. Telegram bot connects.

**Step 8: Startup Notification.** Message is sent to Telegram that system has rebooted, with restart reason and firmware version.

**Step 9: First Readings.** First sensor reading is taken and display is updated with real data.

#### Main Loop (loop)

Runs continuously after initialization completes.

**Every 30 seconds:** Connectivity and time check (ensureConnectivity function).

**Continuously:** Telegram message processing. Inside TelegramManager.update() there's its own 1.5 second timer between checks.

**Continuously:** Sensor logic update. DHT22 reading itself happens in a separate task every 6 seconds. Here also history logging occurs (every 30 seconds during ventilation or every 3 minutes in stable mode) and advice cache updates.

**Every 10 minutes:** Weather data update via WeatherManager.update().

**Every second:** OLED display update. Less frequent doesn't make sense as changes are visible anyway, more frequent — unnecessary I2C bus load.

**At end of each iteration:** 1 millisecond pause. This gives ESP32 system tasks (WiFi, watchdog) time to run and prevents hangs.

---

### 4️⃣ SensorManager — Heart of the System

This module handles all sensor logic and climate analysis.

#### Data Structures

**History Record** contains three fields: Unix timestamp (seconds since 1970), temperature, and humidity.

**Climate States** — four smart operating modes:
- STABLE — stable monitoring
- VENTILATING — active ventilation (drying)
- TARGET_MET — goal achieved (50% humidity)
- INEFFICIENT — ventilation efficiency dropped (moisture no longer leaving)

#### Stored Data

The module stores current readings (temperature, humidity, dew point), 24-hour average humidity, last valid temperature for anomaly filtering, baseline temperature for window open/close detection, current state machine state, time of entry into current state, ring buffer history of 500 records, advice cache with last update time, and pointer to weather manager.

#### Thread Safety

A mutex is created to protect data during simultaneous access from different tasks. lock() and unlock() functions acquire and release the mutex with 500 ms timeout to avoid infinite hanging.

#### Sensor Reading Task (Core 1)

When begin() is called, a separate FreeRTOS task is started that runs on Core 1. This task works in an infinite loop with exact 6-second intervals. Each iteration reads temperature and humidity from DHT22 sensor (takes about 250 ms), then if data is valid — acquires mutex, calls reading processing, and releases mutex. After that resets watchdog if active and waits until next cycle. Using vTaskDelayUntil ensures the interval is exactly 6 seconds regardless of code execution time.

#### update Function (called from main loop)

Determines logging interval: 30 seconds if ventilating, 3 minutes in stable mode. Acquires mutex and checks if enough time has passed since last record. If yes — adds current readings to history. Also updates advice cache every 2 seconds.

#### Reading Processing and State Machine

**Calibration:** Offset of minus 2 degrees is added to raw temperature, plus 10.9% to humidity. Humidity result is constrained to 0-100% range using `constrain()` function to prevent impossible values.

**Anomaly Filtering:** If difference from previous valid value is greater than 2 degrees — new value is ignored and previous is used. Otherwise EMA smoothing is applied: 80% of previous value plus 20% of new.

**Dew Point Calculation:** Formula from ClimateMath is called for current temperature and humidity.

**State Machine (Smart State Machine v5.2):**

Improved version with adaptive target, sliding window for plateau detection, and smart window close detector.

**Data Flows:**
- **processReading()** — called every 6 seconds with live sensor data. Rebound detection and goal checking happen here.
- **update()** — called from main loop, every 30 sec (during ventilation) writes data to history[] and slopeWindow[] for plateau algorithm.

**slopeWindow[] — Sliding Window for Plateau:**
Ring buffer of 6 AbsHum values. Filled every 30 sec synchronously with history. Covers 3 minutes of data (6 × 30 sec). Used for averaged humidity drop rate calculation.

*In STABLE state:* System monitors for rapid changes. If humidity dropped by 3% or temperature by 0.5°C — transition to VENTILATING. "Baseline" absolute humidity and initial humidity are remembered for adaptive target. Upon entering VENTILATING, plateau and rebound counters are reset. Baseline is updated deterministically every 50 readings (~5 minutes at 6-second intervals).

*In VENTILATING state:*
- **Success (higher priority than plateau):** Adaptive target = `max(50%, startHum - 15%)`. With initial humidity of 70% target will be 55%, with 60% — 50%. When reached — transition to TARGET_MET.
- **Plateau v2.0:** After 3 minutes of ventilation, checking begins. Slope is calculated as difference between newest and oldest values in slopeWindow[]. If slope > -0.15 g/m³ over 3 minutes (very slow drying) — confirmation counter increases. After 15 consecutive such readings (~90 sec) — transition to INEFFICIENT.
- **Rebound Detection v2.0:** Instead of absolute threshold (+0.3°C), rate of change is used. If temperature starts rising — start moment is remembered. If rise reaches +0.15°C over 2 minutes — window is closed, return to STABLE. Fallback on AbsHum rise +0.3 g/m³ for fast detection of obvious cases.

*In TARGET_MET and INEFFICIENT states:* Waiting for window close (via rebound detection) or 1 hour timeout. Upon window close — return to STABLE.

#### Detection Algorithms

**Window Open Detection (STABLE → VENTILATING):**
Current humidity is compared with last record in history[]. If difference > 3% or temperature dropped > 0.5°C — window is open.

**Window Close Detection (Rebound Detection v2.0):**
Temperature rise trend is tracked. Upon first exceeding +0.05°C above reference point, moment is remembered. If rise continues and reaches +0.15°C over 2 minutes — window is closed. Fallback: AbsHum rise of +0.3 g/m³.

**Plateau Detection (Plateau v2.0):**
Uses slopeWindow[] of 6 points over 3 minutes. Slope = newest - oldest. If slope > -0.15 g/m³ (almost no drop) for 90 seconds — transition to INEFFICIENT. Does not trigger in first 3 minutes of ventilation.

---

### 5️⃣ DisplayManager — OLED Screen

Module manages 128x64 OLED display connected via I2C.

#### Initialization

Sets up I2C bus on pins 21 and 22 with 200 ms timeout to prevent hanging if display disconnects. Initializes SSD1306 display driver and clears screen.

#### Night Mode

Checks current hour. If time is between 22:00 and 09:00 — returns true. In night mode screen stays off.

#### Screen Update

If night mode is active — simply clears screen and exits.

**Header** (top line): Device name "ACM-1" and current status. Status is determined by advice code: CRITICAL for critical situations, VENTILATE for ventilation recommendation, OPTIMAL when everything is good, NORMAL otherwise. If state machine is in ventilation mode — shows "DRYING...", if in plateau — "PLATEAU".

**Main Readings** (large font): Temperature on left with one decimal place and C unit. Humidity on right without decimal and % sign.

**Bottom Information** (small font): Dew point with DP label. On bottom line — device IP address for web interface access.

---

### 6️⃣ WeatherManager — Weather API

Module retrieves current weather data from free open-meteo.com API.

#### Update Frequency

Request is made every 10 minutes or on first call. WiFi connection is verified before request.

#### Request Execution

HTTP client is created with 2-second timeout to not block the program for long. GET request is sent to URL from settings. On successful response (code 200) JSON is parsed. Filter is used to extract only needed fields: temperature and humidity. This saves memory as full API response is quite large. Data is saved and marked as valid.

On error, data is marked as invalid and error text is saved for debugging.

#### Absolute Humidity Calculation

Calls formula from ClimateMath for outdoor temperature and humidity. This allows SensorManager to compare indoor and outdoor humidity in absolute terms.

#### Condition Description

Returns text description of weather: "Frost" if temperature below 0, "Humid (Outdoor)" if humidity above 85%, "Normal (Outdoor)" otherwise, "No data" if data is invalid.

---

### 7️⃣ TelegramManager — Notifications

Module provides two-way communication through Telegram bot.

#### Stored Data

Subscriber list — each contains Chat ID, notification disable flag, username. Last climate state for tracking transitions. Alert sent flags for mold and timeout to avoid spamming.

#### Initialization

HTTPS client is created in insecure mode (without certificate verification for simplicity). Bot object is created with token from settings. Owner is added as first subscriber. Welcome message is sent that bot is online.

#### Main Loop

**Message Check** (every 1.5 seconds): New messages are requested from Telegram servers. If present — they are processed.

**TARGET_MET Alert:** "✅ Target reached (50%)..." — humidity returned to normal.

**INEFFICIENT Alert:** "⚠️ Efficiency dropped..." — moisture stopped leaving, no point keeping window open.

**Timeout Alert** (20 minutes of ventilation): If ventilation lasts more than 20 minutes and alert hasn't been sent yet — warning is sent about need to close window to avoid overcooling.

**Mold Risk Alert:** If difference between temperature and dew point is less than 3 degrees and alert hasn't been sent — critical warning is sent. Flag resets only when difference exceeds 3.5 degrees (0.5 degree hysteresis prevents flickering).

#### Command Handling

New users are automatically subscribed to notifications on first message.

**/start:** Sends greeting with username and shows main menu with buttons.

**🌡️ Status:** Sends current readings — indoor temperature and humidity, outdoor temperature if data available, current system advice. Icon depends on advice code: checkmark for good, red circle for critical, yellow for recommendation.

**🔇/🔊 Sound:** Toggles notification mode for user. If were enabled — disables and vice versa.

**🔗 Web Panel:** Sends inline button with link to web interface at current device IP address.

#### Alert Broadcasting

Goes through all subscribers and sends message to those who have notifications enabled.

---

### 8️⃣ WebManager — HTTP API and Web Interface

Module provides HTTP API and beautiful web dashboard.

#### Architecture

Uses asynchronous ESPAsyncWebServer web server that doesn't block main loop. Server starts on standard port 80.

#### Main Page

On root URL request, embedded HTML page is served. Page contains full dashboard in dark theme with ACM-1 logo, system advice block, large temperature/humidity/dew point indicators, two history graphs (humidity and temperature), and expandable section with debug data and weather.

Page uses Chart.js for graphs and updates automatically: readings every 3 seconds, graphs every 60 seconds. Styles are responsive for mobile devices.

#### Status API (lightweight)

Path: /api/status. Returns JSON with current readings, advice and code, plus debug data including average humidity, indoor absolute humidity, weather status, outdoor readings. Called by frontend every 3 seconds.

#### History API (heavy, streaming)

Path: /api/history. Returns JSON array with all history records. Each record contains temperature, humidity, and Unix timestamp.

To prevent memory overflow and watchdog triggers, chunked streaming is used. Data is sent in packets of 32 records. Before each packet, 1 ms pause is made for watchdog. Data is copied from history buffer with mutex held only during portion copying. After copying, JSON is formed for this portion and sent to client. Proper buffer overflow check is implemented during serialization (`snprintf`) — if buffer is full, loop breaks and continues in next chunk.

Algorithm:
1. First chunk starts JSON array with opening bracket
2. Each portion requests up to 32 records starting from current offset
3. Records are serialized to JSON with comma separators
4. When records are exhausted — closing bracket is added and stream ends

This allows sending all 500 records without allocating large memory buffer.

---

## 🔄 THREADS AND SYNCHRONIZATION

### ESP32 Core Distribution

**Core 0 (Protocol Core):** WiFi stack, TCP/IP, Telegram HTTP requests, NTP synchronization, system tasks.

**Core 1 (Application Core):** DHT22 reading task (sensorTask), web request handling, display update, main loop().

### Shared Data Protection

dataMutex mutex protects all mutable SensorManager data: current readings, history buffer, advice cache.

**Read Rule:** Acquire mutex, copy data to local variable, release mutex, use copy.

**Write Rule:** Acquire mutex, write data, release mutex.

**Timeout:** All mutex acquisitions have 100-500 ms timeout to not hang forever if something goes wrong.

---

## ⚠️ IMPORTANT FEATURES

**Static Memory:** History buffer is statically allocated as 500-element array. Dynamic memory allocation in loop() is forbidden to prevent fragmentation.

**Sensor Calibration:** All DHT22 raw values are corrected by constants from Settings.h. Temperature is reduced by 2 degrees, humidity is increased by 10.9%. Humidity result is constrained by `constrain(0, 100)` function to prevent impossible values above 100%.

**Deterministic Baseline:** Baseline values for window open detection are updated every 50 sensor readings (~5 minutes). This guarantees data freshness without using random numbers.

**Adaptive Logging:** History recording interval depends on state: 30 seconds during ventilation for detailed graph, 3 minutes in stable mode to save space.

**Watchdog:** All potentially long operations contain minimum 1 ms pauses to prevent system watchdog triggers.

**Night Mode:** OLED display turns off from 22:00 to 09:00. OLED consumes power only when pixels are lit, so black screen consumes 0 watts.

**Alert Hysteresis:** Telegram mold risk notifications have 0.5 degree hysteresis to not send repeated messages when oscillating near threshold.

**Safe Slope Calculation:** Absolute humidity drop rate calculation is performed only when valid historical data exists (minimum 60 seconds). This prevents false triggers during cold system start.

---

## 🛠️ DEPENDENCIES

The project uses the following libraries:
- **Adafruit SSD1306** — OLED display driver
- **Adafruit GFX** — graphics primitives
- **DHT sensor library** — DHT22 sensor operation
- **Adafruit Unified Sensor** — base class for sensors
- **ArduinoJson** — JSON parsing and generation
- **ESPAsyncWebServer** — asynchronous HTTP server
- **AsyncTCP** — TCP for async server
- **UniversalTelegramBot** — Telegram Bot API

---

## 📜 CHANGELOG

> Versions sorted from newest to oldest. Current version: **v5.2**

---

### v5.2 (Smart Detection) — January 11, 2026
**Focus: Intelligent plateau and window close detection.**

#### Plateau Algorithm v2.0:
- **Sliding Window:** New `slopeWindow[6]` buffer for storing 6 latest AbsHum values. Filled every 30 sec synchronously with history. Covers 3 minutes of data.
- **Averaged Slope:** Slope calculated as difference between newest and oldest values in window (instead of single point).
- **90 sec Confirmation:** Plateau must hold for 15 consecutive readings (~90 sec) before system transitions to INEFFICIENT.
- **First 3 Minutes Protection:** Plateau algorithm doesn't trigger in first 3 minutes of ventilation to prevent false positives.

#### Rebound Detection v2.0:
- **Trend Instead of Threshold:** Instead of waiting for absolute +0.3°C rise, system tracks temperature rise trend.
- **Dynamic Detector:** On temperature rise of +0.15°C over 2 minutes — window is considered closed.
- **Fallback:** Fast detector based on AbsHum +0.3 g/m³ kept for obvious cases.

#### Adaptive Humidity Target:
- **Formula:** `max(50%, startHum - 15%)`
- With initial humidity 70% → target 55%
- With initial humidity 60% → target 50%

#### Fixes:
- **TARGET_MET/INEFFICIENT Race:** Added `else if` to prevent TARGET_MET being overwritten by INEFFICIENT in same cycle.
- **Variable Initialization:** All float variables (`stateEnterAbsHum`, `lastAbsHum`, etc.) now initialized as NAN in constructor.
- **Static → Member:** Variable `baselineUpdateCounter` moved from static to class member.
- **WiFi Check:** TelegramManager now skips all operations if WiFi is disconnected (prevents long timeouts).
- **False VENTILATING Trigger:** Upon transitioning to STABLE from TARGET_MET/INEFFICIENT/VENTILATING, baseline temperature and humidity are now updated. This prevents false "new ventilation" detection when temperature continues dropping by inertia after window closes.
- **60 sec Lockout Period:** After transitioning to STABLE, system doesn't detect new ventilation for 60 seconds. This protects against false positives from wall thermal inertia.
- **WeatherManager Fix:** `setWeatherManager()` is now always called, not just on successful WiFi. This fixes "No Manager" in weather debug.

#### Optimization:
- **Data Flow Synchronization:** slopeWindow[] is filled in update() synchronously with history[], not in processReading(). This ensures consistent intervals (30 sec).
- **Telegram Polling 3 sec:** Polling interval increased from 1.5 to 3 seconds to reduce WiFi load while maintaining responsiveness.
- **Yield in Main Loop:** Added `delay(1)` at end of loop() to yield control to system tasks (WiFi, watchdog).
- **StaticJsonDocument:** In /api/status, DynamicJsonDocument replaced with StaticJsonDocument<512> to prevent heap fragmentation.
- **WiFi/NTP Reconnection:** Added automatic WiFi check and reconnection every 30 sec, NTP resync if time is incorrect.

---

### v5.1 (Stability Fixes) — January 11, 2026
**Focus: Eliminating false positives and improving code reliability.**

#### Critical Fixes:
- **Humidity Validation:** Added `constrain()` function to limit humidity to 0-100% range after calibration. This prevents impossible values (e.g., 105.9%) with high raw sensor readings.
- **Duplicate Assignment Fix:** Removed duplicate `currentTemp = t;` assignment (copy-paste error).
- **Deterministic Baseline:** Random baseline update (`rand() % 50`) replaced with deterministic counter. Now baseline is guaranteed to update every 50 readings (~5 minutes), eliminating stale data situations.
- **Slope Calculation Safety:** Fallback value for slope calculation changed from `currentAbsHum` to `NAN`. Added `foundHistoricalPoint` check — slope calculation only runs with valid historical data (minimum 60 seconds). This prevents false INEFFICIENT triggers on cold start.

#### WebManager Improvements:
- **Buffer Overflow Protection:** Improved `snprintf()` result checking in history streaming. Now correctly handles buffer overflow — when space runs out, serialization loop breaks and continues in next chunk.

---

### v5.0 (Physics Engine) — January 11, 2026
**Focus: Implementing absolute humidity physics model and 4-stage state machine.**

- **Physics-Based Ventilation:** Transition from relative humidity (%) to Absolute Humidity (g/m³) for ventilation efficiency assessment. This eliminates errors caused by air cooling (thermal lag).
- **Smart State Machine:** New 4-stage logic:
    - **TARGET_MET:** Window can be closed, target (50%) reached.
    - **INEFFICIENT:** Ventilation stopped being effective, moisture not leaving.
- **Russian Notifications:** All Telegram notifications translated to Russian and adapted for new statuses.
- **Rebound Detection:** Logging of window close event based on humidity and temperature "bounce back".

---

### v4.4 (Logic Precision) — January 11, 2026
**Focus: Critical logic bug fixes and sensor stabilization.**

- **Critical Logic Fix (Hunter Mode):** Fixed bug in rapid humidity drop detector. Previously system compared current value with itself (due to buffer update order). Now correctly uses previous historical value, "fixing" the ventilation mode trigger.
- **Sensor Health:** Disabled "Burst Read" mode (5 readings in 200 ms), which violated DHT22 specification (max 0.5 Hz) and could cause self-heating. Switched to single stable reading.
- **Technical Passport:** Created `system_architecture.md` file with exhaustive description of all system timers, thresholds, and logic branches.

---

### v4.3 (Optimization & Stability) — January 10, 2026
**Focus: Memory optimization, refactoring, and stabilization.**

- **Memory Optimization:** History buffer size adjusted to **500 points**. This eliminated memory shortage during JSON generation for charts and ensured stable web interface operation.
- **Boot Speed:** NTP synchronization timeout reduced to 10 seconds, significantly speeding up "cold" system start.
- **Code Refactoring:**
    - Introduced `ClimateMath.h` library for centralized calculations (DRY).
    - OLED display logic switched to status code system (instead of string parsing), improving display reliability.
    - Weather API settings moved to `Settings.h`.

---

### v4.2 (Graph & Mobile Polish) — January 9, 2026
**Focus: Graph stabilization, mobile optimization, and code cleanup.**

- **Seamless Sliding Window:** Graph logic changed to strict "sliding window" (3 hours). Graph no longer resets or rebuilds every 4 hours, but smoothly shifts as data comes in.
- **Code Cleanup:** Removed unused long-term history buffers (`dailyBuffer`) and extra libraries, freeing memory and improving stability.
- **Mobile Experience:** Added touch interactivity on graphs (tap shows values), increased touch zones (Hit Radius).
- **Legibility Upgrade:** Main reading font size increased from 2.5rem to 3.5rem for better phone readability. Fixed graph rendering curves and padding.

---

### v4.1 (State Machine & Calibration) — January 8, 2026
**Focus: State machine refinement and algorithm fine-tuning.**

#### State Machine:
- **Refined State Machine:** Introduced strict state machine (`ClimateState`) combining adaptive polling ("Predator Mode") and advice system. Three clear states:
    - **🟢 STABLE:** Monitoring mode (poll every 2 min).
    - **🟡 VENTILATING:** Active ventilation (poll every 10 sec).
    - **🔴 PLATEAU:** Waiting for user action (poll every 10 sec).
- **Context-Aware Plateau:** "PLATEAU! Close (Eff. low)" advice now appears *exclusively* after completing active ventilation phase. False positives in idle state completely eliminated.

#### Calibration and Fine-Tuning:
- **Sensor Calibration:** Final corrections applied: Temperature **-2.0°C**, Humidity **+10.9%** to match reference.
- **Smart Detection:** Window open detection sensitivity set to **0.5°C** temperature drop.
- **Mold Risk Logic:** Algorithm calibrated. Risk zone < 3.0°C margin to dew point.
- **Time Sync Fix:** Timestamp filter added. Records with year < 2020 (before NTP sync) are ignored, eliminating bug with "01:00" point appearing at graph start.

---

### v4.0 (Cyberpunk Overhaul) — January 6, 2026
**Focus: Complete interface redesign and architecture simplification.**

- **UI Revolution:** Completely new web interface in "Cyberpunk/MedTech" style. Deep black background, neon indicators, Mold Risk Margin visualization.
- **Core Optimization:** Transition to simplified single-file architecture (Single-File) to reduce overhead. CPU frequency fixed at 80MHz to reduce thermal noise.
- **Robust Data Logging:** Introduced ring buffer history for detailed statistics storage.
- **Night Mode Logic:** Updated display logic (off 22:00-09:00), independent of data collection.
- **Chart.js Integration:** Dual graphs (Temperature + Humidity) with automatic update without page reload.

---

### v3.1 (Stable Core) — January 4, 2026
**Focus: Industrial stability, multitasking, memory protection.**

#### Critical Fixes:
- **Crash Fix:** Completely rewrote `/api/history` endpoint. Implemented chunked JSON streaming. This eliminated "Interrupt Watchdog Timeout" crashes and stack overflow when requesting history. Every 32 records, `vTaskDelay(1)` is executed to prevent network hangs.
- **Thread Safety:** Implemented `std::timed_mutex` to protect all shared resources (history, current readings) between sensor task and web server.
- **Static Memory:** Complete rejection of `String` and dynamic allocation in "hot" paths. History buffer is now static (`std::array`), eliminating heap fragmentation.

#### Architecture Improvements:
- **Dedicated Task:** DHT22 reading moved to separate FreeRTOS task (`sensorTask`), pinned to Core 1. This guarantees exact reading intervals (6.0 sec) regardless of web server load.
- **Stack Tuning:** Increased stack size for `loopTask` (to 16KB) and `sensorTask` (to 10KB) for safe JSON handling.

---

### v3.0 (Base Refactor) — January 2, 2026
**Focus: Modularity and base functionality.**

- **Refactoring:** Monolithic `main.cpp` split into 7 independent modules (`SensorManager`, `WebManager`, `DisplayManager`, etc.).
- **Smart Window Logic:** State machine (STABLE → VENTILATING → PLATEAU) for accurate ventilation detection.
- **Plateau Detection:** Algorithm to detect when ventilation stopped being effective ("PLATEAU! Close and Heat").
- **Dark UI:** Modern web interface in dark tones with Chart.js graphs.

---

### v2.x (Legacy) — December 2025
**Focus: Initial development.**

- **v2.5 (December 27):** Integration of open-meteo.com weather API, seasonal advice logic.
- **v2.3 (December 24):** Telegram bot with status commands and notifications.
- **v2.0 (December 20):** Simple synchronous web server with basic dashboard.
- **v1.0 (December 15):** Basic DHT22 and OLED display operation.
