#include <unity.h>
#include "test_advice.hpp"
#include "test_timer.hpp"
#include "test_speedometer.hpp"
#include "test_state_machine.hpp"

void setUp(void) {
    // Reset mocks before each test
    DHT::_temp = NAN;
    DHT::_hum = NAN;
    _mock_millis = 0;
}

void tearDown(void) {
    // Clean up
}

void run_tests() {
    UNITY_BEGIN();
    
    // New Pure Function Tests (Reliable)
    RUN_TEST(test_transition_stable_to_ventilating_trigger);
    RUN_TEST(test_transition_stable_lockout);
    RUN_TEST(test_transition_ventilating_to_target_met);
    RUN_TEST(test_transition_ventilating_to_inefficient_plateau);
    RUN_TEST(test_transition_ventilating_to_stable_rebound);
    RUN_TEST(test_transition_timeout);

    // Legacy Integration Tests
    RUN_TEST(test_advice_stable_winter);
    RUN_TEST(test_advice_stable_summer);
    RUN_TEST(test_advice_ventilating);
    
    // These behave as "Mock Integration Tests" now
    RUN_TEST(test_timer_start);
    RUN_TEST(test_timer_progress);
    RUN_TEST(test_timer_long_duration);
    
    RUN_TEST(test_speedometer_no_drying);
    RUN_TEST(test_speedometer_fast_drying);
    RUN_TEST(test_speedometer_slow_drying);
    RUN_TEST(test_speedometer_negative_drying);
    
    UNITY_END();
}

int main(int argc, char **argv) {
    run_tests();
    return 0;
}
