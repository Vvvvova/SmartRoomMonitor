#include <unity.h>
#include "SensorManager.h"

// Helper to force state (since verify works best if we are actually allowed to set state)
// Since we can't easily change private state, we'll brute force the trigger.
// Assuming VENT_TRIGGER_TEMP_DROP is reasonable (e.g. 1-2 degrees).

void force_ventilating_state(SensorManager &sm) {
    // 0. Bypass Lockout
    delay(3600000); 
    
    // 1. Stable Baseline
    sm.processReading(22.0, 50.0);
    sm.update();
    delay(6000); 
    
    // 2. Drop fast matches passing test
    for(int i=0; i<15; i++) { // Increased loop count
        sm.processReading(18.0, 50.0);
        sm.update();
        delay(6000);
    }
}

void test_timer_start() {
    SensorManager sm;
    sm.begin();
    
    force_ventilating_state(sm);
    
    TEST_ASSERT_EQUAL(SensorManager::ClimateState::VENTILATING, sm.getClimateState());
    
    // Immediately after trigger
    // Time passed since stateEnterTime ~0 (depending on loop)
    // Ideally 0 min.
    
    // Update advice
    delay(2100);
    sm.update();
    
    String advice = sm.getRecommendation();
    // Expected: "Сушка (0 мин) ..."
    TEST_ASSERT_TRUE(advice.s.find("(0 мин)") != std::string::npos);
}

void test_timer_progress() {
    SensorManager sm;
    sm.begin();
    force_ventilating_state(sm);
    
    // Fast forward 5 minutes
    // We need to keep updating so logic runs?
    // Actually updateAdvice() just looks at millis() - stateEnterTime.
    // So we can just jump millis.
    delay(5 * 60 * 1000);
    
    // Trigger advice update
    delay(2100); 
    sm.update();
    
    String advice = sm.getRecommendation();
    // Expected: "Сушка (5 мин) ..."
    TEST_ASSERT_TRUE(advice.s.find("(5 мин)") != std::string::npos);
}

void test_timer_long_duration() {
    SensorManager sm;
    sm.begin();
    force_ventilating_state(sm);
    
    // Fast forward 65 minutes (1h 5m)
    delay(65 * 60 * 1000);
    
    delay(2100);
    sm.update();
    
    String advice = sm.getRecommendation();
    // Should show 65 min.
    // "Сушка (65 мин) ..."
    TEST_ASSERT_TRUE(advice.s.find("(65 мин)") != std::string::npos);
}
