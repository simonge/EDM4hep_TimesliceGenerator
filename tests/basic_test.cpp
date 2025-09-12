#include <iostream>
#include <cassert>

// Simple test to verify the build environment works
int main() {
    std::cout << "Running basic build environment test..." << std::endl;
    
    // Test basic C++ functionality
    int x = 42;
    assert(x == 42);
    
    std::cout << "Basic test passed successfully!" << std::endl;
    return 0;
}