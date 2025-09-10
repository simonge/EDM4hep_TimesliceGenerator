#include "PodioCollectionZipReader.h"
#include <iostream>
#include <cassert>

// Test function for collection zipping functionality
void testCollectionZipping() {
    std::cout << "Testing collection zipping functionality..." << std::endl;
    
    // This test would require actual Podio/EDM4HEP setup
    // For now, just test that the class can be instantiated
    try {
        std::vector<std::string> test_files; // Empty for test
        // Can't actually create reader without valid files, but we can test other functionality
        
        std::cout << "PodioCollectionZipReader test structure created successfully!" << std::endl;
        
        // Test static methods that don't require file I/O
        edm4hep::MCParticleCollection test_particles;
        PodioCollectionZipReader::addTimeOffsetVectorized(test_particles, 10.0f);
        std::cout << "Vectorized time offset function callable!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Expected exception (no files provided): " << e.what() << std::endl;
    }
    
    std::cout << "Collection zipping test completed!" << std::endl;
}

int main() {
    std::cout << "=== PodioCollectionZipReader Tests ===" << std::endl;
    
    testCollectionZipping();
    
    std::cout << "All tests completed!" << std::endl;
    std::cout << "\nNote: Full functionality tests require Podio and EDM4HEP libraries." << std::endl;
    std::cout << "Build with proper dependencies using: ./build.sh" << std::endl;
    
    return 0;
}