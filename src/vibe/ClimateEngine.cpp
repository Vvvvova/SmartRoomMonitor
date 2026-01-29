#include "vibe/ClimateEngine.h"
#include "ClimateMath.h"

ClimateEngine::ClimateEngine()
    : lastTempForWindowCheck(NAN), lastHumForWindowCheck(NAN),
      lastAbsHumForWindowCheck(NAN), stateEnterAbsHum(NAN), stateEnterHum(NAN),
      slopeWindowHead(0), slopeWindowCount(0), plateauConfirmCounter(0),
      baselineUpdateCounter(0), reboundStartTime(0), reboundStartTemp(NAN),
      reboundMinAbsHum(NAN),
      triggerConfirmCount(0), prevAbsHumForTrigger(NAN),
      state(ClimateState::STABLE), stateEnterTime(0),
      lastHistoryLogTime(0), lastSlopeUpdateTime(0),
      lastBaselineUpdateTime(0) {
    for (size_t i = 0; i < SLOPE_WINDOW_SIZE; i++) {
        slopeWindow[i] = NAN;
    }
}

unsigned long ClimateEngine::getSuggestedTickInterval() const {
    // Balanced 10s polling: fast enough to catch windows, 
    // slow enough to prevent DHT22 self-heating.
    return 10000; 
}

ClimateEngine::EngineResult ClimateEngine::process(const CoreSnapshot& sn, unsigned long now) {
    EngineResult result;
    
    // Extract for readability
    float t = sn.temp;
    float h = sn.hum;
    float absHum = sn.absHum;

    // 1. Initialize baseline on first valid reading
    if (isnan(lastHumForWindowCheck)) {
        lastHumForWindowCheck = h;
        lastTempForWindowCheck = t;
        lastAbsHumForWindowCheck = absHum;
        prevAbsHumForTrigger = absHum;
    }

    // 2. Slope updates are now handled in the History Logging section below
    // to maintain a consistent 3-minute analysis rhythm.

    // 3. Prepare State Machine Inputs
    float slopeNewest = NAN, slopeOldest = NAN;
    if (slopeWindowCount >= 3) {
        size_t oldestIdx = (slopeWindowHead + SLOPE_WINDOW_SIZE - slopeWindowCount) % SLOPE_WINDOW_SIZE;
        slopeOldest = slopeWindow[oldestIdx];
        slopeNewest = slopeWindow[(slopeWindowHead + SLOPE_WINDOW_SIZE - 1) % SLOPE_WINDOW_SIZE];
    }

    StateInput in;
    in.temp = t;
    in.hum = h;
    in.absHum = absHum;
    in.baseTemp = lastTempForWindowCheck;
    in.baseHum = lastHumForWindowCheck;
    in.baseAbsHum = lastAbsHumForWindowCheck;
    in.prevAbsHum = prevAbsHumForTrigger;
    in.currentState = state;
    in.stateEnterTime = stateEnterTime;
    in.now = now;
    in.triggerConfirmCount = triggerConfirmCount;
    in.plateauConfirmCount = plateauConfirmCounter;
    in.baselineUpdateCount = baselineUpdateCounter;
    in.reboundStartTemp = reboundStartTemp;
    in.reboundMinAbsHum = reboundMinAbsHum;
    in.reboundStartTime = reboundStartTime;
    in.slopeNewest = slopeNewest;
    in.slopeOldest = slopeOldest;
    in.slopeCount = slopeWindowCount;
    in.stateEnterHum = stateEnterHum;
    in.stateEnterAbsHum = stateEnterAbsHum;
    in.lastBaselineUpdateTime = lastBaselineUpdateTime;

    // 4. Run Pure Decision Logic
    StateOutput out = computeTransition(in);

    // 5. Apply Updates
    bool changed = (state != out.newState);
    state = out.newState;
    stateEnterTime = out.newStateEnterTime;
    triggerConfirmCount = out.newTriggerCount;
    plateauConfirmCounter = out.newPlateauCount;
    baselineUpdateCounter = out.newBaselineCounter;
    reboundStartTemp = out.newReboundStartTemp;
    reboundMinAbsHum = out.newReboundMinAbsHum;
    reboundStartTime = out.newReboundStartTime;
    stateEnterHum = out.newStateEnterHum;
    stateEnterAbsHum = out.newStateEnterAbsHum;

    if (out.updateBaseline) {
        lastTempForWindowCheck = out.newBaseTemp;
        lastHumForWindowCheck = out.newBaseHum;
        lastAbsHumForWindowCheck = out.newBaseAbsHum;
        lastBaselineUpdateTime = now; // Sync time-based baseline
    }

    if (out.transitionReason != nullptr && changed) {
        Serial.printf("[ENGINE] %s\n", out.transitionReason);
    }

    // Reset tracking if we returned to stable
    if (state == ClimateState::STABLE) {
        slopeWindowCount = 0;
        slopeWindowHead = 0;
    }

    prevAbsHumForTrigger = absHum;
    
    // =========================================================================
    // 6. POPULATE RESULT
    // =========================================================================
    result.stateChanged = changed;
    result.state = state;
    result.stateEnterTime = stateEnterTime;
    
    // A. Advice Logic
    AdviceInput advIn;
    advIn.temp = t;
    advIn.hum = h;
    advIn.absHum = absHum;
    advIn.dewPoint = sn.dewPoint;
    advIn.state = state;
    advIn.stateEnterTime = stateEnterTime;
    advIn.now = now;
    advIn.dryingInd = getDryingIndicator(now);
    
    if (sn.weatherValid) {
        advIn.weatherValid = true;
        advIn.outTemp = sn.outTemp;
        advIn.outAbsHum = sn.outAbsHum;
    }
    
    AdviceOutput advOut = AdviceEngine::compute(advIn);
    result.adviceText = advOut.text;
    result.adviceCode = advOut.code;
    
    result.dryingRate = getDryingRate(now);
    result.dryingInd = advIn.dryingInd;
    
    // B. Poll Interval Logic
    result.suggestPollInterval = getSuggestedTickInterval();
    
    // C. History Logging Logic (Always 3 mins for consistent graph length)
    unsigned long logInterval = 180000;
    
    bool timeValid = (time(NULL) > 1000000);

    if (timeValid && (lastHistoryLogTime == 0 || now - lastHistoryLogTime >= logInterval)) {
        result.shouldLogHistory = true;
        lastHistoryLogTime = now;
        
        // Update slope window ONLY during logging to maintain 3-min intervals (Analysis window = 18 mins)
        if (state != ClimateState::STABLE) {
            updateSlope(absHum);
        }
    } else {
        result.shouldLogHistory = false;
    }
    
    return result;
}

void ClimateEngine::updateSlope(float absHum) {
    // Only update slope window periodically (e.g. every 30s) or let caller handle it.
    // For now, mirroring SensorManager behavior but optimized.
    // In original code, it updated every log cycle (30s during vent).
    // Here we just add it to ring buffer.
    slopeWindow[slopeWindowHead] = absHum;
    slopeWindowHead = (slopeWindowHead + 1) % SLOPE_WINDOW_SIZE;
    if (slopeWindowCount < SLOPE_WINDOW_SIZE) slopeWindowCount++;
}

float ClimateEngine::getDryingRate(unsigned long now) const {
    if (state != ClimateState::VENTILATING && state != ClimateState::TARGET_MET) return 0.0f;
    unsigned long dur = now - stateEnterTime;
    if (dur < 60000 || isnan(stateEnterAbsHum)) return 0.0f;
    
    float absHumDrop = stateEnterAbsHum - slopeWindow[(slopeWindowHead + SLOPE_WINDOW_SIZE - 1) % SLOPE_WINDOW_SIZE]; 
    // Fallback to current if slope window is empty? Actually, process() takes current absHum.
    // Let's use the current state enter vs current logic.
    return absHumDrop / (dur / 60000.0f);
}

String ClimateEngine::getDryingIndicator(unsigned long now) const {
    float rate = getDryingRate(now);
    if (rate <= 0.0f) return "-";
    if (rate >= RATE_EXCELLENT) return "▲▲";
    if (rate >= RATE_GOOD) return "▲";
    return "▼";
}

// =============================================================================
// TRANSITION LOGIC (Pasted from SensorManager)
// =============================================================================
ClimateEngine::StateOutput
ClimateEngine::computeTransition(const StateInput& in) {
  StateOutput out;
  
  // Initialize output with current state (no change by default)
  out.newState = in.currentState;
  out.newStateEnterTime = in.stateEnterTime;
  out.updateBaseline = false;
  out.newBaseTemp = in.baseTemp;
  out.newBaseHum = in.baseHum;
  out.newBaseAbsHum = in.baseAbsHum;
  out.newTriggerCount = in.triggerConfirmCount;
  out.newPlateauCount = in.plateauConfirmCount;
  out.newBaselineCounter = in.baselineUpdateCount;
  out.newReboundStartTemp = in.reboundStartTemp;
  out.newReboundStartTime = in.reboundStartTime;
  out.newStateEnterHum = in.stateEnterHum;
  out.newStateEnterAbsHum = in.stateEnterAbsHum;
  out.transitionReason = nullptr;

  unsigned long timeSinceStateEnter = in.now - in.stateEnterTime;

  // --- STABLE ---
  if (in.currentState == ClimateState::STABLE) {
    out.newPlateauCount = 0;
    out.newReboundStartTemp = NAN;

    bool lockoutActive = (timeSinceStateEnter < STATE_LOCKOUT_MS);

    bool rapidHumDrop = (!isnan(in.baseHum) && (in.baseHum - in.hum) > VENT_TRIGGER_HUM_DROP);
    bool rapidTempDrop = (!isnan(in.baseTemp) && (in.baseTemp - in.temp) > VENT_TRIGGER_TEMP_DROP);
    bool rapidAbsHumDrop = (!isnan(in.prevAbsHum) && (in.prevAbsHum - in.absHum) > VENT_TRIGGER_ABSHUM_DROP);

    // If polling is slow (30s), even 1 confirm is enough, but for 6s we keep 2-3
    unsigned int confTarget = (in.now - in.stateEnterTime < 6500) ? VENT_TRIGGER_CONFIRM : 1;

    if ((rapidHumDrop || rapidTempDrop || rapidAbsHumDrop) && !lockoutActive) {
      out.newTriggerCount = in.triggerConfirmCount + 1;
      if (out.newTriggerCount >= confTarget) {
        out.newState = ClimateState::VENTILATING;
        out.newStateEnterTime = in.now;
        out.newStateEnterHum = in.hum;
        out.newStateEnterAbsHum = in.absHum;
        out.updateBaseline = true;
        out.newBaseTemp = in.temp;
        out.newBaseHum = in.hum;
        out.newBaseAbsHum = in.absHum;
        out.newTriggerCount = 0;
        out.transitionReason = "STABLE -> VENTILATING (Trigger)";
      }
    } else {
      out.newTriggerCount = 0;
    }

    // Every 90 minutes update long-term baseline
    if (in.lastBaselineUpdateTime == 0 || in.now - in.lastBaselineUpdateTime >= 5400000) {
      out.updateBaseline = true;
      out.newBaseTemp = in.temp;
      out.newBaseHum = in.hum;
      out.newBaseAbsHum = in.absHum;
      // We'll update lastBaselineUpdateTime in process()
    }
  }

  // --- VENTILATING ---
  // --- NON-STABLE STATES (VENTILATING, TARGET_MET, INEFFICIENT) ---
  else {
    // 1. VENTILATING Specific: Check for success or plateau
    if (in.currentState == ClimateState::VENTILATING) {
      float targetHum = fmax(50.0f, in.stateEnterHum - 15.0f);
      if (in.hum <= targetHum) {
        out.newState = ClimateState::TARGET_MET;
        out.newStateEnterTime = in.now;
        out.updateBaseline = true;
        out.newBaseTemp = in.temp;
        out.newBaseHum = in.hum;
        out.newBaseAbsHum = in.absHum;
        out.transitionReason = "Target humidity reached";
      }
      else if (timeSinceStateEnter > 180000 && in.slopeCount >= 3) {
        float slope = in.slopeNewest - in.slopeOldest;
        float adaptiveThreshold = PLATEAU_SLOPE_THRESHOLD;
        if (!isnan(in.stateEnterHum)) {
          float humFactor = fmin(fmax(in.stateEnterHum, 50.0f), 70.0f) - 50.0f;
          adaptiveThreshold = PLATEAU_SLOPE_THRESHOLD - (humFactor * 0.003f);
        }

        if (!isnan(slope) && slope > 0.05f) {
          out.newState = ClimateState::STABLE;
          out.newStateEnterTime = in.now;
          out.updateBaseline = true;
          out.newBaseTemp = in.temp;
          out.newBaseHum = in.hum;
          out.newBaseAbsHum = in.absHum;
          out.transitionReason = "Rebound: Humidity rising (Window closed?)";
        } else if (!isnan(slope) && slope > adaptiveThreshold) {
          out.newPlateauCount = in.plateauConfirmCount + 1;
          if (out.newPlateauCount >= PLATEAU_CONFIRM_COUNT) {
            out.newState = ClimateState::INEFFICIENT;
            out.newStateEnterTime = in.now;
            out.updateBaseline = true;
            out.newBaseTemp = in.temp;
            out.newBaseHum = in.hum;
            out.newBaseAbsHum = in.absHum;
            out.transitionReason = "Drying reached plateau";
          }
        } else {
          out.newPlateauCount = 0;
        }
      }
    }

    // 2. SHARED REBOUND: Check for window close in any active mode
    if (out.newState != ClimateState::STABLE) {
      // Temperature Trough Tracking
      if (isnan(in.reboundStartTemp) || in.temp < in.reboundStartTemp) {
          out.newReboundStartTemp = in.temp;
          out.newReboundStartTime = in.now;
      } 
      
      // AbsHum Trough (The "Smoking Gun" of closed window)
      if (isnan(in.reboundMinAbsHum) || in.absHum < in.reboundMinAbsHum) {
          out.newReboundMinAbsHum = in.absHum;
      } else {
          out.newReboundMinAbsHum = in.reboundMinAbsHum;
      }

      // --- EXIT CONDITIONS ---
      
      // A. Temperature turnaround (Sustained rise)
      if (!isnan(out.newReboundStartTemp) && (in.temp - out.newReboundStartTemp) >= REBOUND_TEMP_RISE) {
          if (in.now - out.newReboundStartTime >= REBOUND_TIME_MS) {
              out.newState = ClimateState::STABLE;
              out.newStateEnterTime = in.now;
              out.updateBaseline = true;
              out.transitionReason = "Rebound: Sustained Temp rise";
          }
      }

      // B. Humidity turnaround (Steady climb from trough)
      if (out.newState != ClimateState::STABLE && !isnan(out.newReboundMinAbsHum)) {
          // If AbsHum rises by 0.15 from its lowest point AND current AbsHum is steady above it
          if (in.absHum > out.newReboundMinAbsHum + 0.15f) {
              out.newState = ClimateState::STABLE;
              out.newStateEnterTime = in.now;
              out.updateBaseline = true;
              out.transitionReason = "Rebound: AbsHum turnaround";
          }
      }

      // C. Immediate Fallback: Rapid rise between readings (10s)
      if (out.newState != ClimateState::STABLE && !isnan(in.prevAbsHum)) {
          if (in.absHum > in.prevAbsHum + 0.3f) { // Increased to 0.3 for noise protection
              out.newState = ClimateState::STABLE;
              out.newStateEnterTime = in.now;
              out.updateBaseline = true;
              out.transitionReason = "Rebound: Rapid AbsHum jump";
          }
      }

      // D. Pre-ventilation recovery
      if (out.newState != ClimateState::STABLE && (in.absHum - in.baseAbsHum) > REBOUND_ABSHUM_RISE) {
          out.newState = ClimateState::STABLE;
          out.newStateEnterTime = in.now;
          out.updateBaseline = true;
          out.transitionReason = "Rebound: AbsHum > Baseline";
      }

      // E. Safety Timeout (1h)
      if (out.newState != ClimateState::STABLE && timeSinceStateEnter > 3600000) {
          out.newState = ClimateState::STABLE;
          out.newStateEnterTime = in.now;
          out.updateBaseline = true;
          out.transitionReason = "Safety Timeout (1h)";
      }
    }
  }

  return out;
}

void ClimateEngine::updateCoreState(CoreState& state, const EngineResult& result) {
    state.lock();
    state.state = result.state;
    state.stateEnterTime = result.stateEnterTime;
    state.advice = result.adviceText;
    state.adviceCode = result.adviceCode;
    state.dryingRate = result.dryingRate;
    state.dryingInd = result.dryingInd;
    state.heapFree = ESP.getFreeHeap();
    state.heapMin = ESP.getMinFreeHeap();
    
    // Feedback Loop: Tell sensors how fast to poll
    state.recommendedPollInterval = result.suggestPollInterval;

    // History Logging
    if (result.shouldLogHistory) {
         state.addHistoryPoint(state.temp, state.hum);

         // Update 24h average ONLY when adding history points (matching master rhythm)
         if (!isnan(state.hum)) {
             if (isnan(state.avg24h)) state.avg24h = state.hum;
             else state.avg24h = (state.avg24h * (1.0f - ALPHA_24H)) + (state.hum * ALPHA_24H);
         }
    }
    state.unlock();
}
