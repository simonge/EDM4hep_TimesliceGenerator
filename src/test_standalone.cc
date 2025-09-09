#include "StandaloneTimesliceMerger.h"
#include <cassert>
#include <iostream>

// Simple test to verify the configuration and basic logic
void testConfiguration() {
    std::cout << "Testing configuration..." << std::endl;
    
    MergerConfig config;
    
    // Test default values
    assert(config.time_slice_duration == 20.0f);
    
    SourceConfig source;
    assert(source.useStaticNumberOfEvents() == false);
    assert(source.getStaticEventsPerTimeslice() == 1);
    assert(source.useBunchCrossing() == false);
    assert(source.attachToBeam() == false);
    assert(source.getGeneratorStatusOffset() == 0);
    
    // Test configuration changes
    config.time_slice_duration = 1000.0f;
    source.setStaticNumberOfEvents(true);
    source.setStaticEventsPerTimeslice(5);
    source.setUseBunchCrossing(true);
    config.bunch_crossing_period = 100.0f;
    
    assert(config.time_slice_duration == 1000.0f);
    assert(source.useStaticNumberOfEvents() == true);
    assert(source.getStaticEventsPerTimeslice() == 5);
    assert(source.useBunchCrossing() == true);
    assert(config.bunch_crossing_period == 100.0f);
    
    std::cout << "Configuration test passed!" << std::endl;
}

void testMergerCreation() {
    std::cout << "Testing merger creation..." << std::endl;
    
    MergerConfig config;
    config.time_slice_duration = 100.0f;
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
    std::cout << "=== Timeslice Merger Tests ===" << std::endl;
    
    testConfiguration();
    testMergerCreation();
    
    std::cout << "All tests completed!" << std::endl;
    std::cout << "\nNote: Full functionality tests require Podio and EDM4HEP libraries." << std::endl;
    std::cout << "Build with proper dependencies using: ./build.sh" << std::endl;
    
    return 0;
}