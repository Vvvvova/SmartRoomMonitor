# Walkthrough: Architectural Evolution (v4.1)

Successfully completed the "Vibecoding" refactor. The project is now safer, more modular, and easier to extend.

## Key Changes

### 1. Safe Data Access (Snapshot Pattern)
We removed the risk of deadlocks from `DisplayManager`, `WebManager`, and `TelegramManager`.
Instead of manually locking the whole state, they now use `g_state.getSnapshot()`.

```diff
- g_state.lock(); 
- float t = g_state.temp;
- g_state.unlock(); 
+ CoreSnapshot sn = g_state.getSnapshot();
+ float t = sn.temp;
```
*Benefit: It's now impossible to forget an `unlock()` call and freeze the device.*

### 2. Logic Isolation (`ClimateEngine`)
The "Brain" has been moved out of `SensorManager` into its own class: [ClimateEngine.h](file:///c:/Users/vvvvo/Documents/SmartRoomMonitor/include/vibe/ClimateEngine.h).
- **`SensorManager`**: Now only polls the DHT22 and filters noise.
- **`ClimateEngine`**: Only handles the state machine (Ventilation, Plateau, Success).

### 3. Mutex Stability
As requested, we kept the original **FreeRTOS Mutexes**. We just hidden the complexity inside `CoreState` and `ClimateEngine`.

## Module Status
| Module | Responsibility | Access Pattern |
| :--- | :--- | :--- |
| `CoreState` | Data Hub | Snapshot / Mutex |
| `SensorManager` | Hardware Polling | Pushes to Hub |
| `ClimateEngine` | Physics & State | Logic processing |
| `DisplayManager` | UI Rendering | Snapshot (Safe) |
| `TelegramBot` | Notifications | Snapshot (Safe) |

---
**Verification**:
- [x] Compilation successful for ESP32.
- [x] Snapshot pattern validated across 3 modules.
- [x] State machine logic preserved and isolated.
