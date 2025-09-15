#include "StandaloneTimesliceMerger.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <memory>
#include <TBranch.h>
#include <TObjArray.h>

void MergedCollections::clear() {
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
    
    // Open output file and create tree
    auto output_file = std::make_unique<TFile>(m_config.output_file.c_str(), "RECREATE");
    if (!output_file || output_file->IsZombie()) {
        throw std::runtime_error("Could not create output file: " + m_config.output_file);
    }
    
    TTree* output_tree = new TTree("events", "Merged timeslices");
    
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
            
            // Process MCParticles
            auto& processed_particles = data_source->processMCParticles(particle_index_offset,
                                                                       m_config.time_slice_duration,
                                                                       m_config.bunch_crossing_period,
                                                                       gen);
            merged_collections_.mcparticles.insert(merged_collections_.mcparticles.end(), 
                                                  processed_particles.begin(), processed_particles.end());
            
            // Process MCParticle references
            std::string parent_ref_branch_name = "_MCParticles_parents";
            auto& processed_parents = data_source->processObjectID(parent_ref_branch_name, particle_index_offset);
            merged_collections_.mcparticle_parents_refs.insert(merged_collections_.mcparticle_parents_refs.end(),
                                                              processed_parents.begin(), processed_parents.end());

            std::string children_ref_branch_name = "_MCParticles_daughters";
            auto& processed_children = data_source->processObjectID(children_ref_branch_name, particle_index_offset);
            merged_collections_.mcparticle_children_refs.insert(merged_collections_.mcparticle_children_refs.end(),
                                                               processed_children.begin(), processed_children.end());
            
            // Process tracker hits
            for (const auto& name : tracker_collection_names_) {
                auto& processed_hits = data_source->processTrackerHits(name, particle_index_offset); // time_offset handled in processMCParticles
                merged_collections_.tracker_hits[name].insert(merged_collections_.tracker_hits[name].end(),
                                                             processed_hits.begin(), processed_hits.end());

                std::string ref_branch_name = "_" + name + "_particle";
                auto& processed_refs = data_source->processObjectID(ref_branch_name, particle_index_offset);
                merged_collections_.tracker_hit_particle_refs[name].insert(merged_collections_.tracker_hit_particle_refs[name].end(),
                                                                          processed_refs.begin(), processed_refs.end());
            }
            
            // Process calorimeter hits
            for (const auto& name : calo_collection_names_) {
                size_t existing_contrib_size = merged_collections_.calo_contributions[name].size();
                
                auto& processed_hits = data_source->processCaloHits(name, particle_index_offset); // time_offset handled in processMCParticles
                merged_collections_.calo_hits[name].insert(merged_collections_.calo_hits[name].end(),
                                                          processed_hits.begin(), processed_hits.end());
                
                std::string ref_branch_name = "_" + name + "_contributions";
                auto& processed_contrib_refs = data_source->processObjectID(ref_branch_name, existing_contrib_size);
                merged_collections_.calo_hit_contributions_refs[name].insert(merged_collections_.calo_hit_contributions_refs[name].end(),
                                                                            processed_contrib_refs.begin(), processed_contrib_refs.end());
                
                // Process contributions
                std::string contrib_branch_name = name + "Contributions";
                auto& processed_contribs = data_source->processCaloContributions(contrib_branch_name, particle_index_offset);
                merged_collections_.calo_contributions[name].insert(merged_collections_.calo_contributions[name].end(),
                                                                   processed_contribs.begin(), processed_contribs.end());
                
                std::string ref_branch_name_contrib = "_" + contrib_branch_name + "_particle";
                auto& processed_contrib_particle_refs = data_source->processObjectID(ref_branch_name_contrib, particle_index_offset);
                merged_collections_.calo_contrib_particle_refs[name].insert(merged_collections_.calo_contrib_particle_refs[name].end(),
                                                                           processed_contrib_particle_refs.begin(), processed_contrib_particle_refs.end());
            }
            
            // Process GP (Global Parameter) branches - only from first event of first source
            if (totalEventsConsumed == 0 && !gp_collection_names_.empty()) {
                std::cout << "Processing GP branches from first event..." << std::endl;
                
                // Process GP key branches
                for (const auto& name : gp_collection_names_) {
                    auto& gp_keys = data_source->processGPBranch(name);
                    merged_collections_.gp_key_branches[name] = gp_keys; // Copy all GP key data
                    // std::cout << "GP key branch " << name << ": " << gp_keys.size() << " entries" << std::endl;
                }
                
                // Process GP value branches
                auto& gp_int_values = data_source->processGPIntValues();
                merged_collections_.gp_int_values = gp_int_values;
                // std::cout << "GP int values: " << gp_int_values.size() << " vectors" << std::endl;
                
                auto& gp_float_values = data_source->processGPFloatValues();
                merged_collections_.gp_float_values = gp_float_values;
                // std::cout << "GP float values: " << gp_float_values.size() << " vectors" << std::endl;
                
                auto& gp_double_values = data_source->processGPDoubleValues();
                merged_collections_.gp_double_values = gp_double_values;
                // std::cout << "GP double values: " << gp_double_values.size() << " vectors" << std::endl;
                
                auto& gp_string_values = data_source->processGPStringValues();
                merged_collections_.gp_string_values = gp_string_values;
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
    tree->Branch("EventHeader", &merged_collections_.event_headers);
    tree->Branch("_EventHeader_weights", &merged_collections_.event_header_weights);
    // tree->Branch("SubEventHeaders", &merged_collections_.sub_event_headers); // Uncomment if needed

    tree->Branch("MCParticles", &merged_collections_.mcparticles);
    tree->Branch("_MCParticles_daughters", &merged_collections_.mcparticle_children_refs);
    tree->Branch("_MCParticles_parents", &merged_collections_.mcparticle_parents_refs);

    // Tracker collections and their references
    for (const auto& name : tracker_collection_names_) {
        tree->Branch(name.c_str(), &merged_collections_.tracker_hits[name]);
        std::string ref_name = "_" + name + "_particle";
        tree->Branch(ref_name.c_str(), &merged_collections_.tracker_hit_particle_refs[name]);
    }

    // Calorimeter collections and their references
    for (const auto& name : calo_collection_names_) {
        tree->Branch(name.c_str(), &merged_collections_.calo_hits[name]);
        std::string ref_name = "_" + name + "_contributions";
        tree->Branch(ref_name.c_str(), &merged_collections_.calo_hit_contributions_refs[name]);
        std::string contrib_name = name + "Contributions";
        tree->Branch(contrib_name.c_str(), &merged_collections_.calo_contributions[name]);
        std::string ref_name_contrib = "_" + contrib_name + "_particle";
        tree->Branch(ref_name_contrib.c_str(), &merged_collections_.calo_contrib_particle_refs[name]);
    }
    
    // GP (Global Parameter) branches - keys and values
    for (const auto& name : gp_collection_names_) {
        tree->Branch(name.c_str(), &merged_collections_.gp_key_branches[name]);
    }
    
    // GP value branches (fixed names)
    tree->Branch("GPIntValues", &merged_collections_.gp_int_values);
    tree->Branch("GPFloatValues", &merged_collections_.gp_float_values);
    tree->Branch("GPDoubleValues", &merged_collections_.gp_double_values);
    tree->Branch("GPStringValues", &merged_collections_.gp_string_values);
    
    // Print the number of created branches
    std::cout << "Total branches created: " << tree->GetListOfBranches()->GetEntries() << std::endl;
    std::cout << "Created branches for all required collections" << std::endl;
}

void StandaloneTimesliceMerger::writeTimesliceToTree(TTree* tree) {
    // Debug: show sizes of merged vectors before writing
    std::cout << "=== Writing timeslice ===" << std::endl;
    std::cout << "  MCParticles: " << merged_collections_.mcparticles.size() << std::endl;
    std::cout << "  MCParticle parents: " << merged_collections_.mcparticle_parents_refs.size() << std::endl;
    std::cout << "  MCParticle daughters: " << merged_collections_.mcparticle_children_refs.size() << std::endl;

    // std::cout << "  Tracker collections (" << merged_collections_.tracker_hits.size() << "):" << std::endl;
    // for (const auto& [name, hits] : merged_collections_.tracker_hits) {
    //     std::cout << "    " << name << ": " << hits.size() << " hits, " 
    //               << merged_collections_.tracker_hit_particle_refs[name].size() << " particle refs" << std::endl;
    // }
    
    // std::cout << "  Calo collections (" << merged_collections_.calo_hits.size() << "):" << std::endl;
    for (const auto& [name, hits] : merged_collections_.calo_hits) {
        size_t contrib_refs_size = 0;
        if (merged_collections_.calo_hit_contributions_refs.count(name)) {
            contrib_refs_size = merged_collections_.calo_hit_contributions_refs.at(name).size();
        }
        size_t contrib_size = 0;
        if (merged_collections_.calo_contributions.count(name)) {
            contrib_size = merged_collections_.calo_contributions.at(name).size();          
        }
        size_t contrib_particle_refs_size = 0;
        if (merged_collections_.calo_contrib_particle_refs.count(name)) {
            contrib_particle_refs_size = merged_collections_.calo_contrib_particle_refs.at(name).size();          
        }
        // std::cout << "    " << name << ": " << hits.size() << " hits, " 
        //           << contrib_refs_size << " contribution refs, "
        //           << contrib_size << " contributions, "
        //           << contrib_particle_refs_size << " contrib particle refs" << std::endl;
    }
    
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
        
        // Copy each metadata tree if it exists
        for (const auto& tree_name : metadata_trees) {
            TTree* metadata_tree = dynamic_cast<TTree*>(source_file->Get(tree_name.c_str()));
            if (metadata_tree) {
                std::cout << "Found " << tree_name << " tree, copying to output..." << std::endl;
                
                // Clone the tree structure and data
                TTree* output_metadata = metadata_tree->CloneTree(-1, "fast");
                if (output_metadata) {
                    output_metadata->Write();
                    // std::cout << "Successfully copied " << tree_name << " tree" << std::endl;
                } else {
                    std::cout << "Warning: Failed to clone " << tree_name << " tree" << std::endl;
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