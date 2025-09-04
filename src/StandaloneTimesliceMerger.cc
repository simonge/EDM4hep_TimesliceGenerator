#include "StandaloneTimesliceMerger.h"
#include <iostream>
#include <algorithm>
#include <cmath>

StandaloneTimesliceMerger::StandaloneTimesliceMerger(const StandaloneMergerConfig& config)
    : m_config(config), gen(rd()), events_generated(0) {
    setupRandomGenerators();
    
    if(m_config.static_number_of_events) {
        events_needed = m_config.static_events_per_timeslice;
    } else {
        events_needed = poisson(gen);
    }
}

void StandaloneTimesliceMerger::setupRandomGenerators() {
    uniform = std::uniform_real_distribution<float>(0.0f, m_config.time_slice_duration);
    poisson = std::poisson_distribution<>(m_config.time_slice_duration * m_config.mean_event_frequency);
    gaussian = std::normal_distribution<>(0.0f, m_config.beam_spread);
}

void StandaloneTimesliceMerger::run() {
    std::cout << "Starting standalone timeslice merger..." << std::endl;
    std::cout << "Input files: " << m_config.input_files.size() << std::endl;
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
    
    for (const auto& input_file : m_config.input_files) {
        std::cout << "Processing input file: " << input_file << std::endl;
        
        try {
            podio::ROOTReader reader;
            reader.openFile(input_file);
            
            auto tree_names = reader.getAvailableCategories();
            std::string tree_name = "events"; // Default
            for (const auto& name : tree_names) {
                if (name.find("event") != std::string::npos) {
                    tree_name = name;
                    break;
                }
            }
            
            size_t num_entries = reader.getEntries(tree_name);
            std::cout << "Found " << num_entries << " entries in tree: " << tree_name << std::endl;
            
            if (num_entries == 0) {
                std::cout << "Warning: No entries found in " << input_file << ", skipping..." << std::endl;
                continue;
            }
        
        for (size_t i = 0; i < num_entries && events_generated < m_config.max_events; ++i) {
            try {
                auto frame_data = reader.readEntry(tree_name, i);
                auto frame = std::make_unique<podio::Frame>(std::move(frame_data));
                
                accumulated_frames.push_back(std::move(frame));
                
                if (accumulated_frames.size() >= events_needed) {
                    auto merged_frame = createMergedTimeslice();
                    writeOutput(writer, std::move(merged_frame));
                    
                    accumulated_frames.clear();
                    events_generated++;
                    
                    if (!m_config.static_number_of_events) {
                        events_needed = poisson(gen);
                        if (events_needed == 0) events_needed = 1; // Ensure at least 1 event
                    }
                    
                    if (events_generated >= m_config.max_events) {
                        break;
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "Warning: Failed to read entry " << i << " from " << input_file 
                          << ": " << e.what() << std::endl;
                continue;
            }
        }
        } catch (const std::exception& e) {
            std::cout << "Error: Failed to process file " << input_file << ": " << e.what() << std::endl;
            continue;
        }
    }
    
    // Process any remaining accumulated frames
    if (!accumulated_frames.empty() && events_generated < m_config.max_events) {
        auto merged_frame = createMergedTimeslice();
        writeOutput(writer, std::move(merged_frame));
        events_generated++;
    }
}

std::unique_ptr<podio::Frame> StandaloneTimesliceMerger::createMergedTimeslice() {
    auto output_frame = std::make_unique<podio::Frame>();
    
    edm4hep::MCParticleCollection timeslice_particles_out;
    edm4hep::EventHeaderCollection timeslice_info_out;
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
    
    mergeCollections(accumulated_frames, timeslice_particles_out, 
                    timeslice_tracker_hits_out, timeslice_calorimeter_hits_out, 
                    timeslice_calo_contributions_out);
    
    // Create header
    auto header = edm4hep::MutableEventHeader();
    header.setEventNumber(events_generated);
    header.setRunNumber(0);
    header.setTimeStamp(events_generated);
    timeslice_info_out.push_back(header);
    
    // Insert collections into frame
    output_frame->put(std::move(timeslice_particles_out), "MCParticles");
    output_frame->put(std::move(timeslice_info_out), "info");
    
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
    
    for (const auto& frame : frames) {
        float time_offset = generateTimeOffset();
        
        // Map from original particle handle -> cloned particle handle
        std::unordered_map<const edm4hep::MCParticle*, edm4hep::MCParticle> new_old_particle_map;
        
        // Process MCParticles
        try {
            const auto& particles = frame->get<edm4hep::MCParticleCollection>("MCParticles");
            
            for (const auto& particle : particles) {
                auto new_particle = particle.clone();
                new_particle.setTime(particle.getTime() + time_offset);
                new_particle.setGeneratorStatus(particle.getGeneratorStatus() + m_config.generator_status_offset);
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
    float time_offset = uniform(gen);
    
    // Apply bunch crossing if enabled
    if (m_config.use_bunch_crossing) {
        time_offset = std::floor(time_offset / m_config.bunch_crossing_period) * m_config.bunch_crossing_period;
    }
    
    // Apply beam effects if enabled
    if (m_config.attach_to_beam) {
        time_offset += gaussian(gen);
        // TODO: Add position-based time offset calculation
    }
    
    return time_offset;
}

std::vector<std::string> StandaloneTimesliceMerger::getCollectionNames(const podio::Frame& frame, const std::string& type) {
    std::vector<std::string> names;
    
    for (const auto& [name, coll_ptr] : frame.getAvailableCollections()) {
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
    writer->writeFrame(*frame, "timeslices");
}