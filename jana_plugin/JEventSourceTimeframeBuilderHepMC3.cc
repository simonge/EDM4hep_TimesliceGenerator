#include "JEventSourceTimeframeBuilderHepMC3.h"
#include "../include/TimeframeBuilder.h"
#include "../include/DataHandler.h"
#include "../include/DataSource.h"
#include "../include/MergerConfig.h"

#ifdef HAVE_HEPMC3
#include "../include/HepMC3DataSource.h"
#include "../include/HepMC3DataHandler.h"
#endif

#include <JANA/JEvent.h>
#include <JANA/JException.h>

#include <TFile.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
JEventSourceTimeframeBuilderHepMC3::JEventSourceTimeframeBuilderHepMC3(
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
JEventSourceTimeframeBuilderHepMC3::~JEventSourceTimeframeBuilderHepMC3() = default;

//------------------------------------------------------------------------------
// Open
//------------------------------------------------------------------------------
void JEventSourceTimeframeBuilderHepMC3::Open() {
#ifdef HAVE_HEPMC3
    // Initialize configuration from JANA parameters
    initializeConfiguration();
    
    // Initialize the merger
    initializeMerger();
    
    std::cout << "JEventSourceTimeframeBuilderHepMC3: Opened with " 
              << m_config.sources.size() << " sources" << std::endl;
#else
    throw JException("HepMC3 support not compiled in");
#endif
}

//------------------------------------------------------------------------------
// Close
//------------------------------------------------------------------------------
void JEventSourceTimeframeBuilderHepMC3::Close() {
#ifdef HAVE_HEPMC3
    if (m_data_handler) {
        m_data_handler->finalize();
    }
    m_data_sources.clear();
    m_data_handler.reset();
#endif
}

//------------------------------------------------------------------------------
// Emit
//------------------------------------------------------------------------------
JEventSourceTimeframeBuilderHepMC3::Result 
JEventSourceTimeframeBuilderHepMC3::Emit(JEvent& event) {
    
#ifdef HAVE_HEPMC3
    // Check if we've reached the maximum number of timeframes
    if (m_finished || m_timeframe_number >= m_max_timeframes) {
        return Result::FailureFinished;
    }
    
    // Update number of events needed per source
    if (!updateInputNEvents()) {
        std::cout << "JEventSourceTimeframeBuilderHepMC3: Reached end of input data at timeframe " 
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
    
    // The merged data is already in the data handler's internal structures
    // For HepMC3, we would need to properly expose the merged GenEvent
    
    m_timeframe_number++;
    
    // Report progress periodically
    if (m_timeframe_number % 10 == 0) {
        std::cout << "JEventSourceTimeframeBuilderHepMC3: Processed " 
                  << m_timeframe_number << " timeframes..." << std::endl;
    }
    
    return Result::Success;
#else
    return Result::FailureTryNext;
#endif
}

//------------------------------------------------------------------------------
// GetDescription
//------------------------------------------------------------------------------
std::string JEventSourceTimeframeBuilderHepMC3::GetDescription() {
    return "TimeframeBuilder HepMC3 Event Source - Merges multiple HepMC3 events into timeframes";
}

//------------------------------------------------------------------------------
// CheckOpenable
//------------------------------------------------------------------------------
template <>
double JEventSourceGeneratorT<JEventSourceTimeframeBuilderHepMC3>::CheckOpenable(
    std::string resource_name) {
    
#ifdef HAVE_HEPMC3
    // Check if this is a HepMC3 ROOT file
    if (resource_name.find(".hepmc3.tree.root") != std::string::npos) {
        // Try to open the file to verify it's valid
        TFile* file = TFile::Open(resource_name.c_str());
        if (file && !file->IsZombie()) {
            file->Close();
            delete file;
            return 0.8; // High confidence for .hepmc3.tree.root files
        }
        if (file) {
            file->Close();
            delete file;
        }
    }
#endif
    
    return 0.0; // Not openable
}

//------------------------------------------------------------------------------
// initializeConfiguration
//------------------------------------------------------------------------------
void JEventSourceTimeframeBuilderHepMC3::initializeConfiguration() {
#ifdef HAVE_HEPMC3
    auto* app = GetApplication();
    
    // Get global configuration parameters from JANA
    // All parameters are registered in timeframe_builder_plugin.cc
    
    // Global timeframe configuration
    m_config.timeframe_duration = app->GetParameterValue<float>("tfb:timeframe_duration");
    m_config.bunch_crossing_period = app->GetParameterValue<float>("tfb:bunch_crossing_period");
    m_config.max_events = app->GetParameterValue<int>("tfb:max_timeframes");
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
            source.static_events_per_timeframe = app->GetParameterValue<int>(
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
            source.generator_status_offset = app->GetParameterValue<int>(
                prefix + "status_offset", 0);
            source.already_merged = app->GetParameterValue<bool>(
                prefix + "already_merged", false);
            source.tree_name = app->GetParameterValue<std::string>(
                prefix + "tree_name", "hepmc3_tree");
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
        source.static_events_per_timeframe = app->GetParameterValue<int>("tfb:events_per_frame");
        source.mean_event_frequency = app->GetParameterValue<float>("tfb:event_frequency");
        source.use_bunch_crossing = app->GetParameterValue<bool>("tfb:use_bunch_crossing");
        source.attach_to_beam = app->GetParameterValue<bool>("tfb:attach_to_beam");
        source.beam_angle = app->GetParameterValue<float>("tfb:beam_angle");
        source.beam_speed = app->GetParameterValue<float>("tfb:beam_speed");
        source.beam_spread = app->GetParameterValue<float>("tfb:beam_spread");
        source.generator_status_offset = app->GetParameterValue<int>("tfb:status_offset");
        source.already_merged = app->GetParameterValue<bool>("tfb:already_merged");
        source.tree_name = app->GetParameterValue<std::string>("tfb:tree_name");
        source.repeat_on_eof = app->GetParameterValue<bool>("tfb:repeat_on_eof");
        
        m_config.sources.push_back(source);
    }
#endif
}

//------------------------------------------------------------------------------
// initializeMerger
//------------------------------------------------------------------------------
void JEventSourceTimeframeBuilderHepMC3::initializeMerger() {
#ifdef HAVE_HEPMC3
    // Create the data handler for HepMC3 format
    m_data_handler = std::make_unique<HepMC3DataHandler>();
    
    // Initialize data sources
    // Note: output_file is still needed for the DataHandler initialization
    // even if we're not writing output. This is a limitation of the current
    // DataHandler design. Use a temporary name that won't be written to.
    std::string output_file = m_config.output_file.empty() 
        ? "timeframe_builder_tmp.hepmc3.tree.root" 
        : m_config.output_file;
    
    m_data_sources = m_data_handler->initializeDataSources(
        output_file,
        m_config.sources);
#endif
}

//------------------------------------------------------------------------------
// updateInputNEvents
//------------------------------------------------------------------------------
bool JEventSourceTimeframeBuilderHepMC3::updateInputNEvents() {
#ifdef HAVE_HEPMC3
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
#else
    return false;
#endif
}
