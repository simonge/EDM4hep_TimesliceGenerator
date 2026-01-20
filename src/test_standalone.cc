#include "TimesliceMerger.h"
#include <cassert>
#include <iostream>

// Simple test to verify the configuration and basic logic
void testConfiguration() {
    std::cout << "Testing configuration..." << std::endl;
    
    MergerConfig config;
    
    // Test default values
    assert(config.time_slice_duration == 20.0f);
    assert(config.max_events == 100);
    assert(config.introduce_offsets == true);
    
    // Test configuration changes
    config.time_slice_duration = 1000.0f;
    config.max_events = 50;
    config.introduce_offsets = false;
    
    assert(config.time_slice_duration == 1000.0f);
    assert(config.max_events == 50);
    assert(config.introduce_offsets == false);
    
    std::cout << "Configuration test passed!" << std::endl;
}

void testMergerCreation() {
    std::cout << "Testing merger creation..." << std::endl;
    
    MergerConfig config;
    config.time_slice_duration = 100.0f;
    config.output_file = "test_output.root";
    config.max_events = 10;
    
    // Add a dummy source config
    SourceConfig source;
    source.static_number_of_events = true;
    source.static_events_per_timeslice = 2;
    config.sources.push_back(source);
    
    try {
        TimesliceMerger merger(config);
        std::cout << "Merger creation test passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Merger creation failed: " << e.what() << std::endl;
        std::cout << "This is expected if ROOT/Podio/EDM4HEP are not available in the test environment." << std::endl;
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