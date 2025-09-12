#include "StandaloneTimesliceMerger.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <TBranch.h>
#include <TObjArray.h>

StandaloneTimesliceMerger::StandaloneTimesliceMerger(const MergerConfig& config)
    : m_config(config), gen(rd()), events_generated(0) {
    
}

void StandaloneTimesliceMerger::run() {
    std::cout << "Starting timeslice merger (vector-based approach)..." << std::endl;
    std::cout << "Sources: " << m_config.sources.size() << std::endl;
    std::cout << "Output file: " << m_config.output_file << std::endl;
    std::cout << "Max events: " << m_config.max_events << std::endl;
    std::cout << "Timeslice duration: " << m_config.time_slice_duration << std::endl;
    
    // Open output file and create tree
    auto output_file = std::make_unique<TFile>(m_config.output_file.c_str(), "RECREATE");
    if (!output_file || output_file->IsZombie()) {
        throw std::runtime_error("Could not create output file: " + m_config.output_file);
    }
    
    TTree* output_tree = new TTree("events", "Merged timeslices");
    
    auto inputs = initializeInputFiles();
    setupOutputTree(output_tree);

    std::cout << "Processing " << m_config.max_events << " timeslices..." << std::endl;

    while (events_generated < m_config.max_events) {
        // Update number of events needed per source
        if (!updateInputNEvents(inputs)) {
            std::cout << "Reached end of input data, stopping at " << events_generated << " timeslices" << std::endl;
            break;
        }
        createMergedTimeslice(inputs, output_file, output_tree);

        events_generated++;
        
        if (events_generated % 10 == 0) {
            std::cout << "Processed " << events_generated << " timeslices..." << std::endl;
        }
    }
    
    output_tree->Write();
    output_file->Close();
    std::cout << "Generated " << events_generated << " timeslices" << std::endl;
    std::cout << "Output saved to: " << m_config.output_file << std::endl;
}

// Initialize input files and validate sources
std::vector<SourceReader> StandaloneTimesliceMerger::initializeInputFiles() {
    std::vector<SourceReader> source_readers(m_config.sources.size());
    
    size_t source_idx = 0;

    for (auto& source_reader : source_readers) {
        const auto& source = m_config.sources[source_idx];
        source_reader.config = &source;

        if (!source.input_files.empty()) {
            try {
                // Create TChain for this source
                source_reader.chain = std::make_unique<TChain>(source.tree_name.c_str());
                
                // Add all input files to the chain
                for (const auto& file : source.input_files) {
                    int result = source_reader.chain->Add(file.c_str());
                    if (result == 0) {
                        throw std::runtime_error("Failed to add file: " + file);
                    }
                    std::cout << "Added file to source " << source_idx << ": " << file << std::endl;
                }
                
                source_reader.total_entries = source_reader.chain->GetEntries();
                
                if (source_reader.total_entries == 0) {
                    throw std::runtime_error("No entries found in source " + std::to_string(source_idx));
                }

                std::cout << "Source " << source_idx << " has " << source_reader.total_entries << " entries" << std::endl;

                // Discover collection names from the tree structure
                if (source_idx == 0) {
                    // Use first source to determine what collections are available
                    tracker_collection_names = discoverCollectionNames(source_reader, "SimTrackerHit");
                    calo_collection_names = discoverCollectionNames(source_reader, "SimCalorimeterHit");
                    calo_contrib_collection_names = discoverCollectionNames(source_reader, "CaloHitContribution");
                    
                    std::cout << "Discovered " << tracker_collection_names.size() << " tracker collections, " 
                              << calo_collection_names.size() << " calorimeter collections, "
                              << calo_contrib_collection_names.size() << " contribution collections" << std::endl;
                }

                // Setup branch reading for this source
                source_reader.collection_names_to_read = {"MCParticles", "EventHeader"};
                source_reader.collection_names_to_read.insert(source_reader.collection_names_to_read.end(), 
                                                            tracker_collection_names.begin(), 
                                                            tracker_collection_names.end());
                source_reader.collection_names_to_read.insert(source_reader.collection_names_to_read.end(), 
                                                            calo_collection_names.begin(), 
                                                            calo_collection_names.end());
                source_reader.collection_names_to_read.insert(source_reader.collection_names_to_read.end(), 
                                                            calo_contrib_collection_names.begin(), 
                                                            calo_contrib_collection_names.end());

                // Check for SubEventHeaders if already_merged
                if (source.already_merged) {
                    source_reader.collection_names_to_read.push_back("SubEventHeaders");
                }

                // Setup branch pointers for reading data
                for (const auto& coll_name : source_reader.collection_names_to_read) {
                    if (coll_name == "MCParticles") {
                        source_reader.mcparticle_branches[coll_name] = new std::vector<edm4hep::MCParticleData>();
                        source_reader.chain->SetBranchAddress(coll_name.c_str(), &source_reader.mcparticle_branches[coll_name]);
                    } else if (coll_name == "EventHeader" || coll_name == "SubEventHeaders") {
                        source_reader.event_header_branches[coll_name] = new std::vector<edm4hep::EventHeaderData>();
                        source_reader.chain->SetBranchAddress(coll_name.c_str(), &source_reader.event_header_branches[coll_name]);
                    } else if (std::find(tracker_collection_names.begin(), tracker_collection_names.end(), coll_name) != tracker_collection_names.end()) {
                        source_reader.tracker_hit_branches[coll_name] = new std::vector<edm4hep::SimTrackerHitData>();
                        source_reader.chain->SetBranchAddress(coll_name.c_str(), &source_reader.tracker_hit_branches[coll_name]);
                        
                        // Also setup the particle reference branch
                        std::string ref_branch_name = "_B0" + coll_name + "_particle";
                        source_reader.tracker_hit_particle_refs[coll_name] = new std::vector<podio::ObjectID>();
                        source_reader.chain->SetBranchAddress(ref_branch_name.c_str(), &source_reader.tracker_hit_particle_refs[coll_name]);
                        
                    } else if (std::find(calo_collection_names.begin(), calo_collection_names.end(), coll_name) != calo_collection_names.end()) {
                        source_reader.calo_hit_branches[coll_name] = new std::vector<edm4hep::SimCalorimeterHitData>();
                        source_reader.chain->SetBranchAddress(coll_name.c_str(), &source_reader.calo_hit_branches[coll_name]);
                    } else if (std::find(calo_contrib_collection_names.begin(), calo_contrib_collection_names.end(), coll_name) != calo_contrib_collection_names.end()) {
                        source_reader.calo_contrib_branches[coll_name] = new std::vector<edm4hep::CaloHitContributionData>();
                        source_reader.chain->SetBranchAddress(coll_name.c_str(), &source_reader.calo_contrib_branches[coll_name]);
                        
                        // Also setup the particle reference branch
                        std::string ref_branch_name = "_B0" + coll_name + "_particle";  
                        source_reader.calo_contrib_particle_refs[coll_name] = new std::vector<podio::ObjectID>();
                        source_reader.chain->SetBranchAddress(ref_branch_name.c_str(), &source_reader.calo_contrib_particle_refs[coll_name]);
                    }
                }

                std::cout << "Successfully initialized source " << source_idx << " (" << source.name << ")" << std::endl;

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



void StandaloneTimesliceMerger::createMergedTimeslice(std::vector<SourceReader>& inputs, std::unique_ptr<TFile>& output_file, TTree* output_tree) {
    
    // Clear global vectors for new timeslice
    merged_mcparticles.clear();
    merged_event_headers.clear();
    merged_sub_event_headers.clear();
    for (auto& [name, vec] : merged_tracker_hits) {
        vec.clear();
    }
    for (auto& [name, vec] : merged_calo_hits) {
        vec.clear();
    }
    for (auto& [name, vec] : merged_calo_contributions) {
        vec.clear();
    }
    for (auto& [name, vec] : merged_tracker_hit_particle_refs) {
        vec.clear();
    }
    for (auto& [name, vec] : merged_calo_contrib_particle_refs) {
        vec.clear();
    }

    int totalEventsConsumed = 0;

    // Loop over sources and read needed events
    for(auto& source: inputs) {
        const auto& config = source.config;
        int sourceEventsConsumed = 0;

        for (size_t i = 0; i < source.entries_needed; ++i) {
            // Read event data from this source
            mergeEventData(source, source.current_entry_index, *config);

            source.current_entry_index++;
            sourceEventsConsumed++;
            totalEventsConsumed++;
        }

        std::cout << "Merged " << sourceEventsConsumed << " events, totalling " << source.current_entry_index << " from source " << config->name << std::endl;
    }

    // Create main timeslice header
    edm4hep::EventHeaderData header;
    header.eventNumber = events_generated;
    header.runNumber = 0;
    header.timeStamp = events_generated;
    merged_event_headers.push_back(header);

    // Write merged data to output tree
    writeTimesliceToTree(output_tree);
}

void StandaloneTimesliceMerger::mergeEventData(SourceReader& source, size_t event_index, const SourceConfig& sourceConfig) {
    // Load the event data from the TChain
    source.chain->GetEntry(event_index);
    
    float time_offset = 0.0f;
    float distance = 0.0f;
    
    // Get the particles vector for reference handling
    std::vector<edm4hep::MCParticleData> particles;
    if (source.mcparticle_branches.count("MCParticles")) {
        particles = *source.mcparticle_branches["MCParticles"];
    }
    
    // Calculate time offset if not already merged
    if (!sourceConfig.already_merged) {
        if (sourceConfig.attach_to_beam && !particles.empty()) {
            // Get position of first particle with generatorStatus 1
            try {
                for (const auto& particle : particles) {
                    if (particle.generatorStatus == 1) {
                        // Distance is dot product of position vector relative to rotation around y of beam relative to z-axis
                        distance = particle.vertex.z * std::cos(sourceConfig.beam_angle) + particle.vertex.x * std::sin(sourceConfig.beam_angle);
                        break;
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "Warning: Could not access MCParticles for beam attachment: " << e.what() << std::endl;
            }
        }
        time_offset = generateTimeOffset(sourceConfig, distance);
    }

    // Keep track of starting index for MCParticles from this event
    size_t particle_index_offset = merged_mcparticles.size();

    // Process MCParticles
    for (const auto& particle : particles) {
        edm4hep::MCParticleData new_particle = particle;
        new_particle.time += time_offset;
        // Update generator status offset
        new_particle.generatorStatus += sourceConfig.generator_status_offset;
        merged_mcparticles.push_back(new_particle);
    }
    
    // Update parent-daughter relationships by adjusting indices
    size_t start_idx = particle_index_offset;
    for (size_t i = start_idx; i < merged_mcparticles.size(); ++i) {
        auto& particle = merged_mcparticles[i];
        const auto& original_particle = particles[i - start_idx];
        
        // Update parent indices
        if (original_particle.parents_begin >= 0) {
            particle.parents_begin = start_idx + original_particle.parents_begin;
            particle.parents_end = start_idx + original_particle.parents_end;
        }
        
        // Update daughter indices  
        if (original_particle.daughters_begin >= 0) {
            particle.daughters_begin = start_idx + original_particle.daughters_begin;  
            particle.daughters_end = start_idx + original_particle.daughters_end;
        }
    }

    // Process SubEventHeaders
    if (sourceConfig.already_merged && source.event_header_branches.count("SubEventHeaders")) {
        const auto& sub_headers = *source.event_header_branches["SubEventHeaders"];
        for (const auto& header : sub_headers) {
            edm4hep::EventHeaderData new_header = header;
            new_header.eventNumber = merged_sub_event_headers.size();
            // Update timestamp to reflect new MCParticle index offset
            new_header.timeStamp = particle_index_offset + header.timeStamp;
            merged_sub_event_headers.push_back(new_header);
        }
    } else {
        // Create new SubEventHeader for regular source
        edm4hep::EventHeaderData sub_header{};
        sub_header.eventNumber = merged_sub_event_headers.size();
        sub_header.runNumber = 0;
        sub_header.timeStamp = particle_index_offset; // index of first MCParticle for this sub-event
        sub_header.weight = time_offset; // time offset
        
        // Copy weights from EventHeader if available
        if (source.event_header_branches.count("EventHeader")) {
            const auto& event_headers = *source.event_header_branches["EventHeader"];
            if (!event_headers.empty()) {
                const auto& event_header = event_headers[0];
                // Note: EventHeaderData doesn't have weights vector, so we store time_offset in weight field
            }
        }
        merged_sub_event_headers.push_back(sub_header);
    }

    // Initialize merged collections for tracker hits if not already done
    for (const auto& name : tracker_collection_names) {
        if (merged_tracker_hits.find(name) == merged_tracker_hits.end()) {
            merged_tracker_hits[name] = std::vector<edm4hep::SimTrackerHitData>();
        }
        if (merged_tracker_hit_particle_refs.find(name) == merged_tracker_hit_particle_refs.end()) {
            merged_tracker_hit_particle_refs[name] = std::vector<podio::ObjectID>();
        }
    }

    // Initialize merged collections for calo hits if not already done  
    for (const auto& name : calo_collection_names) {
        if (merged_calo_hits.find(name) == merged_calo_hits.end()) {
            merged_calo_hits[name] = std::vector<edm4hep::SimCalorimeterHitData>();
        }
    }

    // Initialize merged collections for calo contributions if not already done
    for (const auto& name : calo_contrib_collection_names) {
        if (merged_calo_contributions.find(name) == merged_calo_contributions.end()) {
            merged_calo_contributions[name] = std::vector<edm4hep::CaloHitContributionData>();
        }
        if (merged_calo_contrib_particle_refs.find(name) == merged_calo_contrib_particle_refs.end()) {
            merged_calo_contrib_particle_refs[name] = std::vector<podio::ObjectID>();
        }
    }

    // Process tracker hits
    for (const auto& name : tracker_collection_names) {
        if (source.tracker_hit_branches.count(name) && source.tracker_hit_particle_refs.count(name)) {
            const auto& hits = *source.tracker_hit_branches[name];
            const auto& particle_refs = *source.tracker_hit_particle_refs[name];
            
            if (hits.size() != particle_refs.size()) {
                std::cout << "Warning: Mismatch between hit count (" << hits.size() 
                         << ") and particle reference count (" << particle_refs.size() 
                         << ") for collection " << name << std::endl;
                continue;
            }
            
            for (size_t i = 0; i < hits.size(); ++i) {
                const auto& hit = hits[i];
                const auto& particle_ref = particle_refs[i];
                
                edm4hep::SimTrackerHitData new_hit = hit;
                new_hit.time += time_offset;
                merged_tracker_hits[name].push_back(new_hit);
                
                // Update particle reference if valid
                podio::ObjectID new_particle_ref = particle_ref;
                if (particle_ref.index >= 0 && particle_ref.index < static_cast<int>(particles.size())) {
                    new_particle_ref.index = particle_index_offset + particle_ref.index;
                }
                merged_tracker_hit_particle_refs[name].push_back(new_particle_ref);
            }
        }
    }

    // Process calorimeter hits and contributions
    for (const auto& name : calo_collection_names) {
        if (source.calo_hit_branches.count(name)) {
            const auto& hits = *source.calo_hit_branches[name];
            for (const auto& hit : hits) {
                edm4hep::SimCalorimeterHitData new_hit = hit;
                // Note: SimCalorimeterHitData doesn't have time field directly, 
                // time is in the contributions
                merged_calo_hits[name].push_back(new_hit);
            }
        }
    }

    // Process calorimeter contributions
    for (const auto& name : calo_contrib_collection_names) {
        if (source.calo_contrib_branches.count(name) && source.calo_contrib_particle_refs.count(name)) {
            const auto& contribs = *source.calo_contrib_branches[name];
            const auto& particle_refs = *source.calo_contrib_particle_refs[name];
            
            if (contribs.size() != particle_refs.size()) {
                std::cout << "Warning: Mismatch between contribution count (" << contribs.size() 
                         << ") and particle reference count (" << particle_refs.size() 
                         << ") for collection " << name << std::endl;
                continue;
            }
            
            for (size_t i = 0; i < contribs.size(); ++i) {
                const auto& contrib = contribs[i];
                const auto& particle_ref = particle_refs[i];
                
                edm4hep::CaloHitContributionData new_contrib = contrib;
                new_contrib.time += time_offset;
                merged_calo_contributions[name].push_back(new_contrib);
                
                // Update particle reference if valid
                podio::ObjectID new_particle_ref = particle_ref;
                if (particle_ref.index >= 0 && particle_ref.index < static_cast<int>(particles.size())) {
                    new_particle_ref.index = particle_index_offset + particle_ref.index;
                }
                merged_calo_contrib_particle_refs[name].push_back(new_particle_ref);
            }
        }
    }
}

void StandaloneTimesliceMerger::setupOutputTree(TTree* tree) {
    // Set branch addresses for output tree
    tree->Branch("MCParticles", &merged_mcparticles);
    tree->Branch("EventHeader", &merged_event_headers);
    tree->Branch("SubEventHeaders", &merged_sub_event_headers);
    
    // Setup branches for tracker hits
    for (const auto& name : tracker_collection_names) {
        tree->Branch(name.c_str(), &merged_tracker_hits[name]);
        // Also create the particle reference branch
        std::string ref_branch_name = "_B0" + name + "_particle";
        tree->Branch(ref_branch_name.c_str(), &merged_tracker_hit_particle_refs[name]);
    }
    
    // Setup branches for calo hits
    for (const auto& name : calo_collection_names) {
        tree->Branch(name.c_str(), &merged_calo_hits[name]);
    }
    
    // Setup branches for calo contributions
    for (const auto& name : calo_contrib_collection_names) {
        tree->Branch(name.c_str(), &merged_calo_contributions[name]);
        // Also create the particle reference branch
        std::string ref_branch_name = "_B0" + name + "_particle";
        tree->Branch(ref_branch_name.c_str(), &merged_calo_contrib_particle_refs[name]);
    }
}

void StandaloneTimesliceMerger::writeTimesliceToTree(TTree* tree) {
    // Fill the tree with current merged data
    tree->Fill();
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

std::vector<std::string> StandaloneTimesliceMerger::discoverCollectionNames(SourceReader& reader, const std::string& branch_pattern) {
    std::vector<std::string> names;
    
    // Get list of branches from the TChain
    TObjArray* branches = reader.chain->GetListOfBranches();
    if (!branches) {
        std::cout << "Warning: No branches found in source" << std::endl;
        return names;
    }
    
    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = (TBranch*)branches->At(i);
        if (!branch) continue;
        
        std::string branch_name = branch->GetName();
        
        // Enhanced pattern matching for collection discovery based on dd4hep/edm4hep naming conventions
        if (branch_pattern.find("SimTrackerHit") != std::string::npos) {
            // Look for SimTrackerHit collections (excluding contributions)
            if (branch_name.find("SimTrackerHit") != std::string::npos && 
                branch_name.find("Contribution") == std::string::npos) {
                names.push_back(branch_name);
            }
        } else if (branch_pattern.find("SimCalorimeterHit") != std::string::npos) {
            // Look for SimCalorimeterHit collections (excluding contributions) 
            if (branch_name.find("SimCalorimeterHit") != std::string::npos && 
                branch_name.find("Contribution") == std::string::npos) {
                names.push_back(branch_name);
            }
        } else if (branch_pattern.find("CaloHitContribution") != std::string::npos) {
            // Look for CaloHitContribution collections
            if (branch_name.find("CaloHitContribution") != std::string::npos) {
                names.push_back(branch_name);
            }
        }
    }
    
    std::cout << "Discovered " << names.size() << " collections matching pattern: " << branch_pattern << std::endl;
    for (const auto& name : names) {
        std::cout << "  - " << name << std::endl;
    }
    
    return names;
}
