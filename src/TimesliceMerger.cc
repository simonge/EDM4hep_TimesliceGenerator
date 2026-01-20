#include "TimesliceMerger.h"
#include "EDM4hepDataSource.h"

#include <iostream>
#include <stdexcept>

TimesliceMerger::TimesliceMerger(const MergerConfig& config)
    : m_config(config), gen(rd()) {}

void TimesliceMerger::setOutputHandler(std::unique_ptr<OutputHandler> handler) {
    output_handler_ = std::move(handler);
}

void TimesliceMerger::run() {
    std::cout << "Starting timeslice merger..." << std::endl;
    std::cout << "Sources: " << m_config.sources.size() << std::endl;
    std::cout << "Output file: " << m_config.output_file << std::endl;
    std::cout << "Max events: " << m_config.max_events << std::endl;
    std::cout << "Timeslice duration: " << m_config.time_slice_duration << std::endl;

    if (!output_handler_) {
        throw std::runtime_error("No output handler set - call setOutputHandler() before run()");
    }

    // Initialize data sources
    data_sources_ = initializeDataSources();

    // Initialize output handler
    output_handler_->initialize(m_config.output_file, data_sources_);

    std::cout << "Processing " << m_config.max_events << " timeslices..." << std::endl;

    for (size_t events_generated = 0; events_generated < m_config.max_events; ++events_generated) {
        // Update number of events needed per source
        if (!updateInputNEvents(data_sources_)) {
            std::cout << "Reached end of input data, stopping at " << events_generated
                      << " timeslices" << std::endl;
            break;
        }

        // Prepare for new timeslice
        output_handler_->prepareTimeslice();

        // Merge events from all sources
        output_handler_->mergeEvents(
            data_sources_, events_generated, m_config.time_slice_duration,
            m_config.bunch_crossing_period, gen);

        // Write the timeslice
        output_handler_->writeTimeslice();

        if (events_generated % 10 == 0) {
            std::cout << "Processed " << events_generated << " timeslices..." << std::endl;
        }
    }

    // Finalize output
    output_handler_->finalize();

    // std::cout << "Generated " << events_generated << " timeslices" << std::endl;
    std::cout << "Output saved to: " << m_config.output_file << std::endl;
}

std::vector<std::unique_ptr<DataSource>> TimesliceMerger::initializeDataSources() {
    std::vector<std::unique_ptr<DataSource>> data_sources;
    data_sources.reserve(m_config.sources.size());

    // Create EDM4hep DataSource objects (currently only EDM4hep format supported)
    for (size_t source_idx = 0; source_idx < m_config.sources.size(); ++source_idx) {
        auto data_source = std::make_unique<EDM4hepDataSource>(m_config.sources[source_idx], source_idx);
        data_sources.push_back(std::move(data_source));
    }

    return data_sources;
}

bool TimesliceMerger::updateInputNEvents(std::vector<std::unique_ptr<DataSource>>& sources) {
    for (auto& data_source : sources) {
        const auto& config = data_source->getConfig();

        // Generate new number of events needed for this source
        if (config.already_merged) {
            // Already merged sources should only contribute 1 event (which is already a full timeslice)
            data_source->setEntriesNeeded(1);
        } else if (config.static_number_of_events) {
            data_source->setEntriesNeeded(config.static_events_per_timeslice);
        } else {
            // Use Poisson for this source
            float mean_freq = config.mean_event_frequency;
            std::poisson_distribution<> poisson_dist(m_config.time_slice_duration * mean_freq);
            size_t n = poisson_dist(gen);
            data_source->setEntriesNeeded(n);
        }

        // Check enough events are available in this source
        if (!data_source->hasMoreEntries()) {
            std::cout << "Not enough events available in source " << config.name << std::endl;
            return false;
        }
    }

    return true;
}