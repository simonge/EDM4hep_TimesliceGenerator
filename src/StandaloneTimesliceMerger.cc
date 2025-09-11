#include "StandaloneTimesliceMerger.h"
#include <iostream>
#include <algorithm>
#include <cmath>

StandaloneTimesliceMerger::StandaloneTimesliceMerger(const MergerConfig& config)
    : m_config(config), gen(rd()), events_generated(0) {
    
}

void StandaloneTimesliceMerger::run() {
    std::cout << "Starting timeslice merger..." << std::endl;
    std::cout << "Sources: " << m_config.sources.size() << std::endl;
    std::cout << "Output file: " << m_config.output_file << std::endl;
    std::cout << "Max events: " << m_config.max_events << std::endl;
    std::cout << "Timeslice duration: " << m_config.time_slice_duration << std::endl;
    
    auto writer = std::make_unique<podio::ROOTWriter>(m_config.output_file);

    auto inputs = initializeInputFiles();

    while (events_generated < m_config.max_events) {
        // Update number of events needed per source
        if (!updateInputNEvents(inputs)) break;
        createMergedTimeslice(inputs, writer);

        events_generated++;
    }
    
    writer->finish();
    std::cout << "Generated " << events_generated << " timeslices" << std::endl;
}

// Initialize input files and validate sources
std::vector<SourceReader> StandaloneTimesliceMerger::initializeInputFiles() {
    std::vector<SourceReader> source_readers(m_config.sources.size());
    
    // Initialize readers for each source using modern C++ indexed iteration
    size_t source_idx = 0;

    // vector of collections to read
    std::vector<std::string> collections_to_read;

    for (auto& source_reader : source_readers) {
        const auto& source = m_config.sources[source_idx];
        source_reader.config = &source;

        if (!source.input_files.empty()) {
            try {
                // Create zip reader
                source_reader.zip_reader = std::make_shared<PodioMutableCollectionReader>(source.input_files);

                auto tree_names = source_reader.zip_reader->getAvailableCategories();

                // Use treename from config, check it exists
                if (std::find(tree_names.begin(), tree_names.end(), source.tree_name) == tree_names.end()) {
                    throw std::runtime_error("ERROR: Tree name '" + source.tree_name + "' not found in file for source " + std::to_string(source_idx));
                }

                source_reader.total_entries = source_reader.zip_reader->getEntries(source.tree_name);
         
                // Read the first frame to get collection names and types
                auto frame = source_reader.zip_reader->readMutableEntry(source.tree_name, 0);
                auto available_collections = frame->getAvailableCollections();


                // Check collections_to_merge are present in this source
                for (const auto& coll_name : collections_to_merge) {
                    if (std::find(available_collections.begin(), available_collections.end(), coll_name) == available_collections.end()) {
                        throw std::runtime_error("ERROR: Collection '" + coll_name + "' not found in source " + std::to_string(source_idx));
                    } else {
                        source_reader.collection_names_to_read.push_back(coll_name);
                        const auto* coll = frame->get(coll_name);
                        source_reader.collection_types_to_read.push_back(std::string(coll->getValueTypeName()));
                        if(source_idx == 0) {
                            collections_to_read.push_back(coll_name);
                        }
                    }
                }

                // If not merging particles, we need to ensure we have the right hit collections
                if(!m_config.merge_particles) {
                    // Check hit collections are present in this source
                    for (const auto& coll_name : available_collections) {
                        const auto* coll = frame->get(coll_name);
                        auto coll_type = std::string(coll->getValueTypeName());
                        // check if coll type is in the hit collection types
                        auto it = std::find(hit_collection_types.begin(), hit_collection_types.end(), coll_type);
                        if (it != hit_collection_types.end()) {
                            source_reader.collection_names_to_read.push_back(coll_name);
                            source_reader.collection_types_to_read.push_back(coll_type);
                            if(source_idx == 0) {
                                collections_to_read.push_back(coll_name);
                            }                            
                        }
                    }
                }

                // If merging particles, we need to ensure we have the right collections
                // if(m_config.merge_particles) {
                //     for (const auto& part_type : particle_collection_types) {
                //         for (size_t i = 0; i < available_collections.size(); ++i) {
                //             if (i < source_reader.collection_types_to_read.size() && source_reader.collection_types_to_read[i] == part_type) {
                //                 const auto& coll_name = available_collections[i];
                //                 source_reader.collection_names_to_read.push_back(coll_name);
                //                 if(source_idx == 0) {
                //                     collections_to_read.push_back(coll_name);
                //                 }
                //             }
                //         }
                //     }
                // }

                // Ensure the collections from this source match those from the first source
                if (source_idx > 0) {
                    for (const auto& coll_name : collections_to_read) {
                        if (std::find(source_reader.collection_names_to_read.begin(), source_reader.collection_names_to_read.end(), coll_name) == source_reader.collection_names_to_read.end()) {
                            throw std::runtime_error("ERROR: Collection '" + coll_name + "' not found in source " + std::to_string(source_idx));
                        }
                    }
                }

                // Validate already_merged source has SubEventHeaders collection and regular source has no SubEventHeaders collection
                bool has_sub_event_headers = std::find(available_collections.begin(), available_collections.end(), "SubEventHeaders") != available_collections.end();

                if (source.already_merged && !has_sub_event_headers) {
                    throw std::runtime_error("ERROR: Tree name '" + source.tree_name + "' not found in file for source " + std::to_string(source_idx));
                } else if (!source.already_merged && has_sub_event_headers) {
                    throw std::runtime_error("ERROR: Source " + std::to_string(source_idx) + " is marked as not already_merged but has SubEventHeaders collection.");
                } else if( has_sub_event_headers ) {
                    source_reader.collection_names_to_read.push_back("SubEventHeaders");
                    source_reader.collection_types_to_read.push_back("edm4hep::EventHeader");
                }

            } catch (const std::exception& e) {
                throw std::runtime_error("ERROR: Could not open input files for source " + std::to_string(source_idx) + ": " + e.what());
            }
        }
        ++source_idx;
    }

    return source_readers;
}


// Update number of events needed per source for next timeslice
bool StandaloneTimesliceMerger::updateInputNEvents(std::vector<SourceReader>& inputs) {

    for (auto& source_reader : inputs) {
        const auto& config = source_reader.config;
        // Generate new number of events needed for this source
        if (config->already_merged) {
            // Already merged sources should only contribute 1 event (which is already a full timeslice)
            source_reader.entries_needed = 1;
        } else if (config->static_number_of_events) {
            source_reader.entries_needed = config->static_events_per_timeslice;
        } else {
            // Use Poisson for this source
            float mean_freq = config->mean_event_frequency;
            std::poisson_distribution<> poisson_dist(m_config.time_slice_duration * mean_freq);
            size_t n = poisson_dist(gen);
            source_reader.entries_needed = (n == 0) ? 1 : n;
        }
        
        // Check enough events are available in this source
        if ((source_reader.current_entry_index + source_reader.entries_needed) > source_reader.total_entries) {
            std::cout << "Not enough events available in source " << config->name << std::endl;
            return false;
        }
    }

    return true;

}



void StandaloneTimesliceMerger::createMergedTimeslice(std::vector<SourceReader>& inputs, std::unique_ptr<podio::ROOTWriter>& writer) {

    std::unique_ptr<podio::Frame> output_frame = std::make_unique<podio::Frame>();
    std::unique_ptr<podio::Frame> first_frame;
    
    bool first_event = true;
    
    edm4hep::MCParticleCollection* timeslice_particles_out;
    edm4hep::EventHeaderCollection* timeslice_info_out;
    edm4hep::EventHeaderCollection* sub_event_headers_out;
    std::unordered_map<std::string, edm4hep::SimTrackerHitCollection*> timeslice_tracker_hits_out;
    std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection*> timeslice_calorimeter_hits_out;
    std::unordered_map<std::string, edm4hep::CaloHitContributionCollection*> timeslice_calo_contributions_out;

    auto tracker_collections = getCollectionNames(inputs[0], "edm4hep::SimTrackerHit");
    auto calo_collections = getCollectionNames(inputs[0], "edm4hep::SimCalorimeterHit");
    auto calo_contributions = getCollectionNames(inputs[0], "edm4hep::CaloHitContribution");

    std::vector<std::string> collections_to_write = {"MCParticles", "EventHeader", "SubEventHeaders"};
    collections_to_write.insert(collections_to_write.end(), tracker_collections.begin(), tracker_collections.end());
    collections_to_write.insert(collections_to_write.end(), calo_collections.begin(), calo_collections.end());
    collections_to_write.insert(collections_to_write.end(), calo_contributions.begin(), calo_contributions.end());

    // // Get collection names from first frame
    // if (!inputs.empty()) {
    //     auto tracker_collections = getCollectionNames(inputs[0], "edm4hep::SimTrackerHit");
    //     auto calo_collections = getCollectionNames(inputs[0], "edm4hep::SimCalorimeterHit");



    //     for (const auto& name : tracker_collections) {
    //         timeslice_tracker_hits_out[name] = edm4hep::SimTrackerHitCollection();
    //     }
    //     for (const auto& name : calo_collections) {
    //         timeslice_calorimeter_hits_out[name] = edm4hep::SimCalorimeterHitCollection();
    //         timeslice_calo_contributions_out[name] = edm4hep::CaloHitContributionCollection();
    //     }
    // } else {
    //     throw std::runtime_error("ERROR: No input sources available to create timeslice.");
    // }   

    int totalEventsConsumed = 0;

    // Loop over sources and read needed events, filling the frame
    for(auto& source: inputs) {

        const auto& config = source.config;
        int sourceEventsConsumed = 0;

        for (size_t i = 0; i < source.entries_needed; ++i) {

            auto frame = source_reader.zip_reader->readMutableEntry(config->tree_name, source.current_entry_index);

            // If first event and already merged, take entire frame as starting point
            // std::cout << "Merging event " << source.current_entry_index << " from source " << config->name << std::endl;
            // std::cout << "  first_event: " << first_event << ", already_merged: " << config->already_merged << std::endl;
            if (first_event) {
                if (config->already_merged) {
                    // output_frame = std::make_unique<podio::Frame>(*frame);
                    first_frame = std::move(frame);
                    timeslice_particles_out = &PodioMutableCollectionReader::getMutableCollection<edm4hep::MCParticleCollection>(*first_frame, "MCParticles");
                    timeslice_info_out = &PodioMutableCollectionReader::getMutableCollection<edm4hep::EventHeaderCollection>(*first_frame, "EventHeader");
                    // if (output_frame->hasCollection("SubEventHeaders")) {
                        sub_event_headers_out = &PodioMutableCollectionReader::getMutableCollection<edm4hep::EventHeaderCollection>(*first_frame, "SubEventHeaders");
                    // }
                    for (const auto& name : tracker_collections) {
                        timeslice_tracker_hits_out[name] = &PodioMutableCollectionReader::getMutableCollection<edm4hep::SimTrackerHitCollection>(*first_frame, name);
                    }
                    for (const auto& name : calo_collections) {
                        timeslice_calorimeter_hits_out[name] = &PodioMutableCollectionReader::getMutableCollection<edm4hep::SimCalorimeterHitCollection>(*first_frame, name);
                        timeslice_calo_contributions_out[name] = &PodioMutableCollectionReader::getMutableCollection<edm4hep::CaloHitContributionCollection>(*first_frame, name + "Contributions");
                    }
                } else {
                    // Create new empty frame and collections
                    timeslice_particles_out = new edm4hep::MCParticleCollection();
                    timeslice_info_out = new edm4hep::EventHeaderCollection();
                    sub_event_headers_out = new edm4hep::EventHeaderCollection();
                    std::cout << "Creating new empty collections for non-merged source: " << config->name << std::endl;
                    for (const auto& name : tracker_collections) {
                        timeslice_tracker_hits_out[name] = new edm4hep::SimTrackerHitCollection();
                    }
                    for (const auto& name : calo_collections) {
                        timeslice_calorimeter_hits_out[name] = new edm4hep::SimCalorimeterHitCollection();
                        timeslice_calo_contributions_out[name] = new edm4hep::CaloHitContributionCollection();
                    }
                    mergeCollectionsWithZipping(frame, *config, *timeslice_particles_out, *sub_event_headers_out, timeslice_tracker_hits_out, timeslice_calorimeter_hits_out, timeslice_calo_contributions_out);                
                }
                first_event = false;
            } else {
                mergeCollectionsWithZipping(frame, *config, *timeslice_particles_out, *sub_event_headers_out, timeslice_tracker_hits_out, timeslice_calorimeter_hits_out, timeslice_calo_contributions_out);                
            }


            source.current_entry_index++;
            sourceEventsConsumed++;
            totalEventsConsumed++;
        }

        std::cout << "Merged " << sourceEventsConsumed << " events, totalling " << source.current_entry_index << " from source " << config->name << std::endl;

    }

    // Create main timeslice header
    auto header = edm4hep::MutableEventHeader();
    header.setEventNumber(events_generated);
    header.setRunNumber(0);
    header.setTimeStamp(events_generated);
    timeslice_info_out->push_back(header);

    // Insert collections into frame
    output_frame->put(std::move(*timeslice_particles_out), "MCParticles");
    output_frame->put(std::move(*timeslice_info_out), "EventHeader");
    output_frame->put(std::move(*sub_event_headers_out), "SubEventHeaders");

    for(auto& [collection_name, hit_collection] : timeslice_tracker_hits_out) {
        output_frame->put(std::move(*hit_collection), collection_name);
    }
    for (auto& [collection_name, hit_collection] : timeslice_calorimeter_hits_out) {
        output_frame->put(std::move(*hit_collection), collection_name);
    }
    for (auto& [collection_name, hit_collection] : timeslice_calo_contributions_out) {
        output_frame->put(std::move(*hit_collection), collection_name + "Contributions");
    }

    writer->writeFrame(*output_frame, "events", collections_to_write);

}

void StandaloneTimesliceMerger::mergeCollections(
    const std::unique_ptr<podio::Frame>& frame,
    const SourceConfig& sourceConfig,
    edm4hep::MCParticleCollection& out_particles,
    edm4hep::EventHeaderCollection& out_sub_event_headers,
    std::unordered_map<std::string, edm4hep::SimTrackerHitCollection*>& out_tracker_hits,
    std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection*>& out_calo_hits,
    std::unordered_map<std::string, edm4hep::CaloHitContributionCollection*>& out_calo_contributions) {

    float time_offset = 0.0f;
    float distance = 0.0f; // Placeholder for distance-based time offset calculation
    if(!sourceConfig.already_merged) {
        if(sourceConfig.attach_to_beam) {
            // Get position of first particle with generatorStatus 1
            try {
                const auto& particles = frame->get<edm4hep::MCParticleCollection>("MCParticles");
                for (const auto& particle : particles) {
                    if (particle.getGeneratorStatus() == 1) {
                        auto pos = particle.getVertex();
                        // Distance is dot product of position vector relative to rotation around y of beam relative to z-axis
                        // Needs to be negative if beam is travelling in -z direction
                        distance = pos.z * std::cos(sourceConfig.beam_angle) + pos.x * std::sin(sourceConfig.beam_angle);
                        break;
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "Warning: Could not access MCParticles for beam attachment: " << e.what() << std::endl;
            }
        }
        time_offset = generateTimeOffset(sourceConfig, distance);
    }

    // Keep track of starting index for MCParticles from this frame
    size_t mcparticle_index = out_particles.size();

    // Map from original particle handle -> cloned particle handle
    std::unordered_map<const edm4hep::MCParticle*, edm4hep::MutableMCParticle> new_old_particle_map;

    // Process MCParticles
    try {
        const auto& particles = frame->get<edm4hep::MCParticleCollection>("MCParticles");
        for (const auto& particle : particles) {
            edm4hep::MutableMCParticle new_particle;
            new_particle.setPDG(particle.getPDG());
            new_particle.setGeneratorStatus(particle.getGeneratorStatus() + (m_config.sources.empty() ? 0 : m_config.sources[0].generator_status_offset));
            new_particle.setCharge(particle.getCharge());
            new_particle.setMass(particle.getMass());
            new_particle.setMomentum(particle.getMomentum());
            new_particle.setVertex(particle.getVertex());
            new_particle.setTime(particle.getTime() + time_offset);
            new_particle.setColorFlow(particle.getColorFlow());
            new_particle.setSpin(particle.getSpin());
            new_particle.setSimulatorStatus(particle.getSimulatorStatus());
            out_particles.push_back(new_particle);
            new_old_particle_map[&particle] = new_particle;
        }
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not process MCParticles: " << e.what() << std::endl;
    }

    // Loop through new_old_particle_map updating the parent-child relationships of the new particles
    for (auto& [old_particle_ptr, new_particle] : new_old_particle_map) {
        const auto& old_parents = old_particle_ptr->getParents();
        for (const auto& old_parent : old_parents) {
            if (new_old_particle_map.find(&old_parent) != new_old_particle_map.end()) {
                new_particle.addToParents(new_old_particle_map[&old_parent]);
            }
        }
        const auto& old_children = old_particle_ptr->getDaughters();
        for (const auto& old_child : old_children) {
            if (new_old_particle_map.find(&old_child) != new_old_particle_map.end()) {
                new_particle.addToDaughters(new_old_particle_map[&old_child]);
            }
        }
    }

    // Process SubEventHeaders, creating them if they don't exist, otherwise updating existing ones 
    if (sourceConfig.already_merged) {
        const auto& existing_sub_headers = frame->get<edm4hep::EventHeaderCollection>("SubEventHeaders");
        for (const auto& existing_header : existing_sub_headers) {
            auto sub_header = existing_header.clone();
            sub_header.setEventNumber(out_sub_event_headers.size());
            // Update the "timestamp" to reflect the new MCParticle index offset
            sub_header.setTimeStamp(mcparticle_index + existing_header.getTimeStamp());
            out_sub_event_headers.push_back(sub_header);
        }
    } else {
        // Create new SubEventHeader for regular source
        auto sub_header = edm4hep::MutableEventHeader();
        sub_header.setEventNumber(out_sub_event_headers.size()); // sub-event number
        sub_header.setRunNumber(0);
        sub_header.setTimeStamp(mcparticle_index); // index of first MCParticle for this sub-event
        sub_header.setWeight(time_offset); // time offset
        // Get EventHeader and pass vector of weights if available
        try {
            const auto& event_headers = frame->get<edm4hep::EventHeaderCollection>("EventHeader");
            if (!event_headers.empty()) {
                const auto& event_header = event_headers[0];
                for(const auto& w : event_header.getWeights()) {
                    sub_header.addToWeights(w);
                }
            }
        } catch (...) {
            // If no EventHeader, proceed without weights
        }
        out_sub_event_headers.push_back(sub_header);
    }


    // Process tracker hits
    for (auto& [collection_name, hit_collection] : out_tracker_hits) {
        const auto& hits = frame->get<edm4hep::SimTrackerHitCollection>(collection_name);
        for(const auto& hit : hits) {
            edm4hep::MutableSimTrackerHit new_hit;
            new_hit.setCellID(hit.getCellID());
            new_hit.setEDep(hit.getEDep());
            new_hit.setPosition(hit.getPosition());
            new_hit.setMomentum(hit.getMomentum());
            new_hit.setPathLength(hit.getPathLength());
            new_hit.setQuality(hit.getQuality());
            new_hit.setTime(hit.getTime() + time_offset);
            auto orig_particle = hit.getParticle();
            auto it = new_old_particle_map.find(static_cast<const edm4hep::MCParticle*>(&orig_particle));
            if (it != new_old_particle_map.end()) {
                new_hit.setParticle(it->second);
            }
            hit_collection->push_back(new_hit);
        }
    }

    // Process calorimeter hits
    for (auto& [collection_name, hit_collection] : out_calo_hits) {
        const auto& hits = frame->get<edm4hep::SimCalorimeterHitCollection>(collection_name);
        for (const auto& hit : hits) {
            edm4hep::MutableSimCalorimeterHit new_hit;
            new_hit.setEnergy(hit.getEnergy());
            new_hit.setPosition(hit.getPosition());
            new_hit.setCellID(hit.getCellID());

            // Process contributions
            for (const auto& contrib : hit.getContributions()) {
                edm4hep::MutableCaloHitContribution new_contrib;
                new_contrib.setEnergy(contrib.getEnergy());
                new_contrib.setTime(contrib.getTime() + time_offset);
                new_contrib.setPDG(contrib.getPDG());
                new_contrib.setStepPosition(contrib.getStepPosition());
                // new_contrib.setStepLength(contrib.getStepLength());
                auto orig_particle = contrib.getParticle();
                auto it = new_old_particle_map.find(static_cast<const edm4hep::MCParticle*>(&orig_particle));
                if (it != new_old_particle_map.end()) {
                    new_contrib.setParticle(it->second);
                }
                out_calo_contributions[collection_name]->push_back(new_contrib);
                new_hit.addToContributions(new_contrib);
            }
            hit_collection->push_back(new_hit);
        }
    }
    
}

void StandaloneTimesliceMerger::mergeCollectionsWithZipping(
    const std::unique_ptr<podio::Frame>& frame,
    const SourceConfig& sourceConfig,
    edm4hep::MCParticleCollection& out_particles,
    edm4hep::EventHeaderCollection& out_sub_event_headers,
    std::unordered_map<std::string, edm4hep::SimTrackerHitCollection*>& out_tracker_hits,
    std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection*>& out_calo_hits,
    std::unordered_map<std::string, edm4hep::CaloHitContributionCollection*>& out_calo_contributions) {

    float time_offset = 0.0f;
    float distance = 0.0f;
    
    if(!sourceConfig.already_merged) {
        if(sourceConfig.attach_to_beam) {
            // Get position of first particle with generatorStatus 1
            try {
                const auto& particles = frame->get<edm4hep::MCParticleCollection>("MCParticles");
                for (const auto& particle : particles) {
                    if (particle.getGeneratorStatus() == 1) {
                        auto pos = particle.getVertex();
                        distance = pos.z * std::cos(sourceConfig.beam_angle) + pos.x * std::sin(sourceConfig.beam_angle);
                        break;
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "Warning: Could not access MCParticles for beam attachment: " << e.what() << std::endl;
            }
        }
        time_offset = generateTimeOffset(sourceConfig, distance);
    }

    // Use the new vectorized time offset functionality if needed
    if (time_offset != 0.0f) {
        PodioMutableCollectionReader::addTimeOffsetToFrame(*frame, time_offset);
    }
    
    // Use the frame directly since it now supports mutable access through helper methods

    // Keep track of starting index for MCParticles from this frame
    size_t mcparticle_index = out_particles.size();

    // Map from original particle handle -> cloned particle handle
    std::unordered_map<const edm4hep::MCParticle*, edm4hep::MutableMCParticle> new_old_particle_map;

    // Process MCParticles
    try {
        const auto& particles = frame->get<edm4hep::MCParticleCollection>("MCParticles");
        
        for (const auto& particle : particles) {
            edm4hep::MutableMCParticle new_particle;
            new_particle.setPDG(particle.getPDG());
            new_particle.setGeneratorStatus(particle.getGeneratorStatus() + sourceConfig.generator_status_offset);
            new_particle.setCharge(particle.getCharge());
            new_particle.setMass(particle.getMass());
            new_particle.setMomentum(particle.getMomentum());
            new_particle.setVertex(particle.getVertex());
            new_particle.setTime(particle.getTime()); // Time offset already applied if needed
            new_particle.setColorFlow(particle.getColorFlow());
            new_particle.setSpin(particle.getSpin());
            new_particle.setSimulatorStatus(particle.getSimulatorStatus());
            out_particles.push_back(new_particle);
            new_old_particle_map[&particle] = new_particle;
        }
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not process MCParticles: " << e.what() << std::endl;
    }

    // Fix parent-child relationships
    for (auto& [old_particle_ptr, new_particle] : new_old_particle_map) {
        const auto& old_parents = old_particle_ptr->getParents();
        for (const auto& old_parent : old_parents) {
            if (new_old_particle_map.find(&old_parent) != new_old_particle_map.end()) {
                new_particle.addToParents(new_old_particle_map[&old_parent]);
            }
        }
        const auto& old_children = old_particle_ptr->getDaughters();
        for (const auto& old_child : old_children) {
            if (new_old_particle_map.find(&old_child) != new_old_particle_map.end()) {
                new_particle.addToDaughters(new_old_particle_map[&old_child]);
            }
        }
    }

    // Process tracker hits efficiently
    for (auto& [collection_name, hit_collection] : out_tracker_hits) {
        try {
            const auto& hits = frame->get<edm4hep::SimTrackerHitCollection>(collection_name);
            for (const auto& hit : hits) {
                edm4hep::MutableSimTrackerHit new_hit;
                new_hit.setCellID(hit.getCellID());
                new_hit.setEDep(hit.getEDep());
                new_hit.setTime(hit.getTime()); // Time offset already applied if needed
                new_hit.setPathLength(hit.getPathLength());
                new_hit.setQuality(hit.getQuality());
                new_hit.setPosition(hit.getPosition());
                new_hit.setMomentum(hit.getMomentum());

                // Update particle reference
                const auto& original_particle = hit.getParticle();
                if (new_old_particle_map.find(&original_particle) != new_old_particle_map.end()) {
                    new_hit.setParticle(new_old_particle_map[&original_particle]);
                }
                hit_collection->push_back(new_hit);
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not process " << collection_name << ": " << e.what() << std::endl;
        }
    }

    // Process calorimeter hits and contributions efficiently
    for (auto& [collection_name, hit_collection] : out_calo_hits) {
        try {
            const auto& hits = frame->get<edm4hep::SimCalorimeterHitCollection>(collection_name);
            auto& contributions_collection = out_calo_contributions[collection_name];
            
            for (const auto& hit : hits) {
                edm4hep::MutableSimCalorimeterHit new_hit;
                new_hit.setCellID(hit.getCellID());
                new_hit.setEnergy(hit.getEnergy());
                new_hit.setPosition(hit.getPosition());

                // Process contributions
                const auto& original_contributions = hit.getContributions();
                for (const auto& contrib : original_contributions) {
                    edm4hep::MutableCaloHitContribution new_contrib;
                    new_contrib.setPDG(contrib.getPDG());
                    new_contrib.setEnergy(contrib.getEnergy());
                    new_contrib.setTime(contrib.getTime()); // Time offset already applied if needed
                    new_contrib.setStepPosition(contrib.getStepPosition());

                    // Update particle reference
                    const auto& original_particle = contrib.getParticle();
                    if (new_old_particle_map.find(&original_particle) != new_old_particle_map.end()) {
                        new_contrib.setParticle(new_old_particle_map[&original_particle]);
                    }
                    contributions_collection->push_back(new_contrib);
                    new_hit.addToContributions(new_contrib);
                }
                hit_collection->push_back(new_hit);
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not process " << collection_name << ": " << e.what() << std::endl;
        }
    }

    // Process SubEventHeaders
    if (sourceConfig.already_merged) {
        try {
            const auto& existing_sub_headers = frame->get<edm4hep::EventHeaderCollection>("SubEventHeaders");
            for (const auto& existing_header : existing_sub_headers) {
                auto sub_header = existing_header.clone();
                sub_header.setEventNumber(out_sub_event_headers.size());
                sub_header.setTimeStamp(mcparticle_index + existing_header.getTimeStamp());
                out_sub_event_headers.push_back(sub_header);
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not process SubEventHeaders: " << e.what() << std::endl;
        }
    } else {
        // Create new SubEventHeader for regular source
        auto sub_header = edm4hep::MutableEventHeader();
        sub_header.setEventNumber(out_sub_event_headers.size());
        sub_header.setRunNumber(0);
        sub_header.setTimeStamp(mcparticle_index);
        sub_header.setWeight(time_offset);
        
        try {
            const auto& event_header = frame->get<edm4hep::EventHeaderCollection>("EventHeader");
            if (!event_header.empty()) {
                const auto& header = event_header[0];
                auto weights = header.getWeights();
                if (!weights.empty()) {
                    std::vector<float> new_weights(weights.begin(), weights.end());
                    sub_header.setWeights(new_weights);
                }
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not access EventHeader weights: " << e.what() << std::endl;
        }
        
        out_sub_event_headers.push_back(sub_header);
    }
}

float StandaloneTimesliceMerger::generateTimeOffset(SourceConfig sourceConfig, float distance) {

    std::uniform_real_distribution<float> uniform(0.0f, m_config.time_slice_duration);
    float time_offset = uniform(gen);
    
    // Apply bunch crossing if enabled
    if (sourceConfig.use_bunch_crossing) {
        time_offset = std::floor(time_offset / m_config.bunch_crossing_period) * m_config.bunch_crossing_period;
    }
    
    // Apply beam effects if enabled
    if (sourceConfig.attach_to_beam) {
        std::normal_distribution<float> gaussian(0.0f, sourceConfig.beam_spread);
        time_offset += gaussian(gen);
        time_offset += distance / sourceConfig.beam_speed;
    }

    return time_offset;
}

std::vector<std::string> StandaloneTimesliceMerger::getCollectionNames(const SourceReader& reader, const std::string& type) {
    std::vector<std::string> names;

    // Iterate through the collection names and types to find matches with type
    for (size_t i = 0; i < reader.collection_names_to_read.size(); ++i) {
        const auto& coll_name = reader.collection_names_to_read[i];
        // Here we would need a way to get the type of the collection by name
        // Assuming we have a method in SourceReader to get types by name
        if (i < reader.collection_types_to_read.size() && reader.collection_types_to_read[i] == type) {
            names.push_back(coll_name);
        }
    }
    
    return names;
}
