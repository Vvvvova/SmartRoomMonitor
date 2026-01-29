# Architecture Refactor: Reactive Hub & Clean Logic

This plan implements the "Vibecoding" architecture by refactoring `CoreState` into a reactive data hub and slimming down the `SensorManager` monolith.

## Proposed Changes

### 🔧 [Component] CoreState (Data Hub)
- **[MODIFY] [CoreState.h](file:///c:/Users/vvvvo/Documents/SmartRoomMonitor/include/CoreState.h)**
    - **Keep existing Mutex-based logic** (it works and it's stable!).
    - Implement `getSnapshot()`: A small helper that does the `lock() -> copy -> unlock()` sequence for you. This prevents bugs where someone forgets to `unlock()` and freezes the whole ESP32.
    - Implement `updateData(...)`: A setter that handles the lock automatically.
    - **Why?** This doesn't change *how* it works, it just makes it impossible to "miss" an unlock command.

---

### 🧠 [Component] Logic Layer (Climate Engine)
- **[NEW] [ClimateEngine.h](file:///c:/Users/vvvvo/Documents/SmartRoomMonitor/include/vibe/ClimateEngine.h)**
- **[NEW] [ClimateEngine.cpp](file:///c:/Users/vvvvo/Documents/SmartRoomMonitor/src/vibe/ClimateEngine.cpp)**
    - Move all "State Machine" variables (slope window, rebound tracking, trigger counters) here.
    - Use the pure `computeTransition` function to drive internal state updates.
    - **Note**: This class will be stateless in terms of hardware, only caring about the *physics* and *rules*.

---

### 🔌 [Component] Hardware Layer (Sensor Service)
- **[MODIFY] [SensorManager.h](file:///c:/Users/vvvvo/Documents/SmartRoomMonitor/include/SensorManager.h)**
- **[MODIFY] [SensorManager.cpp](file:///c:/Users/vvvvo/Documents/SmartRoomMonitor/src/SensorManager.cpp)**
    - Rename/Refactor to focus purely on hardware polling (the FreeRTOS task).
    - It will read the DHT22 and push raw data to `CoreState`.
    - It will no longer "decide" if an alert is needed or what the climate state is.

---

### 🎨 [Component] Infrastructure (Managers)
- **[MODIFY] [DisplayManager.cpp](file:///c:/Users/vvvvo/Documents/SmartRoomMonitor/src/DisplayManager.cpp)**
- **[MODIFY] [TelegramManager.cpp](file:///c:/Users/vvvvo/Documents/SmartRoomMonitor/src/TelegramManager.cpp)**
- **[MODIFY] [WebManager.cpp](file:///c:/Users/vvvvo/Documents/SmartRoomMonitor/src/WebManager.cpp)**
    - Remove all `g_state.lock()` / `unlock()` calls.
    - Switch to `g_state.getSnapshot()`.
    - (Optional) Use reactive callbacks to avoid polling in `loop()`.

---

### 🚀 [Component] System (Main)
- **[MODIFY] [main.cpp](file:///c:/Users/vvvvo/Documents/SmartRoomMonitor/src/main.cpp)**
    - Simplify the `loop()` by removing constant `update()` calls for components that can be reactive.

## Verification Plan

### Automated Tests
- Run existing state machine tests against the new `ClimateEngine`.
- Verify compilation for `esp32doit-devkit-v1`.

### Manual Verification
- Check Serial output for state transitions.
- Verify OLED display updates correctly when breathing on the sensor.
- Ensure Telegram bot still responds to `/status`.
