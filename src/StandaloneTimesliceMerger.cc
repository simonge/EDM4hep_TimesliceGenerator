#include "StandaloneTimesliceMerger.h"
#include "SourceReader.h"
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
        updateInputNEvents(inputs);
        auto merged_frame = createMergedTimeslice(inputs);
        writeOutput(writer, std::move(merged_frame));

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
        source_reader.initialize(source);

        if (source.hasInputFiles()) {
            source_reader.openFiles();

            // Check collections_to_merge are present in this source
            for (const auto& coll_name : collections_to_merge) {
                if (source_idx == 0) {
                    collections_to_read.push_back(coll_name);
                }
            }

            // If not merging particles, we need to ensure we have the right hit collections
            if (!m_config.merge_particles) {
                // Read the first frame to get collection names and types
                auto frame_data = source_reader.getReader().readEntry(source.getTreeName(), 0);
                podio::Frame frame(std::move(frame_data));
                auto available_collections = frame.getAvailableCollections();

                // Check hit collections are present in this source
                for (const auto& coll_name : available_collections) {
                    const auto* coll = frame.get(coll_name);
                    auto coll_type = std::string(coll->getValueTypeName());
                    // check if coll type is in the hit collection types
                    auto it = std::find(hit_collection_types.begin(), hit_collection_types.end(), coll_type);
                    if (it != hit_collection_types.end()) {
                        source_reader.addCollectionToRead(coll_name, coll_type);
                        if (source_idx == 0) {
                            collections_to_read.push_back(coll_name);
                        }
                    }
                }
            }

            // Validate collections for this source
            source_reader.validateCollections(collections_to_merge);

            // Ensure the collections from this source match those from the first source
            if (source_idx > 0) {
                for (const auto& coll_name : collections_to_read) {
                    const auto& reader_collections = source_reader.getCollectionNamesToRead();
                    if (std::find(reader_collections.begin(), reader_collections.end(), coll_name) == reader_collections.end()) {
                        throw std::runtime_error("ERROR: Collection '" + coll_name + "' not found in source " + std::to_string(source_idx));
                    }
                }
            }
        }
        ++source_idx;
    }

    return source_readers;
}


// Update number of events needed per source for next timeslice
void StandaloneTimesliceMerger::updateInputNEvents(std::vector<SourceReader>& inputs) {

    for (auto& source_reader : inputs) {
        const auto& config = source_reader.getConfig();
        // Generate new number of events needed for this source
        if (config.isAlreadyMerged()) {
            // Already merged sources should only contribute 1 event (which is already a full timeslice)
            source_reader.setEntriesNeeded(1);
        } else if (config.useStaticNumberOfEvents()) {
            source_reader.setEntriesNeeded(config.getStaticEventsPerTimeslice());
        } else {
            // Use Poisson for this source
            float mean_freq = config.getMeanEventFrequency();
            std::poisson_distribution<> poisson_dist(m_config.time_slice_duration * mean_freq);
            size_t n = poisson_dist(gen);
            source_reader.setEntriesNeeded((n == 0) ? 1 : n);
        }
        
        // Check enough events are available in this source
        if (!source_reader.canReadRequiredEntries()) {
            throw std::runtime_error("ERROR: Not enough events available in source " + config.getName());
        }
    }

}



std::unique_ptr<podio::Frame> StandaloneTimesliceMerger::createMergedTimeslice(std::vector<SourceReader>& inputs) {
           
    auto output_frame = std::make_unique<podio::Frame>();
    
    edm4hep::MCParticleCollection timeslice_particles_out;
    edm4hep::EventHeaderCollection timeslice_info_out;
    edm4hep::EventHeaderCollection sub_event_headers_out;
    std::unordered_map<std::string, edm4hep::SimTrackerHitCollection> timeslice_tracker_hits_out;
    std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection> timeslice_calorimeter_hits_out;
    std::unordered_map<std::string, edm4hep::CaloHitContributionCollection> timeslice_calo_contributions_out;
    
    // Get collection names from first frame
    if (!inputs.empty()) {
        auto tracker_collections = getCollectionNames(inputs[0], "edm4hep::SimTrackerHit");
        auto calo_collections = getCollectionNames(inputs[0], "edm4hep::SimCalorimeterHit");



        for (const auto& name : tracker_collections) {
            timeslice_tracker_hits_out[name] = edm4hep::SimTrackerHitCollection();
        }
        for (const auto& name : calo_collections) {
            timeslice_calorimeter_hits_out[name] = edm4hep::SimCalorimeterHitCollection();
            timeslice_calo_contributions_out[name] = edm4hep::CaloHitContributionCollection();
        }
    } else {
        throw std::runtime_error("ERROR: No input sources available to create timeslice.");
    }   

    int totalEventsConsumed = 0;

    // Loop over sources and read needed events, filling the frame
    for(auto& source: inputs) {

        const auto& config = source.getConfig();
        int sourceEventsConsumed = 0;

        for (size_t i = 0; i < source.getEntriesNeeded(); ++i) {

            auto frame_data = source.getReader().readEntry(config.getTreeName(), source.getCurrentEntryIndex());
            auto frame      = std::make_unique<podio::Frame>(std::move(frame_data));

            mergeCollections(frame, config, timeslice_particles_out, sub_event_headers_out, timeslice_tracker_hits_out, timeslice_calorimeter_hits_out, timeslice_calo_contributions_out);

            source.advanceEntry();
            sourceEventsConsumed++;
            totalEventsConsumed++;
        }

        std::cout << "Merged " << sourceEventsConsumed << " events, totalling " << source.getCurrentEntryIndex() << " from source " << config.getName() << std::endl;

    }

    // Create main timeslice header
    auto header = edm4hep::MutableEventHeader();
    header.setEventNumber(events_generated);
    header.setRunNumber(0);
    header.setTimeStamp(events_generated);
    timeslice_info_out.push_back(header);

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
    const std::unique_ptr<podio::Frame>& frame,
    const SourceConfig& sourceConfig,
    edm4hep::MCParticleCollection& out_particles,
    edm4hep::EventHeaderCollection& out_sub_event_headers,
    std::unordered_map<std::string, edm4hep::SimTrackerHitCollection>& out_tracker_hits,
    std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection>& out_calo_hits,
    std::unordered_map<std::string, edm4hep::CaloHitContributionCollection>& out_calo_contributions) {

    float time_offset = 0.0f;
    float distance = 0.0f; // Placeholder for distance-based time offset calculation
    if(!sourceConfig.isAlreadyMerged()) {
        if(sourceConfig.attachToBeam()) {
            // Get position of first particle with generatorStatus 1
            try {
                const auto& particles = frame->get<edm4hep::MCParticleCollection>("MCParticles");
                for (const auto& particle : particles) {
                    if (particle.getGeneratorStatus() == 1) {
                        auto pos = particle.getVertex();
                        // Distance is dot product of position vector relative to rotation around y of beam relative to z-axis
                        // Needs to be negative if beam is travelling in -z direction
                        distance = pos.z * std::cos(sourceConfig.getBeamAngle()) + pos.x * std::sin(sourceConfig.getBeamAngle());
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
    std::unordered_map<const edm4hep::MCParticle*, edm4hep::MCParticle> new_old_particle_map;

    // Process MCParticles
    try {
        const auto& particles = frame->get<edm4hep::MCParticleCollection>("MCParticles");
        for (const auto& particle : particles) {
            auto new_particle = particle.clone();
            new_particle.setTime(particle.getTime() + time_offset);
            // Use first source for generator status offset
            int status_offset = m_config.sources.empty() ? 0 : m_config.sources[0].getGeneratorStatusOffset();
            new_particle.setGeneratorStatus(particle.getGeneratorStatus() + status_offset);
            out_particles.push_back(new_particle);
            new_old_particle_map[&particle] = new_particle;
            // MCParticle Tree is probably not conserverd.
        }
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not process MCParticles: " << e.what() << std::endl;
    }

    // Process SubEventHeaders, creating them if they don't exist, otherwise updating existing ones 
    if (sourceConfig.isAlreadyMerged()) {
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
            auto new_hit = hit.clone();
            new_hit.setTime(hit.getTime() + time_offset);
            auto orig_particle = hit.getParticle();
            if (new_old_particle_map.find(&orig_particle) != new_old_particle_map.end()) {
                new_hit.setParticle(new_old_particle_map[&orig_particle]);
            }
            hit_collection.push_back(new_hit);
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
    }
    
}

float StandaloneTimesliceMerger::generateTimeOffset(const SourceConfig& sourceConfig, float distance) {

    std::uniform_real_distribution<float> uniform(0.0f, m_config.time_slice_duration);
    float time_offset = uniform(gen);
    
    // Apply bunch crossing if enabled
    if (sourceConfig.useBunchCrossing()) {
        time_offset = std::floor(time_offset / m_config.bunch_crossing_period) * m_config.bunch_crossing_period;
    }
    
    // Apply beam effects if enabled
    if (sourceConfig.attachToBeam()) {
        std::normal_distribution<float> gaussian(0.0f, sourceConfig.getBeamSpread());
        time_offset += gaussian(gen);
        time_offset += distance / sourceConfig.getBeamSpeed();
    }

    return time_offset;
}

std::vector<std::string> StandaloneTimesliceMerger::getCollectionNames(const SourceReader& reader, const std::string& type) {
    std::vector<std::string> names;

    // Iterate through the collection names and types to find matches with type
    const auto& collection_names = reader.getCollectionNamesToRead();
    const auto& collection_types = reader.getCollectionTypesToRead();
    
    for (size_t i = 0; i < collection_names.size(); ++i) {
        const auto& coll_name = collection_names[i];
        // Here we would need a way to get the type of the collection by name
        // Assuming we have a method in SourceReader to get types by name
        if (i < collection_types.size() && collection_types[i] == type) {
            names.push_back(coll_name);
        }
    }
    
    return names;
}

void StandaloneTimesliceMerger::writeOutput(std::unique_ptr<podio::ROOTWriter>& writer, std::unique_ptr<podio::Frame> frame) {
    writer->writeFrame(*frame, "events");
}