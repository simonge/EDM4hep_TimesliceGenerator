#include "StandaloneTimesliceMerger.h"
#include <iostream>
#include <string>
#include <vector>
#include <getopt.h>

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] input_file1 [input_file2 ...]\n"
              << "Options:\n"
              << "  -o, --output FILE           Output file name (default: merged_timeslices.root)\n"
              << "  -n, --nevents N             Maximum number of timeslices to generate (default: 100)\n"
              << "  -d, --duration TIME         Timeslice duration in ns (default: 20.0)\n"
              << "  -f, --frequency FREQ        Mean event frequency (events/ns) (default: 1.0)\n"
              << "  -p, --bunch-period PERIOD   Bunch crossing period in ns (default: 10.0)\n"
              << "  -b, --use-bunch-crossing    Enable bunch crossing logic\n"
              << "  -s, --static-events         Use static number of events per timeslice\n"
              << "  -e, --events-per-slice N    Static events per timeslice (default: 1)\n"
              << "  --beam-attachment           Enable beam attachment with Gaussian smearing\n"
              << "  --beam-speed SPEED          Beam speed in ns/mm (default: 299792.458)\n"
              << "  --beam-spread SPREAD        Beam spread for Gaussian smearing (default: 0.0)\n"
              << "  --status-offset OFFSET      Generator status offset (default: 0)\n"
              << "  -h, --help                  Show this help message\n";
}

int main(int argc, char* argv[]) {
    StandaloneMergerConfig config;
    
    static struct option long_options[] = {
        {"output", required_argument, 0, 'o'},
        {"nevents", required_argument, 0, 'n'},
        {"duration", required_argument, 0, 'd'},
        {"frequency", required_argument, 0, 'f'},
        {"bunch-period", required_argument, 0, 'p'},
        {"use-bunch-crossing", no_argument, 0, 'b'},
        {"static-events", no_argument, 0, 's'},
        {"events-per-slice", required_argument, 0, 'e'},
        {"beam-attachment", no_argument, 0, 1000},
        {"beam-speed", required_argument, 0, 1001},
        {"beam-spread", required_argument, 0, 1002},
        {"status-offset", required_argument, 0, 1003},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "o:n:d:f:p:bse:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'o':
                config.output_file = optarg;
                break;
            case 'n':
                config.max_events = std::stoul(optarg);
                break;
            case 'd':
                config.time_slice_duration = std::stof(optarg);
                break;
            case 'f':
                config.mean_event_frequency = std::stof(optarg);
                break;
            case 'p':
                config.bunch_crossing_period = std::stof(optarg);
                break;
            case 'b':
                config.use_bunch_crossing = true;
                break;
            case 's':
                config.static_number_of_events = true;
                break;
            case 'e':
                config.static_events_per_timeslice = std::stoul(optarg);
                break;
            case 1000:
                config.attach_to_beam = true;
                break;
            case 1001:
                config.beam_speed = std::stof(optarg);
                break;
            case 1002:
                config.beam_spread = std::stof(optarg);
                break;
            case 1003:
                config.generator_status_offset = std::stoi(optarg);
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    // Collect input files
    for (int i = optind; i < argc; i++) {
        config.input_files.push_back(argv[i]);
    }
    
    if (config.input_files.empty()) {
        std::cerr << "Error: No input files specified\n";
        printUsage(argv[0]);
        return 1;
    }
    
    // Print configuration
    std::cout << "=== Standalone Timeslice Merger Configuration ===" << std::endl;
    std::cout << "Input files: ";
    for (const auto& file : config.input_files) {
        std::cout << file << " ";
    }
    std::cout << std::endl;
    std::cout << "Output file: " << config.output_file << std::endl;
    std::cout << "Max events: " << config.max_events << std::endl;
    std::cout << "Timeslice duration: " << config.time_slice_duration << " ns" << std::endl;
    std::cout << "Use bunch crossing: " << (config.use_bunch_crossing ? "true" : "false") << std::endl;
    std::cout << "Bunch crossing period: " << config.bunch_crossing_period << " ns" << std::endl;
    std::cout << "Static number of events: " << (config.static_number_of_events ? "true" : "false") << std::endl;
    std::cout << "Events per timeslice: " << config.static_events_per_timeslice << std::endl;
    std::cout << "Mean event frequency: " << config.mean_event_frequency << " events/ns" << std::endl;
    std::cout << "Beam attachment: " << (config.attach_to_beam ? "true" : "false") << std::endl;
    std::cout << "Beam speed: " << config.beam_speed << " ns/mm" << std::endl;
    std::cout << "Beam spread: " << config.beam_spread << std::endl;
    std::cout << "Generator status offset: " << config.generator_status_offset << std::endl;
    std::cout << "================================================" << std::endl;
    
    try {
        StandaloneTimesliceMerger merger(config);
        merger.run();
        
        std::cout << "Successfully completed timeslice merging!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}