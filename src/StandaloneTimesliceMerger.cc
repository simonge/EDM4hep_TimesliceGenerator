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
                    // Load first entry to ensure branch list is populated
                    if (source_reader.total_entries > 0) {
                        source_reader.chain->LoadTree(0);
                    }
                    
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
                std::cout << "=== Setting up branches for source " << source_idx << " ===" << std::endl;
                std::cout << "Collections to read: " << source_reader.collection_names_to_read.size() << std::endl;
                for (const auto& coll_name : source_reader.collection_names_to_read) {
                    std::cout << "  Setting up: " << coll_name << std::endl;
                    
                    if (coll_name == "MCParticles") {
                        source_reader.mcparticle_branches[coll_name] = new std::vector<edm4hep::MCParticleData>();
                        int result = source_reader.chain->SetBranchAddress(coll_name.c_str(), &source_reader.mcparticle_branches[coll_name]);
                        if (result != 0) {
                            std::cout << "    ❌ Could not set branch address for " << coll_name << " (result: " << result << ")" << std::endl;
                        } else {
                            std::cout << "    ✓ Successfully set up " << coll_name << std::endl;
                        }
                        
                        // Also setup the parent and children reference branches
                        std::string parents_branch_name = "_MCParticles_parents";
                        std::string children_branch_name = "_MCParticles_daughters";
                        source_reader.mcparticle_parents_refs[coll_name] = new std::vector<podio::ObjectID>();
                        source_reader.mcparticle_children_refs[coll_name] = new std::vector<podio::ObjectID>();
                        
                        result = source_reader.chain->SetBranchAddress(parents_branch_name.c_str(), &source_reader.mcparticle_parents_refs[coll_name]);
                        if (result != 0) {
                            std::cout << "    ⚠️  Could not set branch address for " << parents_branch_name << " (result: " << result << ")" << std::endl;
                            std::cout << "       This may be normal if the input file doesn't contain MCParticle relationship information" << std::endl;
                        } else {
                            std::cout << "    ✓ Successfully set up " << parents_branch_name << std::endl;
                        }
                        
                        result = source_reader.chain->SetBranchAddress(children_branch_name.c_str(), &source_reader.mcparticle_children_refs[coll_name]);
                        if (result != 0) {
                            std::cout << "    ⚠️  Could not set branch address for " << children_branch_name << " (result: " << result << ")" << std::endl;
                            std::cout << "       This may be normal if the input file doesn't contain MCParticle relationship information" << std::endl;
                        } else {
                            std::cout << "    ✓ Successfully set up " << children_branch_name << std::endl;
                        }
                        
                    } else if (coll_name == "EventHeader" || coll_name == "SubEventHeaders") {
                        source_reader.event_header_branches[coll_name] = new std::vector<edm4hep::EventHeaderData>();
                        int result = source_reader.chain->SetBranchAddress(coll_name.c_str(), &source_reader.event_header_branches[coll_name]);
                        if (result != 0) {
                            std::cout << "    ❌ Could not set branch address for " << coll_name << " (result: " << result << ")" << std::endl;
                        } else {
                            std::cout << "    ✓ Successfully set up " << coll_name << std::endl;
                        }
                    } else if (std::find(tracker_collection_names.begin(), tracker_collection_names.end(), coll_name) != tracker_collection_names.end()) {
                        source_reader.tracker_hit_branches[coll_name] = new std::vector<edm4hep::SimTrackerHitData>();
                        int result = source_reader.chain->SetBranchAddress(coll_name.c_str(), &source_reader.tracker_hit_branches[coll_name]);
                        if (result != 0) {
                            std::cout << "    ❌ Could not set branch address for " << coll_name << " (result: " << result << ")" << std::endl;
                        } else {
                            std::cout << "    ✓ Successfully set up tracker collection " << coll_name << std::endl;
                        }
                        
                        // Also setup the particle reference branch
                        std::string ref_branch_name = "_" + coll_name + "_particle";
                        source_reader.tracker_hit_particle_refs[coll_name] = new std::vector<podio::ObjectID>();
                        result = source_reader.chain->SetBranchAddress(ref_branch_name.c_str(), &source_reader.tracker_hit_particle_refs[coll_name]);
                        if (result != 0) {
                            std::cout << "    ❌ Could not set branch address for " << ref_branch_name << " (result: " << result << ")" << std::endl;
                        } else {
                            std::cout << "    ✓ Successfully set up particle ref " << ref_branch_name << std::endl;
                        }
                        
                    } else if (std::find(calo_collection_names.begin(), calo_collection_names.end(), coll_name) != calo_collection_names.end()) {
                        source_reader.calo_hit_branches[coll_name] = new std::vector<edm4hep::SimCalorimeterHitData>();
                        int result = source_reader.chain->SetBranchAddress(coll_name.c_str(), &source_reader.calo_hit_branches[coll_name]);
                        if (result != 0) {
                            std::cout << "    ❌ Could not set branch address for " << coll_name << " (result: " << result << ")" << std::endl;
                        } else {
                            std::cout << "    ✓ Successfully set up calo collection " << coll_name << std::endl;
                        }
                    } else if (std::find(calo_contrib_collection_names.begin(), calo_contrib_collection_names.end(), coll_name) != calo_contrib_collection_names.end()) {
                        source_reader.calo_contrib_branches[coll_name] = new std::vector<edm4hep::CaloHitContributionData>();
                        int result = source_reader.chain->SetBranchAddress(coll_name.c_str(), &source_reader.calo_contrib_branches[coll_name]);
                        if (result != 0) {
                            std::cout << "    ❌ Could not set branch address for " << coll_name << " (result: " << result << ")" << std::endl;
                        } else {
                            std::cout << "    ✓ Successfully set up calo contrib collection " << coll_name << std::endl;
                        }
                        
                        // Also setup the particle reference branch
                        std::string ref_branch_name = "_" + coll_name + "_particle";  
                        source_reader.calo_contrib_particle_refs[coll_name] = new std::vector<podio::ObjectID>();
                        result = source_reader.chain->SetBranchAddress(ref_branch_name.c_str(), &source_reader.calo_contrib_particle_refs[coll_name]);
                        if (result != 0) {
                            std::cout << "    ❌ Could not set branch address for " << ref_branch_name << " (result: " << result << ")" << std::endl;
                        } else {
                            std::cout << "    ✓ Successfully set up particle ref " << ref_branch_name << std::endl;
                        }
                    } else {
                        std::cout << "    ⚠️  Unknown collection type: " << coll_name << std::endl;
                    }
                }
                std::cout << "=== Branch setup complete ===" << std::endl;

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
    for (auto& [name, vec] : merged_mcparticle_parents_refs) {
        vec.clear();
    }
    for (auto& [name, vec] : merged_mcparticle_children_refs) {
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
    
    // Process MCParticle parent-child relationships
    if (source.mcparticle_parents_refs.count("MCParticles") && source.mcparticle_children_refs.count("MCParticles")) {
        const auto& parents_refs = *source.mcparticle_parents_refs["MCParticles"];
        const auto& children_refs = *source.mcparticle_children_refs["MCParticles"];
        
        std::cout << "Processing MCParticle relationships: " << parents_refs.size() << " parent refs, " 
                  << children_refs.size() << " children refs" << std::endl;
        
        // Initialize merged relationship vectors if not already done
        if (merged_mcparticle_parents_refs.find("MCParticles") == merged_mcparticle_parents_refs.end()) {
            merged_mcparticle_parents_refs["MCParticles"] = std::vector<podio::ObjectID>();
        }
        if (merged_mcparticle_children_refs.find("MCParticles") == merged_mcparticle_children_refs.end()) {
            merged_mcparticle_children_refs["MCParticles"] = std::vector<podio::ObjectID>();
        }
        
        // Update parent references - adjust indices to account for particle offset
        for (const auto& parent_ref : parents_refs) {
            podio::ObjectID new_parent_ref = parent_ref;
            if (parent_ref.index >= 0 && parent_ref.index < static_cast<int>(particles.size())) {
                new_parent_ref.index = particle_index_offset + parent_ref.index;
            }
            merged_mcparticle_parents_refs["MCParticles"].push_back(new_parent_ref);
        }
        
        // Update children references - adjust indices to account for particle offset
        for (const auto& child_ref : children_refs) {
            podio::ObjectID new_child_ref = child_ref;
            if (child_ref.index >= 0 && child_ref.index < static_cast<int>(particles.size())) {
                new_child_ref.index = particle_index_offset + child_ref.index;
            }
            merged_mcparticle_children_refs["MCParticles"].push_back(new_child_ref);
        }
    } else {
        std::cout << "No MCParticle parent-child relationship branches found in input" << std::endl;
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
    
    // Setup branches for MCParticle parent-child relationships
    tree->Branch("_MCParticles_parents", &merged_mcparticle_parents_refs["MCParticles"]);
    tree->Branch("_MCParticles_daughters", &merged_mcparticle_children_refs["MCParticles"]);
    
    // Setup branches for tracker hits
    for (const auto& name : tracker_collection_names) {
        tree->Branch(name.c_str(), &merged_tracker_hits[name]);
        // Also create the particle reference branch
        std::string ref_branch_name = "_" + name + "_particle";
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
        std::string ref_branch_name = "_" + name + "_particle";
        tree->Branch(ref_branch_name.c_str(), &merged_calo_contrib_particle_refs[name]);
    }
}

void StandaloneTimesliceMerger::writeTimesliceToTree(TTree* tree) {
    // Debug: show sizes of merged vectors before writing
    std::cout << "=== Writing timeslice ===" << std::endl;
    std::cout << "  MCParticles: " << merged_mcparticles.size() << std::endl;
    std::cout << "  MCParticle parents: " << merged_mcparticle_parents_refs["MCParticles"].size() << std::endl;
    std::cout << "  MCParticle daughters: " << merged_mcparticle_children_refs["MCParticles"].size() << std::endl;
    
    std::cout << "  Tracker collections (" << merged_tracker_hits.size() << "):" << std::endl;
    for (const auto& [name, hits] : merged_tracker_hits) {
        std::cout << "    " << name << ": " << hits.size() << " hits, " 
                  << merged_tracker_hit_particle_refs[name].size() << " particle refs" << std::endl;
    }
    
    std::cout << "  Calo collections (" << merged_calo_hits.size() << "):" << std::endl;
    for (const auto& [name, hits] : merged_calo_hits) {
        std::cout << "    " << name << ": " << hits.size() << " hits" << std::endl;
    }
    
    std::cout << "  Calo contribution collections (" << merged_calo_contributions.size() << "):" << std::endl;
    for (const auto& [name, contribs] : merged_calo_contributions) {
        std::cout << "    " << name << ": " << contribs.size() << " contributions, " 
                  << merged_calo_contrib_particle_refs[name].size() << " particle refs" << std::endl;
    }
    
    // Fill the tree with current merged data
    tree->Fill();
    std::cout << "=== Timeslice written ===" << std::endl;
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
    
    std::cout << "=== Branch Discovery for pattern: " << branch_pattern << " ===" << std::endl;
    std::cout << "Total branches in chain: " << branches->GetEntries() << std::endl;
    
    // List all branches for debugging
    for (int i = 0; i < branches->GetEntries() && i < 20; ++i) {  // Limit to first 20 for readability
        TBranch* branch = (TBranch*)branches->At(i);
        if (!branch) continue;
        std::string branch_name = branch->GetName();
        std::string branch_class_name = "";
        if (branch->GetExpectedType() && branch->GetExpectedType()->GetName()) {
            branch_class_name = branch->GetExpectedType()->GetName();
        }
        std::cout << "  Branch[" << i << "]: " << branch_name << " (type: " << branch_class_name << ")";
        if (branch_name.find("_") == 0) std::cout << " (ObjectID branch)";
        std::cout << std::endl;
    }
    
    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = (TBranch*)branches->At(i);
        if (!branch) continue;
        
        std::string branch_name = branch->GetName();
        std::string branch_type = "";
        
        // Get the branch data type
        if (branch->GetExpectedType() && branch->GetExpectedType()->GetName()) {
            branch_type = branch->GetExpectedType()->GetName();
        }
        
        // Skip branches that start with "_" for now - these are ObjectID references
        // We'll handle them separately
        if (branch_name.find("_") == 0) continue;
        
        // Look for collections based on data type rather than branch name
        if (branch_pattern.find("SimTrackerHit") != std::string::npos) {
            // Look for vector<edm4hep::SimTrackerHitData> or similar
            if (branch_type.find("vector<edm4hep::SimTrackerHitData>") != std::string::npos ||
                branch_type.find("SimTrackerHitData") != std::string::npos) {
                names.push_back(branch_name);
                std::cout << "  ✓ MATCHED TRACKER: " << branch_name << " (type: " << branch_type << ")" << std::endl;
            }
        } else if (branch_pattern.find("SimCalorimeterHit") != std::string::npos) {
            // Look for vector<edm4hep::SimCalorimeterHitData> or similar
            if (branch_type.find("vector<edm4hep::SimCalorimeterHitData>") != std::string::npos ||
                branch_type.find("SimCalorimeterHitData") != std::string::npos) {
                names.push_back(branch_name);
                std::cout << "  ✓ MATCHED CALO: " << branch_name << " (type: " << branch_type << ")" << std::endl;
            }
        } else if (branch_pattern.find("CaloHitContribution") != std::string::npos) {
            // Look for vector<edm4hep::CaloHitContributionData> or similar
            if (branch_type.find("vector<edm4hep::CaloHitContributionData>") != std::string::npos ||
                branch_type.find("CaloHitContributionData") != std::string::npos) {
                names.push_back(branch_name);
                std::cout << "  ✓ MATCHED CONTRIB: " << branch_name << " (type: " << branch_type << ")" << std::endl;
            }
        }
    }
    
    std::cout << "=== Discovery Summary ===" << std::endl;
    std::cout << "Discovered " << names.size() << " collections matching pattern: " << branch_pattern << std::endl;
    for (const auto& name : names) {
        std::cout << "  - " << name << std::endl;
    }
    std::cout << "=========================" << std::endl;
    
    return names;
}
