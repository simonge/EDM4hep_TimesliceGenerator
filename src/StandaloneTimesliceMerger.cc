#include "StandaloneTimesliceMerger.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <podio/Frame.h>

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
                source_reader.mutable_reader = std::make_shared<MutableRootReader>(source.input_files);

                auto tree_names = source_reader.mutable_reader->getAvailableCategories();

                // Use treename from config, check it exists
                if (std::find(tree_names.begin(), tree_names.end(), source.tree_name) == tree_names.end()) {
                    throw std::runtime_error("ERROR: Tree name '" + source.tree_name + "' not found in file for source " + std::to_string(source_idx));
                }

                source_reader.total_entries = source_reader.mutable_reader->getEntries(source.tree_name);
         
                // Read the first frame to get collection names and types
                auto frame = source_reader.mutable_reader->readMutableEntry(source.tree_name, 0);
                auto available_collections = frame->getAvailableCollections();


                // Check collections_to_merge are present in this source
                for (const auto& coll_name : collections_to_merge) {
                    if (std::find(available_collections.begin(), available_collections.end(), coll_name) == available_collections.end()) {
                        throw std::runtime_error("ERROR: Collection '" + coll_name + "' not found in source " + std::to_string(source_idx));
                    } else {
                        source_reader.collection_names_to_read.push_back(coll_name);
                        auto coll_type = frame->getCollectionTypeName(coll_name);
                        source_reader.collection_types_to_read.push_back(coll_type);
                        if(source_idx == 0) {
                            collections_to_read.push_back(coll_name);
                        }
                    }
                }

                // If not merging particles, we need to ensure we have the right hit collections
                if(!m_config.merge_particles) {
                    // Check hit collections are present in this source
                    for (const auto& coll_name : available_collections) {
                        auto coll_type = frame->getCollectionTypeName(coll_name);
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
    auto output_frame = std::make_unique<MutableRootReader::MutableFrame>();

    auto tracker_collections = getCollectionNames(inputs[0], "edm4hep::SimTrackerHit");
    auto calo_collections = getCollectionNames(inputs[0], "edm4hep::SimCalorimeterHit");
    auto calo_contributions = getCollectionNames(inputs[0], "edm4hep::CaloHitContribution");

    std::vector<std::string> collections_to_write = {"MCParticles", "EventHeader", "SubEventHeaders"};
    collections_to_write.insert(collections_to_write.end(), tracker_collections.begin(), tracker_collections.end());
    collections_to_write.insert(collections_to_write.end(), calo_collections.begin(), calo_collections.end());
    collections_to_write.insert(collections_to_write.end(), calo_contributions.begin(), calo_contributions.end());

    int totalEventsConsumed = 0;
    bool is_first_frame = true;

    // Loop over sources and read needed events using the new efficient merging approach
    for(auto& source: inputs) {

        const auto& config = source.config;
        int sourceEventsConsumed = 0;

        for (size_t i = 0; i < source.entries_needed; ++i) {
            auto frame = source.mutable_reader->readMutableEntry(config->tree_name, source.current_entry_index);

            // Use the new efficient merging approach
            mergeCollectionsEfficient(frame, *config, output_frame, is_first_frame);
            is_first_frame = false;

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
    
    // Add header to EventHeader collection or create one if it doesn't exist
    if (output_frame->hasCollection("EventHeader")) {
        auto& event_headers = output_frame->getMutable<edm4hep::EventHeaderCollection>("EventHeader");
        event_headers.push_back(header);
    } else {
        auto event_header_collection = std::make_unique<edm4hep::EventHeaderCollection>();
        event_header_collection->push_back(header);
        output_frame->putMutable(std::move(event_header_collection), "EventHeader");
    }
    
    // Convert MutableFrame to podio::Frame for writing
    auto podio_frame = output_frame->toPodioFrame();
    writer->writeFrame(*podio_frame, "events", collections_to_write);
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

void StandaloneTimesliceMerger::mergeCollectionsEfficient(std::unique_ptr<MutableRootReader::MutableFrame>& frame, 
                                                        const SourceConfig& sourceConfig,
                                                        std::unique_ptr<MutableRootReader::MutableFrame>& output_frame,
                                                        bool is_first_frame) {
    float time_offset = 0.0f;
    float distance = 0.0f;
    
    // Calculate time offset if needed
    if (!sourceConfig.already_merged) {
        if (sourceConfig.attach_to_beam) {
            // Get position of first particle with generatorStatus 1
            try {
                auto& particles = frame->getMutable<edm4hep::MCParticleCollection>("MCParticles");
                for (auto& particle : particles) {
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
    
    // Apply time offset directly to collections in-place before merging
    if (time_offset != 0.0f) {
        applyTimeOffsetToFrame(*frame, time_offset, sourceConfig);
    }
    
    if (is_first_frame) {
        // For the first frame, we can move collections directly
        auto available_collections = frame->getAvailableCollections();
        for (const auto& collection_name : available_collections) {
            // Move collection directly from frame to output_frame
            // This preserves all associations without needing to reconstruct them
            transferCollectionBetweenFrames(*frame, *output_frame, collection_name);
        }
    } else {
        // For subsequent frames, we need to merge collections
        // But we can still be more efficient by working with mutable collections directly
        zipCollectionsWithAssociations(*frame, *output_frame, sourceConfig);
    }
}

void StandaloneTimesliceMerger::applyTimeOffsetToFrame(MutableRootReader::MutableFrame& frame, 
                                                      float time_offset, 
                                                      const SourceConfig& sourceConfig) {
    // Apply time offset to MCParticles
    try {
        auto& particles = frame.getMutable<edm4hep::MCParticleCollection>("MCParticles");
        for (auto& particle : particles) {
            particle.setTime(particle.getTime() + time_offset);
        }
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not apply time offset to MCParticles: " << e.what() << std::endl;
    }
    
    // Apply time offset to tracker hits
    auto available_collections = frame.getAvailableCollections();
    for (const auto& collection_name : available_collections) {
        try {
            // Check if it's a tracker hit collection
            auto type_name = frame.getCollectionTypeName(collection_name);
            if (type_name.find("SimTrackerHit") != std::string::npos) {
                auto& hits = frame.getMutable<edm4hep::SimTrackerHitCollection>(collection_name);
                for (auto& hit : hits) {
                    hit.setTime(hit.getTime() + time_offset);
                }
            }
            // Check if it's a calorimeter hit collection
            else if (type_name.find("SimCalorimeterHit") != std::string::npos) {
                auto& hits = frame.getMutable<edm4hep::SimCalorimeterHitCollection>(collection_name);
                for (auto& hit : hits) {
                    // Apply time offset to contributions
                    for (auto& contrib : hit.getContributions()) {
                        contrib.setTime(contrib.getTime() + time_offset);
                    }
                }
            }
            // Check if it's a contribution collection
            else if (type_name.find("CaloHitContribution") != std::string::npos) {
                auto& contribs = frame.getMutable<edm4hep::CaloHitContributionCollection>(collection_name);
                for (auto& contrib : contribs) {
                    contrib.setTime(contrib.getTime() + time_offset);
                }
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not apply time offset to collection " << collection_name 
                      << ": " << e.what() << std::endl;
        }
    }
}

void StandaloneTimesliceMerger::transferCollectionBetweenFrames(MutableRootReader::MutableFrame& source_frame,
                                                              MutableRootReader::MutableFrame& dest_frame,
                                                              const std::string& collection_name) {
    try {
        auto type_name = source_frame.getCollectionTypeName(collection_name);
        
        // Use type-specific transfer to maintain type safety
        if (type_name.find("MCParticleCollection") != std::string::npos) {
            source_frame.moveCollectionTo<edm4hep::MCParticleCollection>(collection_name, dest_frame);
        }
        else if (type_name.find("EventHeaderCollection") != std::string::npos) {
            source_frame.moveCollectionTo<edm4hep::EventHeaderCollection>(collection_name, dest_frame);
        }
        else if (type_name.find("SimTrackerHitCollection") != std::string::npos) {
            source_frame.moveCollectionTo<edm4hep::SimTrackerHitCollection>(collection_name, dest_frame);
        }
        else if (type_name.find("SimCalorimeterHitCollection") != std::string::npos) {
            source_frame.moveCollectionTo<edm4hep::SimCalorimeterHitCollection>(collection_name, dest_frame);
        }
        else if (type_name.find("CaloHitContributionCollection") != std::string::npos) {
            source_frame.moveCollectionTo<edm4hep::CaloHitContributionCollection>(collection_name, dest_frame);
        }
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not transfer collection " << collection_name 
                  << ": " << e.what() << std::endl;
    }
}

void StandaloneTimesliceMerger::zipCollectionsWithAssociations(MutableRootReader::MutableFrame& source_frame,
                                                             MutableRootReader::MutableFrame& dest_frame,
                                                             const SourceConfig& sourceConfig) {
    // This is a more efficient approach that zips collections while preserving associations
    // For now, we'll merge the collections into the destination frame
    
    auto available_collections = source_frame.getAvailableCollections();
    
    for (const auto& collection_name : available_collections) {
        try {
            auto type_name = source_frame.getCollectionTypeName(collection_name);
            
            // Merge collections based on type
            if (type_name.find("MCParticleCollection") != std::string::npos) {
                auto& source_particles = source_frame.getMutable<edm4hep::MCParticleCollection>(collection_name);
                
                // Get or create destination collection
                edm4hep::MCParticleCollection* dest_particles = nullptr;
                if (dest_frame.hasCollection(collection_name)) {
                    dest_particles = &dest_frame.getMutable<edm4hep::MCParticleCollection>(collection_name);
                } else {
                    auto new_collection = std::make_unique<edm4hep::MCParticleCollection>();
                    dest_particles = new_collection.get();
                    dest_frame.putMutable(std::move(new_collection), collection_name);
                }
                
                // Append particles to destination (associations are preserved within the collection)
                for (auto& particle : source_particles) {
                    dest_particles->push_back(particle);
                }
            }
            else if (type_name.find("SimTrackerHitCollection") != std::string::npos) {
                auto& source_hits = source_frame.getMutable<edm4hep::SimTrackerHitCollection>(collection_name);
                
                // Get or create destination collection
                edm4hep::SimTrackerHitCollection* dest_hits = nullptr;
                if (dest_frame.hasCollection(collection_name)) {
                    dest_hits = &dest_frame.getMutable<edm4hep::SimTrackerHitCollection>(collection_name);
                } else {
                    auto new_collection = std::make_unique<edm4hep::SimTrackerHitCollection>();
                    dest_hits = new_collection.get();
                    dest_frame.putMutable(std::move(new_collection), collection_name);
                }
                
                // Append hits to destination
                for (auto& hit : source_hits) {
                    dest_hits->push_back(hit);
                }
            }
            else if (type_name.find("SimCalorimeterHitCollection") != std::string::npos) {
                auto& source_hits = source_frame.getMutable<edm4hep::SimCalorimeterHitCollection>(collection_name);
                
                // Get or create destination collection
                edm4hep::SimCalorimeterHitCollection* dest_hits = nullptr;
                if (dest_frame.hasCollection(collection_name)) {
                    dest_hits = &dest_frame.getMutable<edm4hep::SimCalorimeterHitCollection>(collection_name);
                } else {
                    auto new_collection = std::make_unique<edm4hep::SimCalorimeterHitCollection>();
                    dest_hits = new_collection.get();
                    dest_frame.putMutable(std::move(new_collection), collection_name);
                }
                
                // Append hits to destination
                for (auto& hit : source_hits) {
                    dest_hits->push_back(hit);
                }
            }
            else if (type_name.find("CaloHitContributionCollection") != std::string::npos) {
                auto& source_contribs = source_frame.getMutable<edm4hep::CaloHitContributionCollection>(collection_name);
                
                // Get or create destination collection
                edm4hep::CaloHitContributionCollection* dest_contribs = nullptr;
                if (dest_frame.hasCollection(collection_name)) {
                    dest_contribs = &dest_frame.getMutable<edm4hep::CaloHitContributionCollection>(collection_name);
                } else {
                    auto new_collection = std::make_unique<edm4hep::CaloHitContributionCollection>();
                    dest_contribs = new_collection.get();
                    dest_frame.putMutable(std::move(new_collection), collection_name);
                }
                
                // Append contributions to destination
                for (auto& contrib : source_contribs) {
                    dest_contribs->push_back(contrib);
                }
            }
            else if (type_name.find("EventHeaderCollection") != std::string::npos) {
                // Special handling for event headers - we typically want to merge them differently
                auto& source_headers = source_frame.getMutable<edm4hep::EventHeaderCollection>(collection_name);
                
                if (collection_name == "SubEventHeaders") {
                    // Get or create destination collection
                    edm4hep::EventHeaderCollection* dest_headers = nullptr;
                    if (dest_frame.hasCollection(collection_name)) {
                        dest_headers = &dest_frame.getMutable<edm4hep::EventHeaderCollection>(collection_name);
                    } else {
                        auto new_collection = std::make_unique<edm4hep::EventHeaderCollection>();
                        dest_headers = new_collection.get();
                        dest_frame.putMutable(std::move(new_collection), collection_name);
                    }
                    
                    for (auto& header : source_headers) {
                        dest_headers->push_back(header);
                    }
                }
                // For main EventHeader, we usually don't append but handle differently in createMergedTimeslice
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not zip collection " << collection_name 
                      << ": " << e.what() << std::endl;
        }
    }
}
