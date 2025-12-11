#include "HepMC3TimesliceMerger.h"
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
    std::cout << "HepMC3 Timeslice Merger - Merge HepMC3 events into timeslices\n\n"
              << "Usage: " << program_name << " [options]\n"
              << "\nGeneral Options:\n"
              << "  --config FILE                YAML config file\n"
              << "  -o, --output FILE           Output file name (default: merged_timeslices.hepmc3.tree.root)\n"
              << "  -n, --nevents N             Maximum number of timeslices to generate (default: 100)\n"
              << "  -d, --duration TIME         Timeslice duration in ns (default: 2000.0)\n"
              << "  -p, --bunch-period PERIOD   Bunch crossing period in ns (default: 10.0)\n"
              << "  -h, --help                  Show this help message\n"
              << "\nSource-Specific Options:\n"
              << "  --source:NAME               Create or select source named NAME\n"
              << "  --source:NAME:input_files FILE1,FILE2\n"
              << "                              Input files for source (comma-separated)\n"
              << "  --source:NAME:frequency FREQ\n"
              << "                              Mean event frequency (events/ns) for source\n"
              << "                              Set to 0 for single event per slice (signal mode)\n"
              << "                              Set to negative for weighted mode\n"
              << "  --source:NAME:static_events BOOL\n"
              << "                              Use static events (true/false)\n"
              << "  --source:NAME:events_per_slice N\n"
              << "                              Static events per timeslice\n"
              << "  --source:NAME:bunch_crossing BOOL\n"
              << "                              Enable bunch crossing (true/false)\n"
              << "  --source:NAME:status_offset OFFSET\n"
              << "                              Generator status offset\n"
              << "  --source:NAME:repeat_on_eof BOOL\n"
              << "                              Cycle back to start when EOF reached (true/false)\n"
              << "\nExamples:\n"
              << "  # Single signal source with one event per slice\n"
              << "  " << program_name << " --source:signal:input_files signal.hepmc3.tree.root --source:signal:frequency 0\n\n"
              << "  # Signal + background with Poisson distribution\n"
              << "  " << program_name << " --source:signal:input_files signal.root --source:signal:frequency 0 \\\n"
              << "    --source:bg:input_files bg.root --source:bg:frequency 0.02 --source:bg:status_offset 1000\n\n"
              << "  # Using YAML configuration\n"
              << "  " << program_name << " --config hepmc3_config.yml\n\n"
              << "Note: This merger uses the same configuration format as the EDM4hep merger.\n";
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
    } else if (property == "status_offset") {
        source->generator_status_offset = std::stoi(value);
    } else if (property == "repeat_on_eof") {
        source->repeat_on_eof = parseBool(value);
    } else {
        std::cerr << "Warning: Unknown source property: " << property << std::endl;
        return false;
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    MergerConfig config;
    config.output_file = "merged_timeslices.hepmc3.tree.root";
    std::string config_file = "";
    std::vector<SourceConfig> cli_sources;
    
    // First pass: extract source-specific options
    std::vector<char*> remaining_args;
    remaining_args.push_back(argv[0]);
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg.find("--source:") == 0) {
            std::string option_name = arg.substr(2);
            std::string value = "";
            
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                value = argv[i + 1];
                i++;
            }
            
            if (!handleSourceOption(cli_sources, option_name, value)) {
                std::cerr << "Error: Invalid source option: " << arg << std::endl;
                return 1;
            }
        } else {
            remaining_args.push_back(argv[i]);
        }
    }
    
    int new_argc = remaining_args.size();
    char** new_argv = remaining_args.data();
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 1000},
        {"output", required_argument, 0, 'o'},
        {"nevents", required_argument, 0, 'n'},
        {"duration", required_argument, 0, 'd'},
        {"bunch-period", required_argument, 0, 'p'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(new_argc, new_argv, "o:n:d:p:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 1000:
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
            case 'p':
                config.bunch_crossing_period = std::stof(optarg);
                break;
            case 'h':
                printUsage(new_argv[0]);
                return 0;
            default:
                printUsage(new_argv[0]);
                return 1;
        }
    }
    
    // Load YAML config if specified
    if (!config_file.empty()) {
        try {
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
                    if (source_yaml["static_number_of_events"]) source.static_number_of_events = source_yaml["static_number_of_events"].as<bool>();
                    if (source_yaml["static_events_per_timeslice"]) source.static_events_per_timeslice = source_yaml["static_events_per_timeslice"].as<size_t>();
                    if (source_yaml["mean_event_frequency"]) source.mean_event_frequency = source_yaml["mean_event_frequency"].as<float>();
                    if (source_yaml["use_bunch_crossing"]) source.use_bunch_crossing = source_yaml["use_bunch_crossing"].as<bool>();
                    if (source_yaml["generator_status_offset"]) source.generator_status_offset = source_yaml["generator_status_offset"].as<int32_t>();
                    if (source_yaml["repeat_on_eof"]) source.repeat_on_eof = source_yaml["repeat_on_eof"].as<bool>();
                    config.sources.push_back(source);
                }
            }
        } catch (const YAML::Exception& e) {
            std::cerr << "Error parsing YAML config: " << e.what() << std::endl;
            return 1;
        }
    }
    
    // Merge CLI sources with config sources
    // Note: This merge strategy checks if CLI values differ from defaults.
    // Limitation: Cannot override YAML true with CLI false for boolean flags.
    // This is acceptable as the typical use case is to add/modify values, not revert them.
    for (const auto& cli_source : cli_sources) {
        bool found = false;
        for (auto& existing_source : config.sources) {
            if (existing_source.name == cli_source.name) {
                // Override with CLI values where provided
                if (!cli_source.input_files.empty()) {
                    existing_source.input_files = cli_source.input_files;
                }
                // mean_event_frequency default is 1.0f, but allow frequency=0 for signal mode
                if (cli_source.mean_event_frequency != 1.0f) {
                    existing_source.mean_event_frequency = cli_source.mean_event_frequency;
                }
                // Boolean flags only set to true via CLI (limitation: can't unset YAML true values)
                if (cli_source.static_number_of_events) {
                    existing_source.static_number_of_events = cli_source.static_number_of_events;
                }
                // static_events_per_timeslice default is 1
                if (cli_source.static_events_per_timeslice != 1) {
                    existing_source.static_events_per_timeslice = cli_source.static_events_per_timeslice;
                }
                if (cli_source.use_bunch_crossing) {
                    existing_source.use_bunch_crossing = cli_source.use_bunch_crossing;
                }
                if (cli_source.generator_status_offset != 0) {
                    existing_source.generator_status_offset = cli_source.generator_status_offset;
                }
                if (cli_source.repeat_on_eof) {
                    existing_source.repeat_on_eof = cli_source.repeat_on_eof;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            config.sources.push_back(cli_source);
        }
    }
    
    // Check if we have sources
    if (config.sources.empty()) {
        std::cerr << "Error: No sources specified. Use --source:NAME:input_files or --config\n";
        printUsage(new_argv[0]);
        return 1;
    }
    
    // Validate that all sources have input files
    for (const auto& source : config.sources) {
        if (source.input_files.empty()) {
            std::cerr << "Error: Source '" << source.name << "' has no input files\n";
            return 1;
        }
    }
    
    // Run the merger
    try {
        HepMC3TimesliceMerger merger(config);
        merger.run();
        
        std::cout << "\n=== HepMC3 Timeslice Merging Complete ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
