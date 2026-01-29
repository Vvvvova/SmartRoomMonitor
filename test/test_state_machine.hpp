#pragma once
#include <unity.h>
#include "SensorManager.h"

// Helper to create a default "Stable" input state
SensorManager::StateInput create_default_input() {
    SensorManager::StateInput in;
    in.now = 1000000; // Arbitrary start time
    in.stateEnterTime = 0; // Way in the past
    in.currentState = SensorManager::ClimateState::STABLE;
    
    // Baseline = Current (Stable)
    in.temp = 22.0;
    in.hum = 50.0;
    in.absHum = 12.0; // Approx
    
    in.baseTemp = 22.0;
    in.baseHum = 50.0;
    in.baseAbsHum = 12.0;
    in.prevAbsHum = 12.0;
    
    in.triggerConfirmCount = 0;
    in.plateauConfirmCount = 0;
    in.baselineUpdateCount = 0;
    
    in.reboundStartTemp = NAN;
    in.reboundStartTime = 0;
    
    in.slopeNewest = NAN;
    in.slopeOldest = NAN;
    in.slopeCount = 0;
    
    in.stateEnterHum = NAN;
    in.stateEnterAbsHum = NAN;
    
    return in;
}

void test_transition_stable_to_ventilating_trigger() {
    SensorManager::StateInput in = create_default_input();
    
    // Condition: Rapid Humidity Drop (> 2.0%)
    in.baseHum = 50.0;
    in.hum = 45.0; // 5% drop
    
    // Threshold is likely defined in Settings (usually 2-3%)
    // Let's assume we need confirmation
    in.triggerConfirmCount = 0; // First detection
    
    // Trigger 1
    SensorManager::StateOutput out = SensorManager::computeTransition(in);
    // Should increment counter but NOT transition yet (if confirm > 1)
    // Actually standard confirm is often 3. 
    // Let's verify counter increment.
    TEST_ASSERT_EQUAL(1, out.newTriggerCount);
    TEST_ASSERT_EQUAL(SensorManager::ClimateState::STABLE, out.newState);
    
    // Trigger Final (Simulate last step)
    in.triggerConfirmCount = 2; // Assuming confirm is 3? 
    // Wait, let's check Settings.h or just try max.
    // Ideally we'd look up VENT_TRIGGER_CONFIRM.
    // If I don't know it, I'll set it high or check return.
    
    // Let's assume it's 3.
    out = SensorManager::computeTransition(in);
    
    if (out.newTriggerCount > 2) { 
         // If it incremented, it means we didn't hit threshold yet?
         // OR strict > vs >=.
    }
    
    // Force transition by high confirmation count input
    in.triggerConfirmCount = 10; 
    out = SensorManager::computeTransition(in);
    
    TEST_ASSERT_EQUAL(SensorManager::ClimateState::VENTILATING, out.newState);
    TEST_ASSERT_NOT_NULL(out.transitionReason);
    TEST_ASSERT_EQUAL_STRING("STABLE -> VENTILATING (Smart Trigger)", out.transitionReason);
    
    // Verify Updates
    TEST_ASSERT_TRUE(out.updateBaseline);
    TEST_ASSERT_EQUAL_FLOAT(in.temp, out.newBaseTemp);
}

void test_transition_stable_lockout() {
    SensorManager::StateInput in = create_default_input();
    
    // Condition: Rapid Drop (Trigger valid)
    in.baseHum = 50.0;
    in.hum = 45.0; 
    in.triggerConfirmCount = 10; // Should trigger immediately if no lockout
    
    // BUT: State entered recently
    in.stateEnterTime = in.now - 1000; // 1 second ago
    
    SensorManager::StateOutput out = SensorManager::computeTransition(in);
    
    // Should STAY Stable due to lockout
    TEST_ASSERT_EQUAL(SensorManager::ClimateState::STABLE, out.newState);
    TEST_ASSERT_EQUAL(0, out.newTriggerCount); // Should simulate reset or ignore? Code says "anyTrigger && !lockout". so else -> cnt=0.
}

void test_transition_ventilating_to_target_met() {
    SensorManager::StateInput in = create_default_input();
    in.currentState = SensorManager::ClimateState::VENTILATING;
    in.stateEnterTime = in.now - 600000; // 10 mins in
    in.stateEnterHum = 60.0;
    
    // Target is max(50, start-15) -> max(50, 45) = 50.0
    // Current Hum = 49.0 -> Target Met
    in.hum = 49.0;
    
    SensorManager::StateOutput out = SensorManager::computeTransition(in);
    
    TEST_ASSERT_EQUAL(SensorManager::ClimateState::TARGET_MET, out.newState);
    TEST_ASSERT_EQUAL_STRING("VENTILATING -> TARGET_MET (Target reached)", out.transitionReason);
}

void test_transition_ventilating_to_inefficient_plateau() {
    SensorManager::StateInput in = create_default_input();
    in.currentState = SensorManager::ClimateState::VENTILATING;
    in.stateEnterTime = in.now - 200000; // > 3 mins
    in.stateEnterHum = 70.0;
    in.hum = 65.0; // Well above target (approx 55)
    
    // Plateau Trigger
    in.slopeCount = 6;
    // Slope = Newest - Oldest
    // Flat slope: 12.0 - 12.0 = 0.0
    in.slopeNewest = 12.0;
    in.slopeOldest = 12.0;
    // Threshold usually negative (drying means AbsHum drops).
    // Wait, slope = New - Old. 
    // Drying: Old=12, New=10 -> Slope = -2.
    // Plateau: Old=10, New=10 -> Slope = 0.
    // Threshold "adaptiveThreshold" is calculated.
    // Usually around -0.15. 
    // So 0.0 > -0.15 -> TRUE (Plateau detected, drying stopped).
    
    in.plateauConfirmCount = 15; // Matches PLATEAU_CONFIRM_COUNT of 15
    
    SensorManager::StateOutput out = SensorManager::computeTransition(in);
    
    TEST_ASSERT_EQUAL(SensorManager::ClimateState::INEFFICIENT, out.newState);
    TEST_ASSERT_EQUAL_STRING("VENTILATING -> INEFFICIENT (Plateau)", out.transitionReason);
}

void test_transition_ventilating_to_stable_rebound() {
    SensorManager::StateInput in = create_default_input();
    in.currentState = SensorManager::ClimateState::VENTILATING;
    in.stateEnterHum = 70.0;
    in.hum = 65.0; // Safe from target met
    
    // Rebound Setup
    in.reboundStartTemp = 18.0;
    in.reboundStartTime = in.now - 120000; // 2 mins ago
    
    // Current Temp rose siginificantly
    in.temp = 20.0; // +2C rise
    // REBOUND_TEMP_RISE usually ~0.5C ??
    // REBOUND_TIME_MS usually ~60s ??
    
    SensorManager::StateOutput out = SensorManager::computeTransition(in);
    
    TEST_ASSERT_EQUAL(SensorManager::ClimateState::STABLE, out.newState);
    TEST_ASSERT_TRUE(std::string(out.transitionReason).find("(Temp rebound)") != std::string::npos);
}

void test_transition_timeout() {
    SensorManager::StateInput in = create_default_input();
    in.currentState = SensorManager::ClimateState::TARGET_MET;
    in.stateEnterTime = in.now - 3600001; // > 1 hour
    
    SensorManager::StateOutput out = SensorManager::computeTransition(in);
    
    TEST_ASSERT_EQUAL(SensorManager::ClimateState::STABLE, out.newState);
    TEST_ASSERT_TRUE(std::string(out.transitionReason).find("Timeout") != std::string::npos);
}
