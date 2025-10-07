#include "StandaloneTimesliceMerger.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <string>
#include <vector>
#include <getopt.h>
#include <map>
#include <sstream>
#include <algorithm>
#include <cctype>

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] input_file1 [input_file2 ...]\n"
              << "\nGeneral Options:\n"
              << "  --config FILE                YAML config file\n"
              << "  -o, --output FILE           Output file name (default: merged_timeslices.root)\n"
              << "  -n, --nevents N             Maximum number of timeslices to generate (default: 100)\n"
              << "  -d, --duration TIME         Timeslice duration in ns (default: 20.0)\n"
              << "  -p, --bunch-period PERIOD   Bunch crossing period in ns (default: 10.0)\n"
              << "  -h, --help                  Show this help message\n"
              << "\nDefault Source Options (backward compatibility):\n"
              << "  -f, --frequency FREQ        Mean event frequency (events/ns) (default: 1.0)\n"
              << "  -b, --use-bunch-crossing    Enable bunch crossing logic\n"
              << "  -s, --static-events         Use static number of events per timeslice\n"
              << "  -e, --events-per-slice N    Static events per timeslice (default: 1)\n"
              << "  --beam-attachment           Enable beam attachment with Gaussian smearing\n"
              << "  --beam-speed SPEED          Beam speed in m/ns (default: 0.299792458)\n"
              << "  --beam-spread SPREAD        Beam spread for Gaussian smearing (default: 0.0)\n"
              << "  --status-offset OFFSET      Generator status offset (default: 0)\n"
              << "\nSource-Specific Options:\n"
              << "  --source:NAME               Create or select source named NAME\n"
              << "  --source:NAME:input_files FILE1,FILE2\n"
              << "                              Input files for source (comma-separated)\n"
              << "  --source:NAME:frequency FREQ\n"
              << "                              Mean event frequency for source\n"
              << "  --source:NAME:static_events BOOL\n"
              << "                              Use static events (true/false)\n"
              << "  --source:NAME:events_per_slice N\n"
              << "                              Static events per timeslice\n"
              << "  --source:NAME:bunch_crossing BOOL\n"
              << "                              Enable bunch crossing (true/false)\n"
              << "  --source:NAME:beam_attachment BOOL\n"
              << "                              Enable beam attachment (true/false)\n"
              << "  --source:NAME:beam_speed SPEED\n"
              << "                              Beam speed in ns/mm\n"
              << "  --source:NAME:beam_spread SPREAD\n"
              << "                              Beam spread for Gaussian smearing\n"
              << "  --source:NAME:status_offset OFFSET\n"
              << "                              Generator status offset\n"
              << "  --source:NAME:repeat_on_eof BOOL\n"
              << "                              Repeat source when EOF reached (true/false)\n"
              << "\nExamples:\n"
              << "  # Create signal source with specific files and frequency\n"
              << "  " << program_name << " --source:signal:input_files signal1.root,signal2.root --source:signal:frequency 0.5\n"
              << "  # Create background source with static events\n"
              << "  " << program_name << " --source:bg:input_files bg.root --source:bg:static_events true --source:bg:events_per_slice 2\n";
}

// Helper function to parse boolean values
bool parseBool(const std::string& value) {
    std::string lower_value = value;
    std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
    return (lower_value == "true" || lower_value == "1" || lower_value == "yes" || lower_value == "on");
}

// Helper function to split comma-separated values
std::vector<std::string> splitCommaSeparated(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

// Helper function to find or create a source by name
SourceConfig* findOrCreateSource(std::vector<SourceConfig>& sources, const std::string& name) {
    for (auto& source : sources) {
        if (source.name == name) {
            return &source;
        }
    }
    // Create new source
    SourceConfig new_source;
    new_source.name = name;
    sources.push_back(new_source);
    return &sources.back();
}

// Helper function to handle source-specific options
bool handleSourceOption(std::vector<SourceConfig>& sources, const std::string& option, const std::string& value) {
    // Expected format: source:name:property
    size_t first_colon = option.find(':');
    if (first_colon == std::string::npos || first_colon == 0) {
        return false;
    }
    
    std::string prefix = option.substr(0, first_colon);
    if (prefix != "source") {
        return false;
    }
    
    std::string rest = option.substr(first_colon + 1);
    size_t second_colon = rest.find(':');
    
    // Handle --source:name (create source)
    if (second_colon == std::string::npos) {
        std::string source_name = rest;
        findOrCreateSource(sources, source_name);
        return true;
    }
    
    // Handle --source:name:property
    std::string source_name = rest.substr(0, second_colon);
    std::string property = rest.substr(second_colon + 1);
    
    SourceConfig* source = findOrCreateSource(sources, source_name);
    
    if (property == "input_files") {
        source->input_files = splitCommaSeparated(value);
    } else if (property == "frequency") {
        source->mean_event_frequency = std::stof(value);
    } else if (property == "static_events") {
        source->static_number_of_events = parseBool(value);
    } else if (property == "events_per_slice") {
        source->static_events_per_timeslice = std::stoul(value);
    } else if (property == "bunch_crossing") {
        source->use_bunch_crossing = parseBool(value);
    } else if (property == "beam_attachment") {
        source->attach_to_beam = parseBool(value);
    } else if (property == "beam_speed") {
        source->beam_speed = std::stof(value);
    } else if (property == "beam_spread") {
        source->beam_spread = std::stof(value);
    } else if (property == "status_offset") {
        source->generator_status_offset = std::stoi(value);
    } else if (property == "already_merged") {
        source->already_merged = parseBool(value);
    } else if (property == "tree_name") {
        source->tree_name = value;
    } else if (property == "beam_angle") {
        source->beam_angle = std::stof(value);
    } else if (property == "repeat_on_eof") {
        source->repeat_on_eof = parseBool(value);
    } else {
        std::cerr << "Warning: Unknown source property: " << property << std::endl;
        return false;
    }
    
    return true;
}

// -------------------------------------------------------------------------------------------------------
// Main function
// -------------------------------------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    MergerConfig config;
    SourceConfig default_source; // Default source config
    std::string config_file = "";
    std::vector<SourceConfig> cli_sources; // Sources defined via CLI
    
    // First pass: extract source-specific options and process them separately
    std::vector<char*> remaining_args;
    remaining_args.push_back(argv[0]); // program name
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg.find("--source:") == 0) {
            // Handle source-specific option
            std::string option_name = arg.substr(2); // Remove "--"
            std::string value = "";
            
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                value = argv[i + 1];
                i++; // Skip the value argument
            }
            
            if (!handleSourceOption(cli_sources, option_name, value)) {
                std::cerr << "Error: Invalid source option: " << arg << std::endl;
                return 1;
            }
        } else {
            // Regular argument - keep for standard processing
            remaining_args.push_back(argv[i]);
        }
    }
    
    // Update argc/argv for standard getopt processing
    int new_argc = remaining_args.size();
    char** new_argv = remaining_args.data();
    
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
        {"beam-attachment", no_argument, 0, 1000},
        {"beam-speed", required_argument, 0, 1001},
        {"beam-spread", required_argument, 0, 1002},
        {"status-offset", required_argument, 0, 1003},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(new_argc, new_argv, "o:n:d:f:p:bse:h", long_options, &option_index)) != -1) {
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
                printUsage(new_argv[0]);
                return 0;
            default:
                printUsage(new_argv[0]);
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
        
        if (yaml["sources"]) {
            config.sources.clear();
            for (const auto& source_yaml : yaml["sources"]) {
                SourceConfig source;
                if (source_yaml["input_files"]) {
                    for (const auto& f : source_yaml["input_files"]) {
                        source.input_files.push_back(f.as<std::string>());
                    }
                }
                if (source_yaml["name"]) source.name = source_yaml["name"].as<std::string>();
                if (source_yaml["already_merged"]) source.already_merged = source_yaml["already_merged"].as<bool>();
                if (source_yaml["static_number_of_events"]) source.static_number_of_events = source_yaml["static_number_of_events"].as<bool>();
                if (source_yaml["static_events_per_timeslice"]) source.static_events_per_timeslice = source_yaml["static_events_per_timeslice"].as<size_t>();
                if (source_yaml["mean_event_frequency"]) source.mean_event_frequency = source_yaml["mean_event_frequency"].as<float>();
                if (source_yaml["use_bunch_crossing"]) source.use_bunch_crossing = source_yaml["use_bunch_crossing"].as<bool>();
                if (source_yaml["attach_to_beam"]) source.attach_to_beam = source_yaml["attach_to_beam"].as<bool>();
                if (source_yaml["beam_angle"]) source.beam_angle = source_yaml["beam_angle"].as<float>();
                if (source_yaml["beam_speed"]) source.beam_speed = source_yaml["beam_speed"].as<float>();
                if (source_yaml["beam_spread"]) source.beam_spread = source_yaml["beam_spread"].as<float>();
                if (source_yaml["generator_status_offset"]) source.generator_status_offset = source_yaml["generator_status_offset"].as<int32_t>();
                if (source_yaml["repeat_on_eof"]) source.repeat_on_eof = source_yaml["repeat_on_eof"].as<bool>();
                config.sources.push_back(source);
            }
        }
    }
    
    // Merge CLI sources with existing config sources
    // CLI sources can override YAML sources with the same name
    for (const auto& cli_source : cli_sources) {
        bool found = false;
        for (auto& existing_source : config.sources) {
            if (existing_source.name == cli_source.name) {
                // Override YAML source with CLI values (only non-default values)
                if (!cli_source.input_files.empty()) {
                    existing_source.input_files = cli_source.input_files;
                }
                if (cli_source.mean_event_frequency != 1.0f) {
                    existing_source.mean_event_frequency = cli_source.mean_event_frequency;
                }
                if (cli_source.static_number_of_events) {
                    existing_source.static_number_of_events = cli_source.static_number_of_events;
                }
                if (cli_source.static_events_per_timeslice != 1) {
                    existing_source.static_events_per_timeslice = cli_source.static_events_per_timeslice;
                }
                if (cli_source.use_bunch_crossing) {
                    existing_source.use_bunch_crossing = cli_source.use_bunch_crossing;
                }
                if (cli_source.attach_to_beam) {
                    existing_source.attach_to_beam = cli_source.attach_to_beam;
                }
                if (cli_source.beam_speed != 299792.4580f) {
                    existing_source.beam_speed = cli_source.beam_speed;
                }
                if (cli_source.beam_spread != 0.0f) {
                    existing_source.beam_spread = cli_source.beam_spread;
                }
                if (cli_source.generator_status_offset != 0) {
                    existing_source.generator_status_offset = cli_source.generator_status_offset;
                }
                if (cli_source.already_merged) {
                    existing_source.already_merged = cli_source.already_merged;
                }
                if (cli_source.tree_name != "events") {
                    existing_source.tree_name = cli_source.tree_name;
                }
                if (cli_source.beam_angle != 0.0f) {
                    existing_source.beam_angle = cli_source.beam_angle;
                }
                if (cli_source.repeat_on_eof) {
                    existing_source.repeat_on_eof = cli_source.repeat_on_eof;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            // Add new CLI source
            config.sources.push_back(cli_source);
        }
    }
    
    // Command-line input files override YAML - add to default source
    for (int i = optind; i < new_argc; i++) {
        default_source.input_files.push_back(new_argv[i]);
    }
    
    // If we have command-line files or no sources from any method, add default source
    if (!default_source.input_files.empty() || (config.sources.empty() && cli_sources.empty())) {
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
        printUsage(new_argv[0]);
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
        std::cout << "  Name: " << source.name << std::endl;
        std::cout << "  Static number of events: " << (source.static_number_of_events ? "true" : "false") << std::endl;
        std::cout << "  Events per timeslice: " << source.static_events_per_timeslice << std::endl;
        std::cout << "  Mean event frequency: " << source.mean_event_frequency << " events/ns" << std::endl;
        std::cout << "  Use bunch crossing: " << (source.use_bunch_crossing ? "true" : "false") << std::endl;
        std::cout << "  Beam attachment: " << (source.attach_to_beam ? "true" : "false") << std::endl;
        std::cout << "  Beam speed: " << source.beam_speed << " ns/mm" << std::endl;
        std::cout << "  Beam spread: " << source.beam_spread << std::endl;
        std::cout << "  Generator status offset: " << source.generator_status_offset << std::endl;
        std::cout << "  Repeat on EOF: " << (source.repeat_on_eof ? "true" : "false") << std::endl;
    }
    std::cout << "Output file: " << config.output_file << std::endl;
    std::cout << "Max events: " << config.max_events << std::endl;
    std::cout << "Timeslice duration: " << config.time_slice_duration << " ns" << std::endl;
    std::cout << "Bunch crossing period: " << config.bunch_crossing_period << " ns" << std::endl;
    std::cout << "Introduce offsets: " << (config.introduce_offsets ? "true" : "false") << std::endl;
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