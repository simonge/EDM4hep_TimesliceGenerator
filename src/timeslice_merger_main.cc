#include "StandaloneTimesliceMerger.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <string>
#include <vector>
#include <getopt.h>

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] input_file1 [input_file2 ...]\n"
              << "Options:\n"
              << "  --config FILE                YAML config file (default: config.yml)\n"
              << "  -o, --output FILE           Output file name (default: merged_timeslices.root)\n"
              << "  -n, --nevents N             Maximum number of timeslices to generate (default: 100)\n"
              << "  -d, --duration TIME         Timeslice duration in ns (default: 20.0)\n"
              << "  -f, --frequency FREQ        Mean event frequency (events/ns) (default: 1.0)\n"
              << "  -p, --bunch-period PERIOD   Bunch crossing period in ns (default: 10.0)\n"
              << "  -b, --use-bunch-crossing    Enable bunch crossing logic\n"
              << "  -s, --static-events         Use static number of events per timeslice\n"
              << "  -e, --events-per-slice N    Static events per timeslice (default: 1)\n"
              << "  -m, --merge-mode MODE       Merge mode: 'edm4hep' (hit collections) or 'edm4eic' (MCParticles/Vertices) (default: edm4hep)\n"
              << "  --beam-attachment           Enable beam attachment with Gaussian smearing\n"
              << "  --beam-speed SPEED          Beam speed in ns/mm (default: 299792.458)\n"
              << "  --beam-spread SPREAD        Beam spread for Gaussian smearing (default: 0.0)\n"
              << "  --status-offset OFFSET      Generator status offset (default: 0)\n"
              << "  -h, --help                  Show this help message\n";
}

int main(int argc, char* argv[]) {
    MergerConfig config;
    SourceConfig default_source; // Default source config
    std::string config_file = "";
    
    static struct option long_options[] = {
    {"config", required_argument, 0, 1004},
        {"output", required_argument, 0, 'o'},
        {"nevents", required_argument, 0, 'n'},
        {"duration", required_argument, 0, 'd'},
        {"frequency", required_argument, 0, 'f'},
        {"bunch-period", required_argument, 0, 'p'},
        {"use-bunch-crossing", no_argument, 0, 'b'},
        {"static-events", no_argument, 0, 's'},
        {"events-per-slice", required_argument, 0, 'e'},
        {"merge-mode", required_argument, 0, 'm'},
        {"beam-attachment", no_argument, 0, 1000},
        {"beam-speed", required_argument, 0, 1001},
        {"beam-spread", required_argument, 0, 1002},
        {"status-offset", required_argument, 0, 1003},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "o:n:d:f:p:bse:m:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 1004:
                config_file = optarg;
                break;
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
                default_source.mean_event_frequency = std::stof(optarg);
                break;
            case 'p':
                config.bunch_crossing_period = std::stof(optarg);
                break;
            case 'b':
                default_source.use_bunch_crossing = true;
                break;
            case 's':
                default_source.static_number_of_events = true;
                break;
            case 'e':
                default_source.static_events_per_timeslice = std::stoul(optarg);
                break;
            case 'm':
                config.merge_mode = optarg;
                break;
            case 1000:
                default_source.attach_to_beam = true;
                break;
            case 1001:
                default_source.beam_speed = std::stof(optarg);
                break;
            case 1002:
                default_source.beam_spread = std::stof(optarg);
                break;
            case 1003:
                default_source.generator_status_offset = std::stoi(optarg);
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    // If config_file specified, parse YAML
    if (!config_file.empty()) {
        YAML::Node yaml = YAML::LoadFile(config_file);
        if (yaml["output_file"]) config.output_file = yaml["output_file"].as<std::string>();
        if (yaml["max_events"]) config.max_events = yaml["max_events"].as<size_t>();
        if (yaml["time_slice_duration"]) config.time_slice_duration = yaml["time_slice_duration"].as<float>();
        if (yaml["bunch_crossing_period"]) config.bunch_crossing_period = yaml["bunch_crossing_period"].as<float>();
        if (yaml["introduce_offsets"]) config.introduce_offsets = yaml["introduce_offsets"].as<bool>();
        if (yaml["merge_mode"]) config.merge_mode = yaml["merge_mode"].as<std::string>();
        
        if (yaml["sources"]) {
            config.sources.clear();
            for (const auto& source_yaml : yaml["sources"]) {
                SourceConfig source;
                if (source_yaml["input_files"]) {
                    for (const auto& f : source_yaml["input_files"]) {
                        source.input_files.push_back(f.as<std::string>());
                    }
                }
                if (source_yaml["static_number_of_events"]) source.static_number_of_events = source_yaml["static_number_of_events"].as<bool>();
                if (source_yaml["static_events_per_timeslice"]) source.static_events_per_timeslice = source_yaml["static_events_per_timeslice"].as<size_t>();
                if (source_yaml["mean_event_frequency"]) source.mean_event_frequency = source_yaml["mean_event_frequency"].as<float>();
                if (source_yaml["use_bunch_crossing"]) source.use_bunch_crossing = source_yaml["use_bunch_crossing"].as<bool>();
                if (source_yaml["attach_to_beam"]) source.attach_to_beam = source_yaml["attach_to_beam"].as<bool>();
                if (source_yaml["beam_angle"]) source.beam_angle = source_yaml["beam_angle"].as<float>();
                if (source_yaml["beam_speed"]) source.beam_speed = source_yaml["beam_speed"].as<float>();
                if (source_yaml["beam_spread"]) source.beam_spread = source_yaml["beam_spread"].as<float>();
                if (source_yaml["generator_status_offset"]) source.generator_status_offset = source_yaml["generator_status_offset"].as<int32_t>();
                config.sources.push_back(source);
            }
        }
    }
    
    // Command-line input files override YAML - add to default source
    for (int i = optind; i < argc; i++) {
        default_source.input_files.push_back(argv[i]);
    }
    
    // If we have command-line files or no sources from YAML, add default source
    if (!default_source.input_files.empty() || config.sources.empty()) {
        config.sources.push_back(default_source);
    }
    
    // Check if we have any input files
    bool has_input_files = false;
    for (const auto& source : config.sources) {
        if (!source.input_files.empty()) {
            has_input_files = true;
            break;
        }
    }
    
    if (!has_input_files) {
        std::cerr << "Error: No input files specified\n";
        printUsage(argv[0]);
        return 1;
    }
    
    // Print configuration
    std::cout << "=== Timeslice Merger Configuration ===" << std::endl;
    std::cout << "Sources: " << config.sources.size() << std::endl;
    for (size_t i = 0; i < config.sources.size(); ++i) {
        const auto& source = config.sources[i];
        std::cout << "Source " << i << " input files: ";
        for (const auto& file : source.input_files) {
            std::cout << file << " ";
        }
        std::cout << std::endl;
        std::cout << "  Static number of events: " << (source.static_number_of_events ? "true" : "false") << std::endl;
        std::cout << "  Events per timeslice: " << source.static_events_per_timeslice << std::endl;
        std::cout << "  Mean event frequency: " << source.mean_event_frequency << " events/ns" << std::endl;
        std::cout << "  Use bunch crossing: " << (source.use_bunch_crossing ? "true" : "false") << std::endl;
        std::cout << "  Beam attachment: " << (source.attach_to_beam ? "true" : "false") << std::endl;
        std::cout << "  Beam speed: " << source.beam_speed << " ns/mm" << std::endl;
        std::cout << "  Beam spread: " << source.beam_spread << std::endl;
        std::cout << "  Generator status offset: " << source.generator_status_offset << std::endl;
    }
    std::cout << "Output file: " << config.output_file << std::endl;
    std::cout << "Max events: " << config.max_events << std::endl;
    std::cout << "Timeslice duration: " << config.time_slice_duration << " ns" << std::endl;
    std::cout << "Bunch crossing period: " << config.bunch_crossing_period << " ns" << std::endl;
    std::cout << "Introduce offsets: " << (config.introduce_offsets ? "true" : "false") << std::endl;
    std::cout << "Merge mode: " << config.merge_mode << std::endl;
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