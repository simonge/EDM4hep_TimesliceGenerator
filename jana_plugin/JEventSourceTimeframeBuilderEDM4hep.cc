#include "JEventSourceTimeframeBuilderEDM4hep.h"
#include "../include/TimeframeBuilder.h"
#include "../include/DataHandler.h"
#include "../include/DataSource.h"
#include "../include/MergerConfig.h"
#include "../include/EDM4hepDataSource.h"
#include "../include/EDM4hepDataHandler.h"

#include <JANA/JEvent.h>
#include <JANA/JException.h>
#include <podio/Frame.h>
#include <edm4hep/EventHeaderCollection.h>

#include <TFile.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
JEventSourceTimeframeBuilderEDM4hep::JEventSourceTimeframeBuilderEDM4hep(
    std::string resource_name, JApplication* app)
    : JEventSource(resource_name, app) {
    
    SetTypeName(NAME_OF_THIS);
    SetCallbackStyle(CallbackStyle::ExpertMode);
    
    // Initialize RNG
    m_gen = std::mt19937(m_rd());
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
JEventSourceTimeframeBuilderEDM4hep::~JEventSourceTimeframeBuilderEDM4hep() = default;

//------------------------------------------------------------------------------
// Open
//------------------------------------------------------------------------------
void JEventSourceTimeframeBuilderEDM4hep::Open() {
    // Initialize configuration from JANA parameters
    initializeConfiguration();
    
    // Initialize the merger
    initializeMerger();
    
    std::cout << "JEventSourceTimeframeBuilderEDM4hep: Opened with " 
              << m_config.sources.size() << " sources" << std::endl;
}

//------------------------------------------------------------------------------
// Close
//------------------------------------------------------------------------------
void JEventSourceTimeframeBuilderEDM4hep::Close() {
    if (m_data_handler) {
        m_data_handler->finalize();
    }
    m_data_sources.clear();
    m_data_handler.reset();
}

//------------------------------------------------------------------------------
// Emit
//------------------------------------------------------------------------------
JEventSourceTimeframeBuilderEDM4hep::Result 
JEventSourceTimeframeBuilderEDM4hep::Emit(JEvent& event) {
    
    // Check if we've reached the maximum number of timeframes
    if (m_finished || m_timeframe_number >= m_max_timeframes) {
        return Result::FailureFinished;
    }
    
    // Update number of events needed per source
    if (!updateInputNEvents()) {
        std::cout << "JEventSourceTimeframeBuilderEDM4hep: Reached end of input data at timeframe " 
                  << m_timeframe_number << std::endl;
        m_finished = true;
        return Result::FailureFinished;
    }
    
    // Prepare for new timeframe
    m_data_handler->prepareTimeframe();
    
    // Merge events from all sources
    m_data_handler->mergeEvents(
        m_data_sources, 
        m_timeframe_number, 
        m_config.timeframe_duration,
        m_config.bunch_crossing_period, 
        m_gen);
    
    // Set event number to timeframe number
    event.SetEventNumber(m_timeframe_number);
    event.SetRunNumber(1);
    
    // Get the merged collections from the data handler
    auto* edm4hep_handler = dynamic_cast<EDM4hepDataHandler*>(m_data_handler.get());
    if (edm4hep_handler) {
        const auto& merged = edm4hep_handler->getMergedCollections();
        
        // TODO: Insert merged collections into JEvent
        // This requires converting the *Data types back to proper PODIO collections
        // For now, this is a limitation - the merged data is available but not yet
        // properly exposed to JANA2 factories.
        // 
        // Potential approaches:
        // 1. Convert to podio::Frame and insert
        // 2. Create custom JFactories that access the merged collections directly
        // 3. Write to file and read back using standard PODIO JEventSource
        //
        // For the immediate use case, we can write the merged timeframe to file
        // and have downstream processors read from that file.
    }
    
    // Optionally write the timeframe to output file if configured
    if (!m_config.output_file.empty()) {
        m_data_handler->writeTimeframe();
    }
    
    m_timeframe_number++;
    
    // Report progress periodically
    if (m_timeframe_number % 10 == 0) {
        std::cout << "JEventSourceTimeframeBuilderEDM4hep: Processed " 
                  << m_timeframe_number << " timeframes..." << std::endl;
    }
    
    return Result::Success;
}

//------------------------------------------------------------------------------
// GetDescription
//------------------------------------------------------------------------------
std::string JEventSourceTimeframeBuilderEDM4hep::GetDescription() {
    return "TimeframeBuilder EDM4hep Event Source - Merges multiple EDM4hep events into timeframes";
}

//------------------------------------------------------------------------------
// CheckOpenable
//------------------------------------------------------------------------------
template <>
double JEventSourceGeneratorT<JEventSourceTimeframeBuilderEDM4hep>::CheckOpenable(
    std::string resource_name) {
    
    // Check if this is an EDM4hep ROOT file
    if (resource_name.find(".edm4hep.root") != std::string::npos) {
        // Try to open the file to verify it's valid
        TFile* file = TFile::Open(resource_name.c_str());
        if (file && !file->IsZombie()) {
            file->Close();
            delete file;
            return 0.8; // High confidence for .edm4hep.root files
        }
        if (file) {
            file->Close();
            delete file;
        }
    }
    
    return 0.0; // Not openable
}

//------------------------------------------------------------------------------
// initializeConfiguration
//------------------------------------------------------------------------------
void JEventSourceTimeframeBuilderEDM4hep::initializeConfiguration() {
    auto* app = GetApplication();
    
    // Get global configuration parameters from JANA
    // All parameters are registered in timeframe_builder_plugin.cc
    
    // Global timeframe configuration
    m_config.timeframe_duration = app->GetParameterValue<float>("tfb:timeframe_duration");
    m_config.bunch_crossing_period = app->GetParameterValue<float>("tfb:bunch_crossing_period");
    m_config.max_events = app->GetParameterValue<size_t>("tfb:max_timeframes");
    m_max_timeframes = m_config.max_events;
    m_config.random_seed = app->GetParameterValue<unsigned int>("tfb:random_seed");
    m_config.introduce_offsets = app->GetParameterValue<bool>("tfb:introduce_offsets");
    m_config.merge_particles = app->GetParameterValue<bool>("tfb:merge_particles");
    m_config.output_file = app->GetParameterValue<std::string>("tfb:output_file");
    
    // Initialize RNG with configured seed
    if (m_config.random_seed != 0) {
        m_gen = std::mt19937(m_config.random_seed);
    }
    
    // Check if multiple sources are specified
    std::string source_names_str = app->GetParameterValue<std::string>("tfb:source_names");
    
    if (!source_names_str.empty()) {
        // Parse multiple sources from configuration
        std::vector<std::string> source_names;
        std::stringstream ss(source_names_str);
        std::string name;
        while (std::getline(ss, name, ',')) {
            // Trim whitespace
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            if (!name.empty()) {
                source_names.push_back(name);
            }
        }
        
        // Create a source configuration for each named source
        for (const auto& src_name : source_names) {
            SourceConfig source;
            source.name = src_name;
            
            // Build parameter names for this source
            std::string prefix = "tfb:" + src_name + ":";
            
            // Get source-specific input files
            std::string files_str = app->GetParameterValue<std::string>(prefix + "input_files", "");
            if (!files_str.empty()) {
                std::stringstream file_ss(files_str);
                std::string file;
                while (std::getline(file_ss, file, ',')) {
                    file.erase(0, file.find_first_not_of(" \t"));
                    file.erase(file.find_last_not_of(" \t") + 1);
                    if (!file.empty()) {
                        source.input_files.push_back(file);
                    }
                }
            }
            
            // Get all source-specific parameters
            source.static_number_of_events = app->GetParameterValue<bool>(
                prefix + "static_events", false);
            source.static_events_per_timeframe = app->GetParameterValue<size_t>(
                prefix + "events_per_frame", 1);
            source.mean_event_frequency = app->GetParameterValue<float>(
                prefix + "event_frequency", 1.0f);
            source.use_bunch_crossing = app->GetParameterValue<bool>(
                prefix + "use_bunch_crossing", false);
            source.attach_to_beam = app->GetParameterValue<bool>(
                prefix + "attach_to_beam", false);
            source.beam_angle = app->GetParameterValue<float>(
                prefix + "beam_angle", 0.0f);
            source.beam_speed = app->GetParameterValue<float>(
                prefix + "beam_speed", 299.792458f);
            source.beam_spread = app->GetParameterValue<float>(
                prefix + "beam_spread", 0.0f);
            source.generator_status_offset = app->GetParameterValue<int32_t>(
                prefix + "status_offset", 0);
            source.already_merged = app->GetParameterValue<bool>(
                prefix + "already_merged", false);
            source.tree_name = app->GetParameterValue<std::string>(
                prefix + "tree_name", "events");
            source.repeat_on_eof = app->GetParameterValue<bool>(
                prefix + "repeat_on_eof", false);
            
            m_config.sources.push_back(source);
        }
    } else {
        // Single source mode - use input file and default parameters
        SourceConfig source;
        source.name = "input";
        source.input_files.push_back(GetResourceName());
        
        // Get default source parameters
        source.static_number_of_events = app->GetParameterValue<bool>("tfb:static_events");
        source.static_events_per_timeframe = app->GetParameterValue<size_t>("tfb:events_per_frame");
        source.mean_event_frequency = app->GetParameterValue<float>("tfb:event_frequency");
        source.use_bunch_crossing = app->GetParameterValue<bool>("tfb:use_bunch_crossing");
        source.attach_to_beam = app->GetParameterValue<bool>("tfb:attach_to_beam");
        source.beam_angle = app->GetParameterValue<float>("tfb:beam_angle");
        source.beam_speed = app->GetParameterValue<float>("tfb:beam_speed");
        source.beam_spread = app->GetParameterValue<float>("tfb:beam_spread");
        source.generator_status_offset = app->GetParameterValue<int32_t>("tfb:status_offset");
        source.already_merged = app->GetParameterValue<bool>("tfb:already_merged");
        source.tree_name = app->GetParameterValue<std::string>("tfb:tree_name");
        source.repeat_on_eof = app->GetParameterValue<bool>("tfb:repeat_on_eof");
        
        m_config.sources.push_back(source);
    }
}

//------------------------------------------------------------------------------
// initializeMerger
//------------------------------------------------------------------------------
void JEventSourceTimeframeBuilderEDM4hep::initializeMerger() {
    // Create the data handler for EDM4hep format
    m_data_handler = std::make_unique<EDM4hepDataHandler>();
    
    // Initialize data sources
    // Note: output_file is still needed for the DataHandler initialization
    // even if we're not writing output. This is a limitation of the current
    // DataHandler design. Use a temporary name that won't be written to.
    std::string output_file = m_config.output_file.empty() 
        ? "timeframe_builder_tmp.edm4hep.root" 
        : m_config.output_file;
    
    m_data_sources = m_data_handler->initializeDataSources(
        output_file,
        m_config.sources);
}

//------------------------------------------------------------------------------
// updateInputNEvents
//------------------------------------------------------------------------------
bool JEventSourceTimeframeBuilderEDM4hep::updateInputNEvents() {
    for (auto& data_source : m_data_sources) {
        const auto& config = data_source->getConfig();
        
        // Generate new number of events needed for this source
        if (config.already_merged) {
            data_source->setEntriesNeeded(1);
        } else if (config.static_number_of_events) {
            data_source->setEntriesNeeded(config.static_events_per_timeframe);
        } else {
            // Use Poisson distribution
            float mean_freq = config.mean_event_frequency;
            std::poisson_distribution<> poisson_dist(m_config.timeframe_duration * mean_freq);
            size_t n = poisson_dist(m_gen);
            data_source->setEntriesNeeded(n);
        }
        
        // Check if enough events are available
        if (!data_source->hasMoreEntries()) {
            return false;
        }
    }
    
    return true;
}
