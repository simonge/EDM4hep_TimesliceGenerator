#pragma once

#include <JANA/JEventSource.h>
#include <JANA/JEventSourceGeneratorT.h>
#include <JANA/JApplication.h>
#include <memory>
#include <string>
#include <vector>
#include <random>

// Forward declarations
class TimeframeBuilder;
class DataHandler;
class DataSource;
struct MergerConfig;

/**
 * @class JEventSourceTimeframeBuilderEDM4hep
 * @brief JANA2 event source that provides merged timeframes from EDM4hep files
 * 
 * This event source integrates the TimeframeBuilder into JANA2, allowing merged
 * timeframe events to be consumed by JANA2 factories and processors. It uses the
 * same configuration and merging logic as the standalone timeframe_builder tool,
 * but outputs events one at a time to the JANA2 event processing pipeline.
 */
class JEventSourceTimeframeBuilderEDM4hep : public JEventSource {
public:
    JEventSourceTimeframeBuilderEDM4hep(std::string resource_name, JApplication* app);
    virtual ~JEventSourceTimeframeBuilderEDM4hep();

    void Open() override;
    void Close() override;
    Result Emit(JEvent& event) override;
    
    static std::string GetDescription();

private:
    // Configuration
    MergerConfig m_config;
    
    // Core components
    std::unique_ptr<TimeframeBuilder> m_merger;
    std::unique_ptr<DataHandler> m_data_handler;
    std::vector<std::unique_ptr<DataSource>> m_data_sources;
    
    // Random number generator
    std::random_device m_rd;
    std::mt19937 m_gen;
    
    // State tracking
    size_t m_timeframe_number{0};
    size_t m_max_timeframes{0};
    bool m_finished{false};
    
    // Helper methods
    void initializeConfiguration();
    void initializeMerger();
    bool updateInputNEvents();
};

template <>
double JEventSourceGeneratorT<JEventSourceTimeframeBuilderEDM4hep>::CheckOpenable(std::string);
