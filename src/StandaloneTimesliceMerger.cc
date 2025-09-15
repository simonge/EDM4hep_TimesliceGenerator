#include "StandaloneTimesliceMerger.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <memory>
#include <TBranch.h>
#include <TObjArray.h>

void MergedCollections::clear() {
    // Use clear() but preserve capacity to avoid repeated memory allocations
    mcparticles.clear();
    event_headers.clear();
    event_header_weights.clear();
    sub_event_headers.clear();
    
    for (auto& [name, vec] : tracker_hits) {
        vec.clear();
    }
    for (auto& [name, vec] : calo_hits) {
        vec.clear();
    }
    for (auto& [name, vec] : calo_contributions) {
        vec.clear();
    }
    for (auto& [name, vec] : tracker_hit_particle_refs) {
        vec.clear();
    }
    for (auto& [name, vec] : calo_contrib_particle_refs) {
        vec.clear();
    }
    
    mcparticle_parents_refs.clear();
    mcparticle_children_refs.clear();
    
    for (auto& [name, vec] : calo_hit_contributions_refs) {
        vec.clear();
    }
    
    // Clear GP branches - they can be repopulated for each event
    for (auto& [name, vec] : gp_key_branches) {
        vec.clear();
    }
    gp_int_values.clear();
    gp_float_values.clear();
    gp_double_values.clear();
    gp_string_values.clear();
    
    // Note: Using clear() instead of shrink_to_fit() preserves capacity
    // This avoids repeated memory allocations for vectors that will grow again
}

StandaloneTimesliceMerger::StandaloneTimesliceMerger(const MergerConfig& config)
    : m_config(config), gen(rd()), events_generated(0) {
    
}

void StandaloneTimesliceMerger::run() {
    std::cout << "Starting timeslice merger (object-oriented approach)..." << std::endl;
    std::cout << "Sources: " << m_config.sources.size() << std::endl;
    std::cout << "Output file: " << m_config.output_file << std::endl;
    std::cout << "Max events: " << m_config.max_events << std::endl;
    std::cout << "Timeslice duration: " << m_config.time_slice_duration << std::endl;
    
    // Open output file and create tree with optimizations
    auto output_file = std::make_unique<TFile>(m_config.output_file.c_str(), "RECREATE");
    if (!output_file || output_file->IsZombie()) {
        throw std::runtime_error("Could not create output file: " + m_config.output_file);
    }
    
    // Set ROOT I/O optimizations
    output_file->SetCompressionLevel(1); // Fast compression (1-9, where 1=fast, 9=best compression)
    
    TTree* output_tree = new TTree("events", "Merged timeslices");
    
    // Optimize TTree settings for performance
    output_tree->SetMaxTreeSize(10000000000LL); // 10GB max tree size to reduce file splits
    output_tree->SetCacheSize(50000000); // 50MB cache for better I/O performance
    output_tree->SetAutoFlush(-50000000); // Auto-flush every 50MB for better I/O patterns
    
    data_sources_ = initializeDataSources();
    setupOutputTree(output_tree);

    // Copy podio_metadata tree from first source file to output file
    copyPodioMetadata(data_sources_, output_file);

    std::cout << "Processing " << m_config.max_events << " timeslices..." << std::endl;

    while (events_generated < m_config.max_events) {
        // Update number of events needed per source
        if (!updateInputNEvents(data_sources_)) {
            std::cout << "Reached end of input data, stopping at " << events_generated << " timeslices" << std::endl;
            break;
        }
        createMergedTimeslice(data_sources_, output_file, output_tree);

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

std::vector<std::unique_ptr<DataSource>> StandaloneTimesliceMerger::initializeDataSources() {
    std::vector<std::unique_ptr<DataSource>> data_sources;
    data_sources.reserve(m_config.sources.size());
    
    // Create DataSource objects
    for (size_t source_idx = 0; source_idx < m_config.sources.size(); ++source_idx) {
        auto data_source = std::make_unique<DataSource>(m_config.sources[source_idx], source_idx);
        data_sources.push_back(std::move(data_source));
    }
    
    // Use first source to discover collection names
    if (!data_sources.empty() && !m_config.sources[0].input_files.empty()) {
        // Discover collection names using the first source
        tracker_collection_names_ = discoverCollectionNames(*data_sources[0], "SimTrackerHit");
        calo_collection_names_ = discoverCollectionNames(*data_sources[0], "SimCalorimeterHit");
        gp_collection_names_ = discoverGPBranches(*data_sources[0]);
        
        std::cout << "Global collection names discovered:" << std::endl;
        std::cout << "  Tracker: ";
        for (const auto& name : tracker_collection_names_) std::cout << name << " ";
        std::cout << std::endl;
        std::cout << "  Calo: ";
        for (const auto& name : calo_collection_names_) std::cout << name << " ";
        std::cout << std::endl;
        std::cout << "  GP: ";
        for (const auto& name : gp_collection_names_) std::cout << name << " ";
        std::cout << std::endl;
        
        // Initialize all data sources with discovered collection names
        for (auto& data_source : data_sources) {
            data_source->initialize(tracker_collection_names_, calo_collection_names_, gp_collection_names_);
        }
    }
    
    return data_sources;
}

bool StandaloneTimesliceMerger::updateInputNEvents(std::vector<std::unique_ptr<DataSource>>& sources) {
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
            data_source->setEntriesNeeded((n == 0) ? 1 : n);
        }
        
        // Check enough events are available in this source
        if (!data_source->hasMoreEntries()) {
            std::cout << "Not enough events available in source " << config.name << std::endl;
            return false;
        }
    }

    return true;
}

void StandaloneTimesliceMerger::createMergedTimeslice(std::vector<std::unique_ptr<DataSource>>& sources, 
                                                     std::unique_ptr<TFile>& output_file, 
                                                     TTree* output_tree) {
    
    // Clear all merged collections for new timeslice
    merged_collections_.clear();


    int totalEventsConsumed = 0;

    // Loop over sources and read needed events
    for(auto& data_source : sources) {
        const auto& config = data_source->getConfig();
        int sourceEventsConsumed = 0;

        for (size_t i = 0; i < data_source->getEntriesNeeded(); ++i) {
            // Calculate particle index offset for this event
            size_t particle_index_offset = merged_collections_.mcparticles.size();
            
            // Load the event data from this source
            data_source->loadEvent(data_source->getCurrentEntryIndex());
            
            // Process MCParticles - use move semantics to avoid copying
            auto& processed_particles = data_source->processMCParticles(particle_index_offset,
                                                                       m_config.time_slice_duration,
                                                                       m_config.bunch_crossing_period,
                                                                       gen,totalEventsConsumed);
            merged_collections_.mcparticles.insert(merged_collections_.mcparticles.end(), 
                                                  std::make_move_iterator(processed_particles.begin()), 
                                                  std::make_move_iterator(processed_particles.end()));
            
            // Process MCParticle references - use move semantics
            std::string parent_ref_branch_name = "_MCParticles_parents";
            auto& processed_parents = data_source->processObjectID(parent_ref_branch_name, particle_index_offset,totalEventsConsumed);
            merged_collections_.mcparticle_parents_refs.insert(merged_collections_.mcparticle_parents_refs.end(),
                                                              std::make_move_iterator(processed_parents.begin()), 
                                                              std::make_move_iterator(processed_parents.end()));

            std::string children_ref_branch_name = "_MCParticles_daughters";
            auto& processed_children = data_source->processObjectID(children_ref_branch_name, particle_index_offset,totalEventsConsumed);
            merged_collections_.mcparticle_children_refs.insert(merged_collections_.mcparticle_children_refs.end(),
                                                               std::make_move_iterator(processed_children.begin()), 
                                                               std::make_move_iterator(processed_children.end()));
            
            // Process SubEventHeaders for non-merged sources to track which MCParticles came from this source
            if (!config.already_merged) {
                // Create a SubEventHeader for this source/event combination
                edm4hep::EventHeaderData sub_header;
                sub_header.eventNumber = totalEventsConsumed; // Use total event count as unique identifier
                sub_header.runNumber = data_source->getSourceIndex(); // Use source index to identify the source
                sub_header.timeStamp = totalEventsConsumed; // Keep consistent with main header
                
                // Store range of MCParticles from this source event
                // The weight field can be used to store the starting index of particles from this event
                sub_header.weight = static_cast<float>(particle_index_offset);
                
                merged_collections_.sub_event_headers.push_back(sub_header);
                
                std::cout << "Added SubEventHeader: event=" << sub_header.eventNumber 
                          << ", source=" << sub_header.runNumber 
                          << ", particle_offset=" << static_cast<size_t>(sub_header.weight) << std::endl;
            } else {
                // For already merged sources, try to process existing SubEventHeaders if available
                auto& existing_sub_headers = data_source->processEventHeaders("SubEventHeaders");
                for (auto& sub_header : existing_sub_headers) {
                    // Adjust the weight (particle index) to account for the current offset
                    float original_offset = sub_header.weight;
                    sub_header.weight += static_cast<float>(particle_index_offset);
                    merged_collections_.sub_event_headers.push_back(sub_header);
                    
                    std::cout << "Processed existing SubEventHeader: event=" << sub_header.eventNumber 
                              << ", source=" << sub_header.runNumber 
                              << ", original_offset=" << static_cast<size_t>(original_offset)
                              << ", new_offset=" << static_cast<size_t>(sub_header.weight) << std::endl;
                }
            }
            
            // Process tracker hits - use move semantics to avoid copying
            for (const auto& name : tracker_collection_names_) {
                auto& processed_hits = data_source->processTrackerHits(name, particle_index_offset,totalEventsConsumed);
                merged_collections_.tracker_hits[name].insert(merged_collections_.tracker_hits[name].end(),
                                                             std::make_move_iterator(processed_hits.begin()), 
                                                             std::make_move_iterator(processed_hits.end()));

                std::string ref_branch_name = "_" + name + "_particle";
                auto& processed_refs = data_source->processObjectID(ref_branch_name, particle_index_offset,totalEventsConsumed);
                merged_collections_.tracker_hit_particle_refs[name].insert(merged_collections_.tracker_hit_particle_refs[name].end(),
                                                                          std::make_move_iterator(processed_refs.begin()), 
                                                                          std::make_move_iterator(processed_refs.end()));
            }
            
            // Process calorimeter hits - use move semantics to avoid copying
            for (const auto& name : calo_collection_names_) {
                size_t existing_contrib_size = merged_collections_.calo_contributions[name].size();

                auto& processed_hits = data_source->processCaloHits(name, existing_contrib_size,totalEventsConsumed);
                merged_collections_.calo_hits[name].insert(merged_collections_.calo_hits[name].end(),
                                                          std::make_move_iterator(processed_hits.begin()), 
                                                          std::make_move_iterator(processed_hits.end()));
                
                std::string ref_branch_name = "_" + name + "_contributions";
                auto& processed_contrib_refs = data_source->processObjectID(ref_branch_name, existing_contrib_size,totalEventsConsumed);
                merged_collections_.calo_hit_contributions_refs[name].insert(merged_collections_.calo_hit_contributions_refs[name].end(),
                                                                            std::make_move_iterator(processed_contrib_refs.begin()), 
                                                                            std::make_move_iterator(processed_contrib_refs.end()));
                
                // Process contributions
                std::string contrib_branch_name = name + "Contributions";
                auto& processed_contribs = data_source->processCaloContributions(contrib_branch_name, particle_index_offset,totalEventsConsumed);
                merged_collections_.calo_contributions[name].insert(merged_collections_.calo_contributions[name].end(),
                                                                   std::make_move_iterator(processed_contribs.begin()), 
                                                                   std::make_move_iterator(processed_contribs.end()));
                
                std::string ref_branch_name_contrib = "_" + contrib_branch_name + "_particle";
                auto& processed_contrib_particle_refs = data_source->processObjectID(ref_branch_name_contrib, particle_index_offset,totalEventsConsumed);
                merged_collections_.calo_contrib_particle_refs[name].insert(merged_collections_.calo_contrib_particle_refs[name].end(),
                                                                           std::make_move_iterator(processed_contrib_particle_refs.begin()), 
                                                                           std::make_move_iterator(processed_contrib_particle_refs.end()));
            }
            
            // Process GP (Global Parameter) branches - only from first event of first source
            if (totalEventsConsumed == 0 && !gp_collection_names_.empty()) {
                std::cout << "Processing GP branches from first event..." << std::endl;
                
                // Process GP key branches - use move semantics to avoid copying
                for (const auto& name : gp_collection_names_) {
                    auto& gp_keys = data_source->processGPBranch(name);
                    merged_collections_.gp_key_branches[name] = std::move(gp_keys); // Move GP key data
                    // std::cout << "GP key branch " << name << ": " << gp_keys.size() << " entries" << std::endl;
                }
                
                // Process GP value branches - use move semantics to avoid copying
                auto& gp_int_values = data_source->processGPIntValues();
                merged_collections_.gp_int_values = std::move(gp_int_values);
                // std::cout << "GP int values: " << gp_int_values.size() << " vectors" << std::endl;
                
                auto& gp_float_values = data_source->processGPFloatValues();
                merged_collections_.gp_float_values = std::move(gp_float_values);
                // std::cout << "GP float values: " << gp_float_values.size() << " vectors" << std::endl;
                
                auto& gp_double_values = data_source->processGPDoubleValues();
                merged_collections_.gp_double_values = std::move(gp_double_values);
                // std::cout << "GP double values: " << gp_double_values.size() << " vectors" << std::endl;
                
                auto& gp_string_values = data_source->processGPStringValues();
                merged_collections_.gp_string_values = std::move(gp_string_values);
                // std::cout << "GP string values: " << gp_string_values.size() << " vectors" << std::endl;
            }

            data_source->setCurrentEntryIndex(data_source->getCurrentEntryIndex() + 1);
            sourceEventsConsumed++;
            totalEventsConsumed++;
        }

        std::cout << "Merged " << sourceEventsConsumed << " events, totalling " 
                  << data_source->getCurrentEntryIndex() << " from source " << config.name << std::endl;
    }

    // Create main timeslice header
    edm4hep::EventHeaderData header;
    header.eventNumber = events_generated;
    header.runNumber = 0;
    header.timeStamp = events_generated;
    merged_collections_.event_headers.push_back(header);

    // Write merged data to output tree
    writeTimesliceToTree(output_tree);
}

void StandaloneTimesliceMerger::setupOutputTree(TTree* tree) {
    // Directly create all required branches, no sorting or BranchInfo struct
    auto eventHeaderBranch = tree->Branch("EventHeader", &merged_collections_.event_headers);
    eventHeaderBranch->SetBasketSize(32000); // Optimize basket size for I/O
    
    auto eventHeaderWeightsBranch = tree->Branch("_EventHeader_weights", &merged_collections_.event_header_weights);
    eventHeaderWeightsBranch->SetBasketSize(32000);
    
    // Enable SubEventHeaders branch to track which MCParticles were associated with each source
    auto subEventHeaderBranch = tree->Branch("SubEventHeaders", &merged_collections_.sub_event_headers);
    subEventHeaderBranch->SetBasketSize(32000);

    auto mcParticlesBranch = tree->Branch("MCParticles", &merged_collections_.mcparticles);
    mcParticlesBranch->SetBasketSize(64000); // Larger basket for main physics data
    
    auto mcDaughtersBranch = tree->Branch("_MCParticles_daughters", &merged_collections_.mcparticle_children_refs);
    mcDaughtersBranch->SetBasketSize(32000);
    
    auto mcParentsBranch = tree->Branch("_MCParticles_parents", &merged_collections_.mcparticle_parents_refs);
    mcParentsBranch->SetBasketSize(32000);

    // Tracker collections and their references
    for (const auto& name : tracker_collection_names_) {
        auto trackerBranch = tree->Branch(name.c_str(), &merged_collections_.tracker_hits[name]);
        trackerBranch->SetBasketSize(64000); // Large basket for hit data
        
        std::string ref_name = "_" + name + "_particle";
        auto trackerRefBranch = tree->Branch(ref_name.c_str(), &merged_collections_.tracker_hit_particle_refs[name]);
        trackerRefBranch->SetBasketSize(32000);
    }

    // Calorimeter collections and their references
    for (const auto& name : calo_collection_names_) {
        auto caloBranch = tree->Branch(name.c_str(), &merged_collections_.calo_hits[name]);
        caloBranch->SetBasketSize(64000); // Large basket for hit data
        
        std::string ref_name = "_" + name + "_contributions";
        auto caloRefBranch = tree->Branch(ref_name.c_str(), &merged_collections_.calo_hit_contributions_refs[name]);
        caloRefBranch->SetBasketSize(32000);
        
        std::string contrib_name = name + "Contributions";
        auto contribBranch = tree->Branch(contrib_name.c_str(), &merged_collections_.calo_contributions[name]);
        contribBranch->SetBasketSize(64000); // Large basket for contribution data
        
        std::string ref_name_contrib = "_" + contrib_name + "_particle";
        auto contribRefBranch = tree->Branch(ref_name_contrib.c_str(), &merged_collections_.calo_contrib_particle_refs[name]);
        contribRefBranch->SetBasketSize(32000);
    }
    
    // GP (Global Parameter) branches - keys and values
    for (const auto& name : gp_collection_names_) {
        auto gpBranch = tree->Branch(name.c_str(), &merged_collections_.gp_key_branches[name]);
        gpBranch->SetBasketSize(16000); // Smaller basket for GP data
    }
    
    // GP value branches (fixed names) - smaller baskets as they're written once per timeslice
    auto gpIntBranch = tree->Branch("GPIntValues", &merged_collections_.gp_int_values);
    gpIntBranch->SetBasketSize(16000);
    
    auto gpFloatBranch = tree->Branch("GPFloatValues", &merged_collections_.gp_float_values);
    gpFloatBranch->SetBasketSize(16000);
    
    auto gpDoubleBranch = tree->Branch("GPDoubleValues", &merged_collections_.gp_double_values);
    gpDoubleBranch->SetBasketSize(16000);
    
    auto gpStringBranch = tree->Branch("GPStringValues", &merged_collections_.gp_string_values);
    gpStringBranch->SetBasketSize(16000);
    
    // Print the number of created branches
    std::cout << "Total branches created: " << tree->GetListOfBranches()->GetEntries() << std::endl;
    std::cout << "Created branches for all required collections with optimized basket sizes" << std::endl;
}

void StandaloneTimesliceMerger::writeTimesliceToTree(TTree* tree) {
    // Debug: show sizes of merged vectors before writing
    std::cout << "=== Writing timeslice ===" << std::endl;
    std::cout << "  Event headers: " << merged_collections_.event_headers.size() << std::endl;
    std::cout << "  Sub event headers: " << merged_collections_.sub_event_headers.size() << std::endl;
    std::cout << "  MCParticles: " << merged_collections_.mcparticles.size() << std::endl;
    std::cout << "  MCParticle parents: " << merged_collections_.mcparticle_parents_refs.size() << std::endl;
    std::cout << "  MCParticle daughters: " << merged_collections_.mcparticle_children_refs.size() << std::endl;

    // std::cout << "  Tracker collections (" << merged_collections_.tracker_hits.size() << "):" << std::endl;
    // for (const auto& [name, hits] : merged_collections_.tracker_hits) {
    //     std::cout << "    " << name << ": " << hits.size() << " hits, " 
    //               << merged_collections_.tracker_hit_particle_refs[name].size() << " particle refs" << std::endl;
    // }
    
    // std::cout << "  Calo collections (" << merged_collections_.calo_hits.size() << "):" << std::endl;
    // for (const auto& [name, hits] : merged_collections_.calo_hits) {
    //     size_t contrib_refs_size = 0;
    //     if (merged_collections_.calo_hit_contributions_refs.count(name)) {
    //         contrib_refs_size = merged_collections_.calo_hit_contributions_refs.at(name).size();
    //     }
    //     size_t contrib_size = 0;
    //     if (merged_collections_.calo_contributions.count(name)) {
    //         contrib_size = merged_collections_.calo_contributions.at(name).size();          
    //     }
    //     size_t contrib_particle_refs_size = 0;
    //     if (merged_collections_.calo_contrib_particle_refs.count(name)) {
    //         contrib_particle_refs_size = merged_collections_.calo_contrib_particle_refs.at(name).size();          
    //     }
    //     // std::cout << "    " << name << ": " << hits.size() << " hits, " 
    //     //           << contrib_refs_size << " contribution refs, "
    //     //           << contrib_size << " contributions, "
    //     //           << contrib_particle_refs_size << " contrib particle refs" << std::endl;
    // }
    
    // Fill the tree with current merged data
    tree->Fill();
    std::cout << "=== Timeslice written ===" << std::endl;
}

std::vector<std::string> StandaloneTimesliceMerger::discoverCollectionNames(DataSource& source, const std::string& branch_pattern) {
    std::vector<std::string> names;
    
    // We need to access the internal TChain from DataSource
    // For now, we'll create a temporary chain to discover collection names
    if (source.getConfig().input_files.empty()) {
        std::cout << "Warning: No input files in source" << std::endl;
        return names;
    }
    
    auto temp_chain = std::make_unique<TChain>(source.getConfig().tree_name.c_str());
    temp_chain->Add(source.getConfig().input_files[0].c_str());
    
    // Get list of branches from the TChain
    TObjArray* branches = temp_chain->GetListOfBranches();
    if (!branches) {
        std::cout << "Warning: No branches found in source" << std::endl;
        return names;
    }
    
    std::cout << "=== Branch Discovery for pattern: " << branch_pattern << " ===" << std::endl;
    std::cout << "Total branches in chain: " << branches->GetEntries() << std::endl;   
    
    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = (TBranch*)branches->At(i);
        if (!branch) continue;
        
        std::string branch_name = branch->GetName();
        std::string branch_type = "";
        
        // Get the branch data type
        TClass* expectedClass2 = nullptr;
        EDataType expectedType2;
        if (branch->GetExpectedType(expectedClass2, expectedType2) == 0 && expectedClass2 && expectedClass2->GetName()) {
            branch_type = expectedClass2->GetName();
        }
        
        // Skip branches that start with "_" for now - these are ObjectID references
        if (branch_name.find("_") == 0) continue;
        // if branch type doesnt match pattern continue
        if (branch_type.find(branch_pattern) == std::string::npos) continue;
        
        // Look for collections based on data type
        if (branch_type == "vector<edm4hep::SimTrackerHitData>") {
            names.push_back(branch_name);
            // std::cout << "  ✓ MATCHED TRACKER: " << branch_name << " (type: " << branch_type << ")" << std::endl;
        } else if (branch_type == "vector<edm4hep::SimCalorimeterHitData>") {
            names.push_back(branch_name);
            // std::cout << "  ✓ MATCHED CALO: " << branch_name << " (type: " << branch_type << ")" << std::endl;            
        }
    }
    
    // std::cout << "=== Discovery Summary ===" << std::endl;
    // std::cout << "Discovered " << names.size() << " collections matching pattern: " << branch_pattern << std::endl;
    // for (const auto& name : names) {
    //     std::cout << "  - " << name << std::endl;
    // }
    // std::cout << "=========================" << std::endl;
    
    return names;
}

std::vector<std::string> StandaloneTimesliceMerger::discoverGPBranches(DataSource& source) {
    std::vector<std::string> names;
    
    // We need to access the internal TChain from DataSource
    // For now, we'll create a temporary chain to discover GP branch names
    if (source.getConfig().input_files.empty()) {
        std::cout << "Warning: No input files in source for GP discovery" << std::endl;
        return names;
    }
    
    auto temp_chain = std::make_unique<TChain>(source.getConfig().tree_name.c_str());
    temp_chain->Add(source.getConfig().input_files[0].c_str());
    
    // Get list of branches from the TChain
    TObjArray* branches = temp_chain->GetListOfBranches();
    if (!branches) {
        std::cout << "Warning: No branches found in source for GP discovery" << std::endl;
        return names;
    }
    
    std::cout << "=== GP Branch Discovery ===" << std::endl;
    
    // GP branch patterns to look for
    std::vector<std::string> gp_patterns = {"GPIntKeys", "GPFloatKeys", "GPStringKeys", "GPDoubleKeys"};

    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = (TBranch*)branches->At(i);
        if (!branch) continue;
        
        std::string branch_name = branch->GetName();
        
        // Check if this branch matches any GP pattern
        for (const auto& pattern : gp_patterns) {
            if (branch_name.find(pattern) == 0) {  // Branch name starts with pattern
                // std::cout << "  ✓ FOUND GP BRANCH: " << branch_name << std::endl;
                names.push_back(branch_name);
                break;  // Don't check other patterns for this branch
            }
        }
    }
    
    std::cout << "Discovered " << names.size() << " GP branches" << std::endl;
    return names;
}

void StandaloneTimesliceMerger::copyPodioMetadata(std::vector<std::unique_ptr<DataSource>>& sources, std::unique_ptr<TFile>& output_file) {
    if (sources.empty()) {
        std::cout << "Warning: No input sources available for podio_metadata copying" << std::endl;
        return;
    }
    
    // Get the first source file to copy podio_metadata from
    const auto& first_source = sources[0];
    if (!first_source->getConfig().input_files.empty()) {
        std::string first_file = first_source->getConfig().input_files[0];
        std::cout << "Attempting to copy podio_metadata from: " << first_file << std::endl;
        
        // Open the first source file directly to access metadata trees
        auto source_file = std::make_unique<TFile>(first_file.c_str(), "READ");
        if (!source_file || source_file->IsZombie()) {
            std::cout << "Warning: Could not open source file for metadata copying: " << first_file << std::endl;
            return;
        }
        
        output_file->cd();
        
        // List of metadata trees to copy
        std::vector<std::string> metadata_trees = {"podio_metadata", "runs", "meta", "metadata"};
        
        // Copy each metadata tree if it exists, with special handling for podio_metadata
        for (const auto& tree_name : metadata_trees) {
            TTree* metadata_tree = dynamic_cast<TTree*>(source_file->Get(tree_name.c_str()));
            if (metadata_tree) {
                std::cout << "Found " << tree_name << " tree, copying to output..." << std::endl;
                
                if (tree_name == "podio_metadata") {
                    // Special handling for podio_metadata to include SubEventHeaders
                    copyAndUpdatePodioMetadataTree(metadata_tree, output_file.get());
                } else {
                    // Clone the tree structure and data normally for other metadata trees
                    TTree* output_metadata = metadata_tree->CloneTree(-1, "fast");
                    if (output_metadata) {
                        output_metadata->Write();
                        // std::cout << "Successfully copied " << tree_name << " tree" << std::endl;
                    } else {
                        std::cout << "Warning: Failed to clone " << tree_name << " tree" << std::endl;
                    }
                }
            } else {
                std::cout << "Info: No " << tree_name << " tree found in source file" << std::endl;
            }
        }
        
        source_file->Close();
    }
}

std::string StandaloneTimesliceMerger::getCorrespondingContributionCollection(const std::string& calo_collection_name) const {
    // Add "Contributions" suffix to get the contribution collection name
    return calo_collection_name + "Contributions";
}

std::string StandaloneTimesliceMerger::getCorrespondingCaloCollection(const std::string& contrib_collection_name) const {
    // Remove "Contributions" suffix if present
    std::string base = contrib_collection_name;
    if (base.length() > 13 && base.substr(base.length() - 13) == "Contributions") {
        base = base.substr(0, base.length() - 13);
    }
    return base;
}

void StandaloneTimesliceMerger::copyAndUpdatePodioMetadataTree(TTree* source_metadata_tree, TFile* output_file) {
    if (!source_metadata_tree || !output_file) {
        std::cout << "Warning: Invalid metadata tree or output file for podio_metadata updating" << std::endl;
        return;
    }
    
    // First try to clone the tree normally
    TTree* output_metadata = source_metadata_tree->CloneTree(-1, "fast");
    if (!output_metadata) {
        std::cout << "Warning: Failed to clone podio_metadata tree" << std::endl;
        return;
    }
    
    // Check if the tree has collection names that we need to update
    // This is a best-effort approach since podio metadata structure can vary
    TObjArray* branches = output_metadata->GetListOfBranches();
    bool collection_names_found = false;
    
    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = dynamic_cast<TBranch*>(branches->At(i));
        if (branch) {
            std::string branch_name = branch->GetName();
            // Look for branches that might contain collection names
            // Common names in podio metadata: "CollectionIDs", "collection_names", etc.
            if (branch_name.find("ollection") != std::string::npos || 
                branch_name.find("Collection") != std::string::npos) {
                std::cout << "Found potential collection names branch: " << branch_name << std::endl;
                collection_names_found = true;
                // Note: Modifying the cloned tree's data is complex and depends on the exact
                // structure of the podio metadata. For now, we note that SubEventHeaders
                // should be included but don't modify the existing metadata structure.
                break;
            }
        }
    }
    
    if (collection_names_found) {
        std::cout << "Info: podio_metadata contains collection information. " 
                  << "SubEventHeaders have been added to the output but metadata tree "
                  << "structure is preserved as-is for compatibility." << std::endl;
    } else {
        std::cout << "Info: No collection names found in podio_metadata, copying as-is." << std::endl;
    }
    
    // Write the (possibly updated) metadata tree
    output_metadata->Write();
    std::cout << "Successfully copied podio_metadata tree" << std::endl;
}