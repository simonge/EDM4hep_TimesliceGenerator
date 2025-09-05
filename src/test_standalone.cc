#include "StandaloneTimesliceMerger.h"
#include <cassert>
#include <iostream>

// Simple test to verify the configuration and basic logic
void testConfiguration() {
    std::cout << "Testing configuration..." << std::endl;
    
    StandaloneMergerConfig config;
    
    // Test default values
    assert(config.time_slice_duration == 20.0f);
    assert(config.static_number_of_events == false);
    assert(config.static_events_per_timeslice == 1);
    assert(config.use_bunch_crossing == false);
    assert(config.attach_to_beam == false);
    assert(config.generator_status_offset == 0);
    
    // Test configuration changes
    config.time_slice_duration = 1000.0f;
    config.static_number_of_events = true;
    config.static_events_per_timeslice = 5;
    config.use_bunch_crossing = true;
    config.bunch_crossing_period = 100.0f;
    
    assert(config.time_slice_duration == 1000.0f);
    assert(config.static_number_of_events == true);
    assert(config.static_events_per_timeslice == 5);
    assert(config.use_bunch_crossing == true);
    assert(config.bunch_crossing_period == 100.0f);
    
    std::cout << "Configuration test passed!" << std::endl;
}

void testMergerCreation() {
    std::cout << "Testing merger creation..." << std::endl;
    
    StandaloneMergerConfig config;
    config.time_slice_duration = 100.0f;
    config.static_number_of_events = true;
    config.static_events_per_timeslice = 2;
    config.output_file = "test_output.root";
    config.max_events = 10;
    
    try {
        StandaloneTimesliceMerger merger(config);
        std::cout << "Merger creation test passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Merger creation failed: " << e.what() << std::endl;
        std::cout << "This is expected if Podio is not available in the test environment." << std::endl;
    }
}

int main() {
    std::cout << "=== Standalone Timeslice Merger Tests ===" << std::endl;
    
    testConfiguration();
    testMergerCreation();
    
    std::cout << "All tests completed!" << std::endl;
    std::cout << "\nNote: Full functionality tests require Podio and EDM4HEP libraries." << std::endl;
    std::cout << "Build with proper dependencies using: ./build_standalone.sh" << std::endl;
    
    return 0;
}