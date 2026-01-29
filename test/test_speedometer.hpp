#include <unity.h>
#include "SensorManager.h"

// Needs force_ventilating_state from test_timer.cpp or duplicated
// For simplicity, defining a local helper or assuming link order.
// Let's duplicate strictly to avoid link errors in single-file compilation if that happens.
static void enter_vent_state(SensorManager &sm) {
    delay(3600000);
    sm.processReading(22.0, 50.0);
    sm.update();
    delay(6000);
    for(int i=0; i<15; i++) {
        sm.processReading(18.0, 50.0);
        sm.update();
        delay(6000);
    }
}

void test_speedometer_no_drying() {
    SensorManager sm;
    sm.begin();
    enter_vent_state(sm);
    
    // Initial State:
    delay(60 * 1000);
    sm.processReading(18.0, 50.0);
    sm.update(); 
    
    float rate = sm.getDryingRate();
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, rate);
    
    String ind = sm.getDryingIndicator();
    TEST_ASSERT_EQUAL_STRING("-", ind.c_str());
}

void test_speedometer_fast_drying() {
    SensorManager sm;
    sm.begin();
    enter_vent_state(sm);
    
    // Start: 18C, 50%
    float startAbs = ClimateMath::calculateAbsHumidity(18, 50);
    
    // 2 minutes later
    delay(120 * 1000);
    
    // Drying happened! Drop to 30% humidity
    sm.processReading(18.0, 30.0);
    sm.update();
    
    float currentAbs = ClimateMath::calculateAbsHumidity(18, 30);
    float diff = startAbs - currentAbs; 
    
    float rate = sm.getDryingRate();
    // Rate = (Start - Current) / Mins
    // ~ (Start - Current) / 2.0
    TEST_ASSERT_FLOAT_WITHIN(0.01, diff / 2.0, rate);
    
    String ind = sm.getDryingIndicator();
    TEST_ASSERT_NOT_EQUAL("-", ind.c_str());
    TEST_ASSERT_NOT_EQUAL("▼", ind.c_str());
}

void test_speedometer_slow_drying() {
    SensorManager sm;
    sm.begin();
    enter_vent_state(sm);
    
    // 5 minutes later
    delay(300 * 1000);
    
    // Very tiny drop
    sm.processReading(18.0, 49.0);
    sm.update();
    
    float rate = sm.getDryingRate();
    // Rate is positive but small
    TEST_ASSERT_TRUE(rate > 0.0);
    
    String ind = sm.getDryingIndicator();
    // Should be slow
    TEST_ASSERT_EQUAL_STRING("▼", ind.c_str()); 
}

void test_speedometer_negative_drying() {
    SensorManager sm;
    sm.begin();
    enter_vent_state(sm);
    
    delay(60 * 1000);
    
    // Humidity ROSE 
    sm.processReading(18.0, 60.0);
    sm.update();
    
    float rate = sm.getDryingRate();
    TEST_ASSERT_TRUE(rate < 0.0);
    
    String ind = sm.getDryingIndicator();
    TEST_ASSERT_EQUAL_STRING("-", ind.c_str());
}
