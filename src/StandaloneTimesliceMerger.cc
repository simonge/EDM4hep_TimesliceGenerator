#include "StandaloneTimesliceMerger.h"
#include <iostream>
#include <algorithm>
#include <cmath>

StandaloneTimesliceMerger::StandaloneTimesliceMerger(const MergerConfig& config)
    : m_config(config), gen(rd()), events_generated(0), inputEventsConsumed(0) {
    setupRandomGenerators();
    
    // Initialize per-source event tracking
    eventsConsumedPerSource.resize(m_config.sources.size(), 0);
    
}

void StandaloneTimesliceMerger::setupRandomGenerators() {
    // Use first source for frequency if available
    // Use first source for beam spread if available
    float beam_spread = m_config.sources.empty() ? 0.0f : m_config.sources[0].beam_spread;
    gaussian = std::normal_distribution<>(0.0f, beam_spread);
}

void StandaloneTimesliceMerger::run() {
    std::cout << "Starting timeslice merger..." << std::endl;
    std::cout << "Sources: " << m_config.sources.size() << std::endl;
    std::cout << "Output file: " << m_config.output_file << std::endl;
    std::cout << "Max events: " << m_config.max_events << std::endl;
    std::cout << "Timeslice duration: " << m_config.time_slice_duration << std::endl;
    
    auto writer = std::make_unique<podio::ROOTWriter>(m_config.output_file);
    
    processInputFiles();
    
    writer->finish();
    std::cout << "Generated " << events_generated << " timeslices" << std::endl;
}

void StandaloneTimesliceMerger::processInputFiles() {
    auto writer = std::make_unique<podio::ROOTWriter>(m_config.output_file);
    
    // Setup readers for each source
    struct SourceReader {
        podio::ROOTReader reader;
        size_t current_file_index = 0;
        size_t current_entry_index = 0;
        size_t total_entries = 0;
        std::string tree_name = "events";
        bool finished = false;
        const SourceConfig* config;
    };
    
    std::vector<SourceReader> source_readers(m_config.sources.size());
    
    // Initialize readers for each source
    for (size_t source_idx = 0; source_idx < m_config.sources.size(); ++source_idx) {
        const auto& source = m_config.sources[source_idx];
        source_readers[source_idx].config = &source;
        
        if (!source.input_files.empty()) {
            try {
                source_readers[source_idx].reader.openFile(source.input_files[0]);
                
                auto tree_names = source_readers[source_idx].reader.getAvailableCategories();
                for (const auto& name : tree_names) {
                    if (name.find("event") != std::string::npos) {
                        source_readers[source_idx].tree_name = name;
                        break;
                    }
                }
                
                source_readers[source_idx].total_entries = source_readers[source_idx].reader.getEntries(source_readers[source_idx].tree_name);
                
                // Validate already_merged sources
                if (source.already_merged) {
                    // Check if this source actually has SubEventHeaders
                    try {
                        auto frame_data = source_readers[source_idx].reader.readEntry(source_readers[source_idx].tree_name, 0);
                        auto test_frame = std::make_unique<podio::Frame>(std::move(frame_data));
                        const auto& sub_headers = test_frame->get<edm4hep::EventHeaderCollection>("SubEventHeaders");
                        std::cout << "Source " << source_idx << ": Verified as already-merged with " 
                                  << sub_headers.size() << " SubEventHeaders" << std::endl;
                    } catch (const std::exception& e) {
                        std::cout << "ERROR: Source " << source_idx << " is marked as already_merged but has no SubEventHeaders. " 
                                  << "Set already_merged=false or provide a properly merged file." << std::endl;
                        std::exit(1);
                    }
                }
                
                std::cout << "Source " << source_idx << ": Opened " << source.input_files[0] 
                          << " with " << source_readers[source_idx].total_entries << " entries" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: Failed to open first file for source " << source_idx << ": " << e.what() << std::endl;
                source_readers[source_idx].finished = true;
            }
        } else {
            source_readers[source_idx].finished = true;
        }
    }
    
    // Track per-source events needed for each timeslice
    std::vector<size_t> events_needed_per_source(m_config.sources.size(), 1);
    std::vector<size_t> events_read_this_slice(m_config.sources.size(), 0);
    while (events_generated < m_config.max_events) {
        bool any_source_active = false;
        accumulated_frames.clear();
        events_read_this_slice.assign(m_config.sources.size(), 0);

        // Determine events needed for each source for this timeslice
        for (size_t source_idx = 0; source_idx < m_config.sources.size(); ++source_idx) {
            const auto& source = m_config.sources[source_idx];
            if (source.already_merged) {
                // Already merged sources should only contribute 1 event (which is already a full timeslice)
                events_needed_per_source[source_idx] = 1;
            } else if (source.static_number_of_events) {
                events_needed_per_source[source_idx] = source.static_events_per_timeslice;
            } else {
                // Use Poisson for this source
                float mean_freq = source.mean_event_frequency;
                std::poisson_distribution<> poisson_dist(m_config.time_slice_duration * mean_freq);
                size_t n = poisson_dist(gen);
                events_needed_per_source[source_idx] = (n == 0) ? 1 : n;
            }
        }

        // For each source, read the required number of events
        bool quota_failed = false;
        for (size_t source_idx = 0; source_idx < source_readers.size(); ++source_idx) {
            auto& source_reader = source_readers[source_idx];
            if (source_reader.finished) continue;
            any_source_active = true;
            size_t events_to_read = events_needed_per_source[source_idx];
            const auto& source = *source_reader.config;

            size_t events_available = 0;
            // Calculate available events in all remaining files for this source
            if (source_reader.current_file_index < source.input_files.size()) {
                // Current file
                events_available += source_reader.total_entries - source_reader.current_entry_index;
                // Remaining files
                for (size_t f = source_reader.current_file_index + 1; f < source.input_files.size(); ++f) {
                    podio::ROOTReader temp_reader;
                    temp_reader.openFile(source.input_files[f]);
                    auto entries = temp_reader.getEntries(source_reader.tree_name);
                    events_available += entries;
                }
            }
            if (events_available < events_to_read) {
                std::cout << "Source " << source_idx << " cannot provide its full quota of events (needed: "
                          << events_to_read << ", available: " << events_available << "). Exiting gracefully." << std::endl;
                std::exit(0);
            }

            for (size_t e = 0; e < events_to_read; ++e) {
                // Check if we need to move to next file
                if (source_reader.current_entry_index >= source_reader.total_entries) {
                    source_reader.current_file_index++;
                    if (source_reader.current_file_index >= source.input_files.size()) {
                        source_reader.finished = true;
                        break;
                    }
                    // Open next file
                    source_reader.reader.openFile(source.input_files[source_reader.current_file_index]);
                    source_reader.total_entries = source_reader.reader.getEntries(source_reader.tree_name);
                    source_reader.current_entry_index = 0;
                    std::cout << "Source " << source_idx << ": Opened " << source.input_files[source_reader.current_file_index]
                              << " with " << source_reader.total_entries << " entries" << std::endl;
                }
                if (source_reader.current_entry_index < source_reader.total_entries) {
                    auto frame_data = source_reader.reader.readEntry(source_reader.tree_name, source_reader.current_entry_index);
                    auto frame = std::make_unique<podio::Frame>(std::move(frame_data));
                    inputEventsConsumed++;
                    eventsConsumedPerSource[source_idx]++;
                    events_read_this_slice[source_idx]++;
                    source_reader.current_entry_index++;
                    accumulated_frames.push_back(std::move(frame));
                } else {
                    source_reader.finished = true;
                    break;
                }
            }
        }

        if (!any_source_active) {
            std::cout << "All sources finished" << std::endl;
            break;
        }

        // Write timeslice if any events were read
        if (!accumulated_frames.empty()) {
            auto merged_frame = createMergedTimeslice();
            writeOutput(writer, std::move(merged_frame));
            events_generated++;
            std::cout << "[StandaloneTimesliceMerger] Total events consumed: " << inputEventsConsumed
                      << ", Output timeslices made: " << events_generated << std::endl;
            std::cout << "  Events per source (this slice): ";
            for (size_t i = 0; i < events_read_this_slice.size(); ++i) {
                std::cout << "S" << i << "=" << events_read_this_slice[i] << " ";
            }
            std::cout << std::endl;
            std::cout << "  Cumulative events per source: ";
            for (size_t i = 0; i < eventsConsumedPerSource.size(); ++i) {
                std::cout << "S" << i << "=" << eventsConsumedPerSource[i] << " ";
            }
            std::cout << std::endl;
        }
    }
    
    // Process any remaining accumulated frames
    if (!accumulated_frames.empty() && events_generated < m_config.max_events) {
        auto merged_frame = createMergedTimeslice();
        writeOutput(writer, std::move(merged_frame));
        events_generated++;
        
        std::cout << "[StandaloneTimesliceMerger] Final - Total events consumed: " << inputEventsConsumed 
                  << ", Output timeslices made: " << events_generated << std::endl;
        std::cout << "  Events per source: ";
        for (size_t i = 0; i < eventsConsumedPerSource.size(); ++i) {
            std::cout << "S" << i << "=" << eventsConsumedPerSource[i] << " ";
        }
        std::cout << std::endl;
    }
}std::unique_ptr<podio::Frame> StandaloneTimesliceMerger::createMergedTimeslice() {
    auto output_frame = std::make_unique<podio::Frame>();
    
    edm4hep::MCParticleCollection timeslice_particles_out;
    edm4hep::EventHeaderCollection timeslice_info_out;
    edm4hep::EventHeaderCollection sub_event_headers_out;
    std::unordered_map<std::string, edm4hep::SimTrackerHitCollection> timeslice_tracker_hits_out;
    std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection> timeslice_calorimeter_hits_out;
    std::unordered_map<std::string, edm4hep::CaloHitContributionCollection> timeslice_calo_contributions_out;
    
    // Get collection names from first frame
    if (!accumulated_frames.empty()) {
        auto tracker_collections = getCollectionNames(*accumulated_frames[0], "edm4hep::SimTrackerHit");
        auto calo_collections = getCollectionNames(*accumulated_frames[0], "edm4hep::SimCalorimeterHit");
        
        for (const auto& name : tracker_collections) {
            timeslice_tracker_hits_out[name] = edm4hep::SimTrackerHitCollection();
        }
        for (const auto& name : calo_collections) {
            timeslice_calorimeter_hits_out[name] = edm4hep::SimCalorimeterHitCollection();
            timeslice_calo_contributions_out[name] = edm4hep::CaloHitContributionCollection();
        }
    }
    
    // Track first MCParticle index for each sub-event
    size_t mcparticle_index = 0;
    std::vector<float> time_offsets;
    // Merge collections and collect time offsets
    for (size_t i = 0; i < accumulated_frames.size(); ++i) {
        float time_offset = generateTimeOffset();
        time_offsets.push_back(time_offset);
    }
    mergeCollections(accumulated_frames, timeslice_particles_out, 
                    timeslice_tracker_hits_out, timeslice_calorimeter_hits_out, 
                    timeslice_calo_contributions_out);

    // Create main timeslice header
    auto header = edm4hep::MutableEventHeader();
    header.setEventNumber(events_generated);
    header.setRunNumber(0);
    header.setTimeStamp(events_generated);
    timeslice_info_out.push_back(header);

    // Create SubEventHeaders
    mcparticle_index = 0;
    for (size_t i = 0; i < accumulated_frames.size(); ++i) {
        const auto& frame = accumulated_frames[i];
        
        // Check if this frame already has SubEventHeaders (already merged source)
        bool has_sub_headers = false;
        try {
            const auto& existing_sub_headers = frame->get<edm4hep::EventHeaderCollection>("SubEventHeaders");
            has_sub_headers = !existing_sub_headers.empty();
        } catch (...) {
            has_sub_headers = false;
        }
        
        if (has_sub_headers) {
            // Copy existing SubEventHeaders and update MCParticle indices
            try {
                const auto& existing_sub_headers = frame->get<edm4hep::EventHeaderCollection>("SubEventHeaders");
                for (const auto& existing_header : existing_sub_headers) {
                    auto sub_header = existing_header.clone();
                    // Update the timestamp to reflect the new MCParticle index offset
                    sub_header.setTimeStamp(mcparticle_index + existing_header.getTimeStamp());
                    sub_event_headers_out.push_back(sub_header);
                }
                // Count MCParticles in this already-merged frame
                try {
                    const auto& particles = frame->get<edm4hep::MCParticleCollection>("MCParticles");
                    mcparticle_index += particles.size();
                } catch (...) {
                    // If no MCParticles, index remains unchanged
                }
            } catch (const std::exception& e) {
                std::cout << "Warning: Could not process existing SubEventHeaders: " << e.what() << std::endl;
            }
        } else {
            // Create new SubEventHeader for regular source
            auto sub_header = edm4hep::MutableEventHeader();
            sub_header.setEventNumber(i); // sub-event number
            sub_header.setRunNumber(0);
            sub_header.setTimeStamp(mcparticle_index); // index of first MCParticle for this sub-event
            sub_header.setWeight(time_offsets[i]); // time offset
            // Count MCParticles in this frame
            try {
                const auto& particles = frame->get<edm4hep::MCParticleCollection>("MCParticles");
                mcparticle_index += particles.size();
            } catch (...) {
                // If no MCParticles, index remains unchanged
            }
            sub_event_headers_out.push_back(sub_header);
        }
    }

    // Insert collections into frame
    output_frame->put(std::move(timeslice_particles_out), "MCParticles");
    output_frame->put(std::move(timeslice_info_out), "EventHeader");
    output_frame->put(std::move(sub_event_headers_out), "SubEventHeaders");

    for(auto& [collection_name, hit_collection] : timeslice_tracker_hits_out) {
        output_frame->put(std::move(hit_collection), collection_name);
    }
    for (auto& [collection_name, hit_collection] : timeslice_calorimeter_hits_out) {
        output_frame->put(std::move(hit_collection), collection_name);
    }
    for (auto& [collection_name, hit_collection] : timeslice_calo_contributions_out) {
        output_frame->put(std::move(hit_collection), collection_name + "Contributions");
    }

    return output_frame;
}

void StandaloneTimesliceMerger::mergeCollections(
    const std::vector<std::unique_ptr<podio::Frame>>& frames,
    edm4hep::MCParticleCollection& out_particles,
    std::unordered_map<std::string, edm4hep::SimTrackerHitCollection>& out_tracker_hits,
    std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection>& out_calo_hits,
    std::unordered_map<std::string, edm4hep::CaloHitContributionCollection>& out_calo_contributions) {
    
    for (size_t frame_idx = 0; frame_idx < frames.size(); ++frame_idx) {
        const auto& frame = frames[frame_idx];
        float time_offset = 0.0f;
        // Check if this frame is from an already merged source
        bool already_merged = false;
        try {
            const auto& sub_headers = frame->get<edm4hep::EventHeaderCollection>("SubEventHeaders");
            already_merged = !sub_headers.empty();
        } catch (...) {
            already_merged = false;
        }
        if (!already_merged) {
            time_offset = generateTimeOffset();
        }

        // Map from original particle handle -> cloned particle handle
        std::unordered_map<const edm4hep::MCParticle*, edm4hep::MCParticle> new_old_particle_map;

        // Process MCParticles
        try {
            const auto& particles = frame->get<edm4hep::MCParticleCollection>("MCParticles");
            for (const auto& particle : particles) {
                auto new_particle = particle.clone();
                new_particle.setTime(particle.getTime() + time_offset);
                // Use first source for generator status offset
                int status_offset = m_config.sources.empty() ? 0 : m_config.sources[0].generator_status_offset;
                new_particle.setGeneratorStatus(particle.getGeneratorStatus() + status_offset);
                out_particles.push_back(new_particle);
                new_old_particle_map[&particle] = new_particle;
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not process MCParticles: " << e.what() << std::endl;
        }

        // Process tracker hits
        for (auto& [collection_name, hit_collection] : out_tracker_hits) {
            try {
                const auto& hits = frame->get<edm4hep::SimTrackerHitCollection>(collection_name);
                for(const auto& hit : hits) {
                    auto new_hit = hit.clone();
                    new_hit.setTime(hit.getTime() + time_offset);
                    auto orig_particle = hit.getParticle();
                    if (new_old_particle_map.find(&orig_particle) != new_old_particle_map.end()) {
                        new_hit.setParticle(new_old_particle_map[&orig_particle]);
                    }
                    hit_collection.push_back(new_hit);
                }
            } catch (const std::exception& e) {
                // Collection may not exist in this frame - that's okay
            }
        }

        // Process calorimeter hits
        for (auto& [collection_name, hit_collection] : out_calo_hits) {
            try {
                const auto& hits = frame->get<edm4hep::SimCalorimeterHitCollection>(collection_name);
                for (const auto& hit : hits) {
                    edm4hep::MutableSimCalorimeterHit new_hit;
                    new_hit.setEnergy(hit.getEnergy());
                    new_hit.setPosition(hit.getPosition());
                    new_hit.setCellID(hit.getCellID());

                    // Process contributions
                    for (const auto& contrib : hit.getContributions()) {
                        auto new_contrib = contrib.clone();
                        new_contrib.setTime(contrib.getTime() + time_offset);
                        auto orig_particle = contrib.getParticle();
                        if (new_old_particle_map.find(&orig_particle) != new_old_particle_map.end()) {
                            new_contrib.setParticle(new_old_particle_map[&orig_particle]);
                        }
                        out_calo_contributions[collection_name].push_back(new_contrib);
                        new_hit.addToContributions(new_contrib);
                    }
                    hit_collection.push_back(new_hit);
                }
            } catch (const std::exception& e) {
                // Collection may not exist in this frame - that's okay
            }
        }
    }
}

float StandaloneTimesliceMerger::generateTimeOffset() {

    std::uniform_real_distribution<float> uniform(0.0f, m_config.time_slice_duration);
    float time_offset = uniform(gen);
    
    if (!m_config.sources.empty()) {
        const auto& source = m_config.sources[0]; // Use first source for now
        
        // Apply bunch crossing if enabled
        if (source.use_bunch_crossing) {
            time_offset = std::floor(time_offset / m_config.bunch_crossing_period) * m_config.bunch_crossing_period;
        }
        
        // Apply beam effects if enabled
        if (source.attach_to_beam) {
            time_offset += gaussian(gen);
            // TODO: Add position-based time offset calculation
        }
    }
    
    return time_offset;
}

std::vector<std::string> StandaloneTimesliceMerger::getCollectionNames(const podio::Frame& frame, const std::string& type) {
    std::vector<std::string> names;
    
    for (const auto& name : frame.getAvailableCollections()) {
        try {
            const auto* coll = frame.get(name);
            if (coll && coll->getValueTypeName() == type) {
                names.push_back(name);
            }
        } catch (...) {
            // Skip collections we can't access
        }
    }
    
    return names;
}

void StandaloneTimesliceMerger::writeOutput(std::unique_ptr<podio::ROOTWriter>& writer, std::unique_ptr<podio::Frame> frame) {
    writer->writeFrame(*frame, "events");
}