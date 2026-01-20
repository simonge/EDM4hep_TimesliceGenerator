#include "TimesliceMerger.h"
#include "OutputHandler.h"
#include "CommandLineParser.h"
#include <iostream>
#include <exception>

int main(int argc, char* argv[]) {
    try {
        // Parse command-line arguments and YAML configuration
        MergerConfig config = CommandLineParser::parse(argc, argv);
        
        // Create the merger
        TimesliceMerger merger(config);
        
        // Create appropriate output handler based on output file extension
        auto output_handler = OutputHandler::create(config.output_file);
        merger.setOutputHandler(std::move(output_handler));
        
        // Run the merger
        merger.run();
        
        std::cout << "Successfully completed timeslice merging!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}