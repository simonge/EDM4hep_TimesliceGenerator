#include "TimesliceMerger.h"
#include "CommandLineParser.h"
#include <iostream>
#include <exception>

int main(int argc, char* argv[]) {
    try {
        // Parse command-line arguments and YAML configuration
        MergerConfig config = CommandLineParser::parse(argc, argv);
        
        // Run the merger
        TimesliceMerger merger(config);
        merger.run();
        
        std::cout << "Successfully completed timeslice merging!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}