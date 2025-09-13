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

// Initialize data sources and validate sources
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
        tracker_collection_names = discoverCollectionNames(*data_sources[0], "SimTrackerHit");
        calo_collection_names = discoverCollectionNames(*data_sources[0], "SimCalorimeterHit");
        calo_contrib_collection_names = discoverCollectionNames(*data_sources[0], "CaloHitContribution");
        
        std::cout << "Global collection names discovered:" << std::endl;
        std::cout << "  Tracker: ";
        for (const auto& name : tracker_collection_names) std::cout << name << " ";
        std::cout << std::endl;
        std::cout << "  Calo: ";
        for (const auto& name : calo_collection_names) std::cout << name << " ";
        std::cout << std::endl;
        std::cout << "  Calo contributions: ";
        for (const auto& name : calo_contrib_collection_names) std::cout << name << " ";
        std::cout << std::endl;
        
        // Initialize all data sources with discovered collection names
        for (auto& data_source : data_sources) {
            data_source->initialize(tracker_collection_names, calo_collection_names, calo_contrib_collection_names);
        }
    }
    
    return data_sources;
}

// Update number of events needed per source for next timeslice
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
    merged_mcparticle_parents_refs.clear();
    merged_mcparticle_children_refs.clear();
    for (auto& [name, vec] : merged_calo_hit_contributions_refs) {
        vec.clear();
    }

    int totalEventsConsumed = 0;

    // Loop over sources and read needed events
    for(auto& data_source : sources) {
        const auto& config = data_source->getConfig();
        int sourceEventsConsumed = 0;

        for (size_t i = 0; i < data_source->getEntriesNeeded(); ++i) {
            // Calculate particle index offset for this event
            size_t particle_index_offset = merged_mcparticles.size();
            
            // Read and merge event data from this source
            data_source->mergeEventData(data_source->getCurrentEntryIndex(), 
                                      particle_index_offset,
                                      merged_mcparticles,
                                      merged_tracker_hits,
                                      merged_calo_hits,
                                      merged_calo_contributions,
                                      merged_mcparticle_parents_refs,
                                      merged_mcparticle_children_refs,
                                      merged_tracker_hit_particle_refs,
                                      merged_calo_contrib_particle_refs,
                                      merged_calo_hit_contributions_refs);

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
    merged_event_headers.push_back(header);

    // Write merged data to output tree
    writeTimesliceToTree(output_tree);
}

void StandaloneTimesliceMerger::setupOutputTree(TTree* tree) {
    // Create a list of all collections and their reference branches in alphabetical order
    struct BranchInfo {
        std::string name;
        std::string type; // "main", "mcparticle", "tracker", "calo", "calocontrib", "header"
        bool is_reference;
        std::string ref_suffix;
    };
    
    std::vector<BranchInfo> branches_to_create;
    
    // Add fixed header branches first (these don't follow alphabetical ordering)
    branches_to_create.push_back({"EventHeader", "header", false, ""});
    branches_to_create.push_back({"SubEventHeaders", "header", false, ""});
    
    // Add MCParticles and its reference branches
    branches_to_create.push_back({"MCParticles", "mcparticle", false, ""});
    branches_to_create.push_back({"_MCParticles_daughters", "mcparticle", true, "daughters"});
    branches_to_create.push_back({"_MCParticles_parents", "mcparticle", true, "parents"});
    
    // Collect all other collections and sort them alphabetically
    std::vector<std::string> all_collections;
    
    // Add tracker collections
    for (const auto& name : tracker_collection_names) {
        all_collections.push_back(name + "|tracker");
    }
    
    // Add calo collections  
    for (const auto& name : calo_collection_names) {
        all_collections.push_back(name + "|calo");
    }
    
    // Add calo contribution collections (using original contribution collection names for the actual branch names)
    for (const auto& name : calo_contrib_collection_names) {
        all_collections.push_back(name + "|calocontrib");
    }
    
    // Sort alphabetically by collection name
    std::sort(all_collections.begin(), all_collections.end());
    
    // Create branch entries for sorted collections
    for (const auto& collection_with_type : all_collections) {
        size_t pos = collection_with_type.find("|");
        std::string collection_name = collection_with_type.substr(0, pos);
        std::string collection_type = collection_with_type.substr(pos + 1);
        
        // Add main collection branch
        branches_to_create.push_back({collection_name, collection_type, false, ""});
        
        // Add reference branches immediately after main branch
        if (collection_type == "tracker") {
            branches_to_create.push_back({"_" + collection_name + "_particle", collection_type, true, "particle"});
        } else if (collection_type == "calo") {
            branches_to_create.push_back({"_" + collection_name + "_contributions", collection_type, true, "contributions"});
        } else if (collection_type == "calocontrib") {
            branches_to_create.push_back({"_" + collection_name + "_particle", collection_type, true, "particle"});
        }
    }
    
    // Create all branches in the determined order
    for (const auto& branch : branches_to_create) {
        if (branch.type == "header") {
            if (branch.name == "EventHeader") {
                tree->Branch("EventHeader", &merged_event_headers);
            } //else if (branch.name == "SubEventHeaders") {
            //     tree->Branch("SubEventHeaders", &merged_sub_event_headers);
            // }
        } else if (branch.type == "mcparticle" && !branch.is_reference) {
            tree->Branch("MCParticles", &merged_mcparticles);
        } else if (branch.type == "mcparticle" && branch.is_reference) {
            if (branch.ref_suffix == "parents") {
                tree->Branch("_MCParticles_parents", &merged_mcparticle_parents_refs);
            } else if (branch.ref_suffix == "daughters") {
                tree->Branch("_MCParticles_daughters", &merged_mcparticle_children_refs);
            }
        } else if (branch.type == "tracker" && !branch.is_reference) {
            tree->Branch(branch.name.c_str(), &merged_tracker_hits[branch.name]);
        } else if (branch.type == "tracker" && branch.is_reference) {
            std::string collection_name = branch.name.substr(1, branch.name.length() - 10); // Remove "_" and "_particle"
            tree->Branch(branch.name.c_str(), &merged_tracker_hit_particle_refs[collection_name]);
        } else if (branch.type == "calo" && !branch.is_reference) {
            tree->Branch(branch.name.c_str(), &merged_calo_hits[branch.name]);
        } else if (branch.type == "calo" && branch.is_reference) {
            std::string collection_name = branch.name.substr(1, branch.name.length() - 15); // Remove "_" and "_contributions"
            tree->Branch(branch.name.c_str(), &merged_calo_hit_contributions_refs[collection_name]);
        } else if (branch.type == "calocontrib" && !branch.is_reference) {
            // Find the corresponding calo collection name for this contribution collection
            std::string corresponding_calo_name = "";
            for (const auto& calo_name : calo_collection_names) {
                if (getCorrespondingContributionCollection(calo_name) == branch.name) {
                    corresponding_calo_name = calo_name;
                    tree->Branch(branch.name.c_str(), &merged_calo_contributions[calo_name]);
                    break;
                }
            }
        } else if (branch.type == "calocontrib" && branch.is_reference) {
            // Find the corresponding calo collection name for this contribution collection
            std::string contrib_collection_name = branch.name.substr(1, branch.name.length() - 10); // Remove "_" and "_particle"
            std::string corresponding_calo_name = "";
            for (const auto& calo_name : calo_collection_names) {
                if (getCorrespondingContributionCollection(calo_name) == contrib_collection_name) {
                    corresponding_calo_name = calo_name;
                    tree->Branch(branch.name.c_str(), &merged_calo_contrib_particle_refs[calo_name]);
                    break;
                }
            }
        }
    }
    
    std::cout << "Created " << branches_to_create.size() << " branches in alphabetical order" << std::endl;
}

void StandaloneTimesliceMerger::writeTimesliceToTree(TTree* tree) {
    // Debug: show sizes of merged vectors before writing
    std::cout << "=== Writing timeslice ===" << std::endl;
    std::cout << "  MCParticles: " << merged_mcparticles.size() << std::endl;
    std::cout << "  MCParticle parents: " << merged_mcparticle_parents_refs.size() << std::endl;
    std::cout << "  MCParticle daughters: " << merged_mcparticle_children_refs.size() << std::endl;

    std::cout << "  Tracker collections (" << merged_tracker_hits.size() << "):" << std::endl;
    for (const auto& [name, hits] : merged_tracker_hits) {
        std::cout << "    " << name << ": " << hits.size() << " hits, " 
                  << merged_tracker_hit_particle_refs[name].size() << " particle refs" << std::endl;
    }
    
    std::cout << "  Calo collections (" << merged_calo_hits.size() << "):" << std::endl;
    for (const auto& [name, hits] : merged_calo_hits) {
        size_t contrib_refs_size = 0;
        if (merged_calo_hit_contributions_refs.count(name)) {
            contrib_refs_size = merged_calo_hit_contributions_refs.at(name).size();
        }
        std::cout << "    " << name << ": " << hits.size() << " hits, " 
                  << contrib_refs_size << " contribution refs" << std::endl;
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

std::vector<std::string> StandaloneTimesliceMerger::discoverCollectionNames(DataSource& source, const std::string& branch_pattern) {
    std::vector<std::string> names;
    
    // We need to access the internal TChain from DataSource
    // For now, we'll create a temporary chain to discover collection names
    // This is a temporary solution until we can access the chain from DataSource
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
    
    // List all branches for debugging
    for (int i = 0; i < branches->GetEntries() && i < 20; ++i) {  // Limit to first 20 for readability
        TBranch* branch = (TBranch*)branches->At(i);
        if (!branch) continue;
        std::string branch_name = branch->GetName();
        std::string branch_class_name = "";
        TClass* expectedClass = nullptr;
        EDataType expectedType;
        if (branch->GetExpectedType(expectedClass, expectedType) == 0 && expectedClass && expectedClass->GetName()) {
            branch_class_name = expectedClass->GetName();
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
        TClass* expectedClass2 = nullptr;
        EDataType expectedType2;
        if (branch->GetExpectedType(expectedClass2, expectedType2) == 0 && expectedClass2 && expectedClass2->GetName()) {
            branch_type = expectedClass2->GetName();
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
        
        // Open the first source file directly to access podio_metadata tree
        auto source_file = std::make_unique<TFile>(first_file.c_str(), "READ");
        if (!source_file || source_file->IsZombie()) {
            std::cout << "Warning: Could not open source file for metadata copying: " << first_file << std::endl;
            return;
        }
        
        // Look for podio_metadata tree
        TTree* metadata_tree = dynamic_cast<TTree*>(source_file->Get("podio_metadata"));
        if (metadata_tree) {
            std::cout << "Found podio_metadata tree, copying to output..." << std::endl;
            output_file->cd();
            
            // Clone the tree structure and data
            TTree* output_metadata = metadata_tree->CloneTree(-1, "fast");
            if (output_metadata) {
                output_metadata->Write();
                std::cout << "Successfully copied podio_metadata tree" << std::endl;
            } else {
                std::cout << "Warning: Failed to clone podio_metadata tree" << std::endl;
            }
        } else {
            std::cout << "Info: No podio_metadata tree found in source file" << std::endl;
        }
        
        source_file->Close();
    }
}

std::string StandaloneTimesliceMerger::getCorrespondingContributionCollection(const std::string& calo_collection_name) {
    // Try to find a matching contribution collection based on naming patterns
    // Common patterns:
    // EcalBarrelHits -> EcalBarrelContributions
    // HcalEndcapHits -> HcalEndcapContributions
    // SomeCaloHits -> SomeCaloContributions

    // Add "Contributions" suffix to get the contribution collection name
    return calo_collection_name + "Contributions";
}

std::string StandaloneTimesliceMerger::getCorrespondingCaloCollection(const std::string& contrib_collection_name) {
    // Remove "Contributions" suffix if present
    std::string base = contrib_collection_name;
    if (base.length() > 13 && base.substr(base.length() - 13) == "Contributions") {
        base = base.substr(0, base.length() - 13);
    }
    // Add "Hits" suffix to get the calorimeter hit collection name
    return base;
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
                        source_reader.mcparticle_branch = new std::vector<edm4hep::MCParticleData>();
                        int result = source_reader.chain->SetBranchAddress(coll_name.c_str(), &source_reader.mcparticle_branch);
                        if (result != 0) {
                            std::cout << "    ❌ Could not set branch address for " << coll_name << " (result: " << result << ")" << std::endl;
                        } else {
                            std::cout << "    ✓ Successfully set up " << coll_name << std::endl;
                        }
                        
                        // Also setup the parent and children reference branches
                        std::string parents_branch_name = "_MCParticles_parents";
                        std::string children_branch_name = "_MCParticles_daughters";
                        source_reader.mcparticle_parents_refs = new std::vector<podio::ObjectID>();
                        source_reader.mcparticle_children_refs = new std::vector<podio::ObjectID>();

                        result = source_reader.chain->SetBranchAddress(parents_branch_name.c_str(), &source_reader.mcparticle_parents_refs);
                        if (result != 0) {
                            std::cout << "    ⚠️  Could not set branch address for " << parents_branch_name << " (result: " << result << ")" << std::endl;
                            std::cout << "       This may be normal if the input file doesn't contain MCParticle relationship information" << std::endl;
                        } else {
                            std::cout << "    ✓ Successfully set up " << parents_branch_name << std::endl;
                        }

                        result = source_reader.chain->SetBranchAddress(children_branch_name.c_str(), &source_reader.mcparticle_children_refs);
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
                        
                        // Also setup the contributions reference branch
                        std::string contrib_branch_name = "_" + coll_name + "_contributions";
                        source_reader.calo_hit_contributions_refs[coll_name] = new std::vector<podio::ObjectID>();
                        result = source_reader.chain->SetBranchAddress(contrib_branch_name.c_str(), &source_reader.calo_hit_contributions_refs[coll_name]);
                        if (result != 0) {
                            std::cout << "    ⚠️  Could not set branch address for " << contrib_branch_name << " (result: " << result << ")" << std::endl;
                            std::cout << "       This may be normal if the input file doesn't contain calorimeter hit contribution relationships" << std::endl;
                        } else {
                            std::cout << "    ✓ Successfully set up contributions ref " << contrib_branch_name << std::endl;
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
    merged_mcparticle_parents_refs.clear();
    merged_mcparticle_children_refs.clear();
    for (auto& [name, vec] : merged_calo_hit_contributions_refs) {
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
    auto& particles = *source.mcparticle_branch;
    auto& parents_refs = *source.mcparticle_parents_refs;
    auto& children_refs = *source.mcparticle_children_refs;

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

    // Start the merging process
    if(!sourceConfig.already_merged && merged_mcparticles.empty()) {
        // If first source is already merged, just need to move data and not update times or references
        // Keep track of starting index for MCParticles from this event
        size_t particle_index_offset = merged_mcparticles.size();

        // Process MCParticles
        for (auto& particle : particles) {
            // edm4hep::MCParticleData new_particle = particle;
            particle.time += time_offset;
            // Update generator status offset
            particle.generatorStatus += sourceConfig.generator_status_offset;
            particle.parents_begin   += particle_index_offset;
            particle.parents_end     += particle_index_offset;
            particle.daughters_begin += particle_index_offset;
            particle.daughters_end   += particle_index_offset;
            // merged_mcparticles.push_back(particle);
        }

        
        // Process MCParticle parent-child relationships
        // Update parent references - adjust indices to account for particle offset
        for (auto& parent_ref : parents_refs) {
            parent_ref.index = particle_index_offset + parent_ref.index;
        }
        
        // Update children references - adjust indices to account for particle offset
        for (auto& child_ref : children_refs) {
            child_ref.index = particle_index_offset + child_ref.index;
        }


        // Process SubEventHeaders
        // if (sourceConfig.already_merged && source.event_header_branches.count("SubEventHeaders")) {
        //     const auto& sub_headers = *source.event_header_branches["SubEventHeaders"];
        //     for (const auto& header : sub_headers) {
        //         edm4hep::EventHeaderData new_header = header;
        //         new_header.eventNumber = merged_sub_event_headers.size();
        //         // Update timestamp to reflect new MCParticle index offset
        //         new_header.timeStamp = particle_index_offset + header.timeStamp;
        //         merged_sub_event_headers.push_back(new_header);
        //     }
        // } else {
        //     // Create new SubEventHeader for regular source
        //     edm4hep::EventHeaderData sub_header{};
        //     sub_header.eventNumber = merged_sub_event_headers.size();
        //     sub_header.runNumber = 0;
        //     sub_header.timeStamp = particle_index_offset; // index of first MCParticle for this sub-event
        //     sub_header.weight = time_offset; // time offset
            
        //     // Copy weights from EventHeader if available
        //     if (source.event_header_branches.count("EventHeader")) {
        //         const auto& event_headers = *source.event_header_branches["EventHeader"];
        //         if (!event_headers.empty()) {
        //             const auto& event_header = event_headers[0];
        //             // Note: EventHeaderData doesn't have weights vector, so we store time_offset in weight field
        //         }
        //     }
        //     merged_sub_event_headers.push_back(sub_header);
        // }



        // Process edm4hep::SimTrackerHits
        for (const auto& name : tracker_collection_names) {
            if (source.tracker_hit_branches.count(name) && source.tracker_hit_particle_refs.count(name)) {
                auto& hits = *source.tracker_hit_branches[name];
                auto& particle_refs = *source.tracker_hit_particle_refs[name];
                
                for (size_t i = 0; i < hits.size(); ++i) {
                    auto& hit = hits[i];
                    auto& particle_ref = particle_refs[i];
                    hit.time += time_offset;            
                    particle_ref.index = particle_index_offset + particle_ref.index;
                }
            }
        }

        // Process calorimeter hits and contributions together
        for (const auto& calo_name : calo_collection_names) {
            // Only process if the branches exist for this source
            if (!source.calo_hit_branches.count(calo_name)) {
                continue; // Skip this collection if not available in this source
            }

            //Get size of contributions before adding new ones
            size_t existing_contrib_size = merged_calo_contributions[calo_name].size();
                    
            auto& hits = *source.calo_hit_branches[calo_name];
            
            // Check if contribution references exist for this source
            if (source.calo_hit_contributions_refs.count(calo_name)) {
                auto& contributions_refs = *source.calo_hit_contributions_refs[calo_name];
                
                //process hits 
                for (size_t i = 0; i < hits.size(); ++i) {
                    auto& hit = hits[i];
                    hit.contributions_begin += existing_contrib_size;
                    hit.contributions_end   += existing_contrib_size;

                    if (i < contributions_refs.size()) {
                        auto& contributions_ref = contributions_refs[i];
                        contributions_ref.index = existing_contrib_size + contributions_ref.index;
                    }
                }
            } else {
                // Process hits without contribution references
                for (size_t i = 0; i < hits.size(); ++i) {
                    auto& hit = hits[i];
                    hit.contributions_begin += existing_contrib_size;
                    hit.contributions_end   += existing_contrib_size;
                }
            }
            
            // Find the corresponding contribution collection and process if available
            std::string corresponding_contrib_collection = getCorrespondingContributionCollection(calo_name);
            if (!corresponding_contrib_collection.empty() && 
                source.calo_contrib_branches.count(corresponding_contrib_collection) &&
                source.calo_contrib_particle_refs.count(corresponding_contrib_collection)) {
                
                auto& contribs = *source.calo_contrib_branches[corresponding_contrib_collection];
                auto& particle_refs = *source.calo_contrib_particle_refs[corresponding_contrib_collection];
                
                // Process contributions directly into merged vectors
                for (size_t i = 0; i < contribs.size(); ++i) {
                    auto& contrib = contribs[i];
                    auto& particle_ref = particle_refs[i];
                    
                    contrib.time += time_offset;
                    particle_ref.index = particle_index_offset + particle_ref.index;
                }
            }
        }
    }

    // Concatenate all of this events vectors into their merged vectors
    merged_mcparticles.insert(merged_mcparticles.end(), std::make_move_iterator(particles.begin()), std::make_move_iterator(particles.end()));
    merged_mcparticle_parents_refs.insert(merged_mcparticle_parents_refs.end(), std::make_move_iterator(parents_refs.begin()), std::make_move_iterator(parents_refs.end()));
    merged_mcparticle_children_refs.insert(merged_mcparticle_children_refs.end(), std::make_move_iterator(children_refs.begin()), std::make_move_iterator(children_refs.end()));

    for (const auto& name : tracker_collection_names) {
        if (source.tracker_hit_branches.count(name)) {
            const auto& hits = *source.tracker_hit_branches[name];
            merged_tracker_hits[name].insert(merged_tracker_hits[name].end(), std::make_move_iterator(hits.begin()), std::make_move_iterator(hits.end()));
        }
        if (source.tracker_hit_particle_refs.count(name)) {
            const auto& refs = *source.tracker_hit_particle_refs[name];
            merged_tracker_hit_particle_refs[name].insert(merged_tracker_hit_particle_refs[name].end(), std::make_move_iterator(refs.begin()), std::make_move_iterator(refs.end()));
        }
    }

    for (const auto& calo_name : calo_collection_names) {
        if (source.calo_hit_branches.count(calo_name)) {
            const auto& hits = *source.calo_hit_branches[calo_name];
            merged_calo_hits[calo_name].insert(merged_calo_hits[calo_name].end(), std::make_move_iterator(hits.begin()), std::make_move_iterator(hits.end()));
        }
        if (source.calo_hit_contributions_refs.count(calo_name)) {
            const auto& refs = *source.calo_hit_contributions_refs[calo_name];
            merged_calo_hit_contributions_refs[calo_name].insert(merged_calo_hit_contributions_refs[calo_name].end(), std::make_move_iterator(refs.begin()), std::make_move_iterator(refs.end()));
        }

        // Also merge contributions from corresponding contribution collection using proper key mapping
        std::string corresponding_contrib_collection = getCorrespondingContributionCollection(calo_name);
        if (!corresponding_contrib_collection.empty() && source.calo_contrib_branches.count(corresponding_contrib_collection)) {
            const auto& contribs = *source.calo_contrib_branches[corresponding_contrib_collection];
            merged_calo_contributions[calo_name].insert(merged_calo_contributions[calo_name].end(), std::make_move_iterator(contribs.begin()), std::make_move_iterator(contribs.end()));
        }
        if (!corresponding_contrib_collection.empty() && source.calo_contrib_particle_refs.count(corresponding_contrib_collection)) {
            const auto& refs = *source.calo_contrib_particle_refs[corresponding_contrib_collection];
            merged_calo_contrib_particle_refs[calo_name].insert(merged_calo_contrib_particle_refs[calo_name].end(), std::make_move_iterator(refs.begin()), std::make_move_iterator(refs.end()));
        }
    }

}

void StandaloneTimesliceMerger::setupOutputTree(TTree* tree) {
    // Create a list of all collections and their reference branches in alphabetical order
    struct BranchInfo {
        std::string name;
        std::string type; // "main", "mcparticle", "tracker", "calo", "calocontrib", "header"
        bool is_reference;
        std::string ref_suffix;
    };
    
    std::vector<BranchInfo> branches_to_create;
    
    // Add fixed header branches first (these don't follow alphabetical ordering)
    branches_to_create.push_back({"EventHeader", "header", false, ""});
    branches_to_create.push_back({"SubEventHeaders", "header", false, ""});
    
    // Add MCParticles and its reference branches
    branches_to_create.push_back({"MCParticles", "mcparticle", false, ""});
    branches_to_create.push_back({"_MCParticles_daughters", "mcparticle", true, "daughters"});
    branches_to_create.push_back({"_MCParticles_parents", "mcparticle", true, "parents"});
    
    // Collect all other collections and sort them alphabetically
    std::vector<std::string> all_collections;
    
    // Add tracker collections
    for (const auto& name : tracker_collection_names) {
        all_collections.push_back(name + "|tracker");
    }
    
    // Add calo collections  
    for (const auto& name : calo_collection_names) {
        all_collections.push_back(name + "|calo");
    }
    
    // Add calo contribution collections (using original contribution collection names for the actual branch names)
    for (const auto& name : calo_contrib_collection_names) {
        all_collections.push_back(name + "|calocontrib");
    }
    
    // Sort alphabetically by collection name
    std::sort(all_collections.begin(), all_collections.end());
    
    // Create branch entries for sorted collections
    for (const auto& collection_with_type : all_collections) {
        size_t pos = collection_with_type.find("|");
        std::string collection_name = collection_with_type.substr(0, pos);
        std::string collection_type = collection_with_type.substr(pos + 1);
        
        // Add main collection branch
        branches_to_create.push_back({collection_name, collection_type, false, ""});
        
        // Add reference branches immediately after main branch
        if (collection_type == "tracker") {
            branches_to_create.push_back({"_" + collection_name + "_particle", collection_type, true, "particle"});
        } else if (collection_type == "calo") {
            branches_to_create.push_back({"_" + collection_name + "_contributions", collection_type, true, "contributions"});
        } else if (collection_type == "calocontrib") {
            branches_to_create.push_back({"_" + collection_name + "_particle", collection_type, true, "particle"});
        }
    }
    
    // Create all branches in the determined order
    for (const auto& branch : branches_to_create) {
        if (branch.type == "header") {
            if (branch.name == "EventHeader") {
                tree->Branch("EventHeader", &merged_event_headers);
            } //else if (branch.name == "SubEventHeaders") {
            //     tree->Branch("SubEventHeaders", &merged_sub_event_headers);
            // }
        } else if (branch.type == "mcparticle" && !branch.is_reference) {
            tree->Branch("MCParticles", &merged_mcparticles);
        } else if (branch.type == "mcparticle" && branch.is_reference) {
            if (branch.ref_suffix == "parents") {
                tree->Branch("_MCParticles_parents", &merged_mcparticle_parents_refs);
            } else if (branch.ref_suffix == "daughters") {
                tree->Branch("_MCParticles_daughters", &merged_mcparticle_children_refs);
            }
        } else if (branch.type == "tracker" && !branch.is_reference) {
            tree->Branch(branch.name.c_str(), &merged_tracker_hits[branch.name]);
        } else if (branch.type == "tracker" && branch.is_reference) {
            std::string collection_name = branch.name.substr(1, branch.name.length() - 10); // Remove "_" and "_particle"
            tree->Branch(branch.name.c_str(), &merged_tracker_hit_particle_refs[collection_name]);
        } else if (branch.type == "calo" && !branch.is_reference) {
            tree->Branch(branch.name.c_str(), &merged_calo_hits[branch.name]);
        } else if (branch.type == "calo" && branch.is_reference) {
            std::string collection_name = branch.name.substr(1, branch.name.length() - 15); // Remove "_" and "_contributions"
            tree->Branch(branch.name.c_str(), &merged_calo_hit_contributions_refs[collection_name]);
        } else if (branch.type == "calocontrib" && !branch.is_reference) {
            // Find the corresponding calo collection name for this contribution collection
            std::string corresponding_calo_name = "";
            for (const auto& calo_name : calo_collection_names) {
                if (getCorrespondingContributionCollection(calo_name) == branch.name) {
                    corresponding_calo_name = calo_name;
                    tree->Branch(branch.name.c_str(), &merged_calo_contributions[calo_name]);
                    break;
                }
            }
        } else if (branch.type == "calocontrib" && branch.is_reference) {
            // Find the corresponding calo collection name for this contribution collection
            std::string contrib_collection_name = branch.name.substr(1, branch.name.length() - 10); // Remove "_" and "_particle"
            std::string corresponding_calo_name = "";
            for (const auto& calo_name : calo_collection_names) {
                if (getCorrespondingContributionCollection(calo_name) == contrib_collection_name) {
                    corresponding_calo_name = calo_name;
                    tree->Branch(branch.name.c_str(), &merged_calo_contrib_particle_refs[calo_name]);
                    break;
                }
            }
        }
    }
    
    std::cout << "Created " << branches_to_create.size() << " branches in alphabetical order" << std::endl;
}

void StandaloneTimesliceMerger::writeTimesliceToTree(TTree* tree) {
    // Debug: show sizes of merged vectors before writing
    std::cout << "=== Writing timeslice ===" << std::endl;
    std::cout << "  MCParticles: " << merged_mcparticles.size() << std::endl;
    std::cout << "  MCParticle parents: " << merged_mcparticle_parents_refs.size() << std::endl;
    std::cout << "  MCParticle daughters: " << merged_mcparticle_children_refs.size() << std::endl;

    std::cout << "  Tracker collections (" << merged_tracker_hits.size() << "):" << std::endl;
    for (const auto& [name, hits] : merged_tracker_hits) {
        std::cout << "    " << name << ": " << hits.size() << " hits, " 
                  << merged_tracker_hit_particle_refs[name].size() << " particle refs" << std::endl;
    }
    
    std::cout << "  Calo collections (" << merged_calo_hits.size() << "):" << std::endl;
    for (const auto& [name, hits] : merged_calo_hits) {
        size_t contrib_refs_size = 0;
        if (merged_calo_hit_contributions_refs.count(name)) {
            contrib_refs_size = merged_calo_hit_contributions_refs.at(name).size();
        }
        std::cout << "    " << name << ": " << hits.size() << " hits, " 
                  << contrib_refs_size << " contribution refs" << std::endl;
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
    TClass* expectedClass = nullptr;
    EDataType expectedType;
    if (branch->GetExpectedType(expectedClass, expectedType) == 0 && expectedClass && expectedClass->GetName()) {
            TClass* expectedClass = nullptr;
            EDataType expectedType;
            if (branch->GetExpectedType(expectedClass, expectedType) == 0 && expectedClass) {
                branch_class_name = expectedClass->GetName();
            } else {
                branch_class_name = "";
            }
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
        TClass* expectedClass2 = nullptr;
        EDataType expectedType2;
        if (branch->GetExpectedType(expectedClass2, expectedType2) == 0 && expectedClass2 && expectedClass2->GetName()) {
            branch_type = expectedClass2->GetName();
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

void StandaloneTimesliceMerger::copyPodioMetadata(std::vector<SourceReader>& inputs, std::unique_ptr<TFile>& output_file) {
    if (inputs.empty()) {
        std::cout << "Warning: No input sources available for podio_metadata copying" << std::endl;
        return;
    }
    
    // Get the first source file to copy podio_metadata from
    const auto& first_source = inputs[0];
    if (!first_source.config->input_files.empty()) {
        std::string first_file = first_source.config->input_files[0];
        std::cout << "Attempting to copy podio_metadata from: " << first_file << std::endl;
        
        // Open the first source file directly to access podio_metadata tree
        auto source_file = std::make_unique<TFile>(first_file.c_str(), "READ");
        if (!source_file || source_file->IsZombie()) {
            std::cout << "Warning: Could not open source file for metadata copying: " << first_file << std::endl;
            return;
        }
        
        // Look for podio_metadata tree
        TTree* metadata_tree = dynamic_cast<TTree*>(source_file->Get("podio_metadata"));
        if (metadata_tree) {
            std::cout << "Found podio_metadata tree, copying to output..." << std::endl;
            output_file->cd();
            
            // Clone the tree structure and data
            TTree* output_metadata = metadata_tree->CloneTree(-1, "fast");
            if (output_metadata) {
                output_metadata->Write();
                std::cout << "Successfully copied podio_metadata tree" << std::endl;
            } else {
                std::cout << "Warning: Failed to clone podio_metadata tree" << std::endl;
            }
        } else {
            std::cout << "Info: No podio_metadata tree found in source file" << std::endl;
        }
        
        source_file->Close();
    }
}

std::string StandaloneTimesliceMerger::getCorrespondingContributionCollection(const std::string& calo_collection_name) {
    // Try to find a matching contribution collection based on naming patterns
    // Common patterns:
    // EcalBarrelHits -> EcalBarrelContributions
    // HcalEndcapHits -> HcalEndcapContributions
    // SomeCaloHits -> SomeCaloContributions

    // // Remove "Hits" suffix if present
    // std::string base = calo_collection_name;
    // if (base.length() > 4 && base.substr(base.length() - 4) == "Hits") {
    //     base = base.substr(0, base.length() - 4);
    // }
    // Add "Contributions" suffix to get the contribution collection name
    return calo_collection_name + "Contributions";
}

std::string StandaloneTimesliceMerger::getCorrespondingCaloCollection(const std::string& contrib_collection_name) {
    // Remove "Contributions" suffix if present
    std::string base = contrib_collection_name;
    if (base.length() > 13 && base.substr(base.length() - 13) == "Contributions") {
        base = base.substr(0, base.length() - 13);
    }
    // Add "Hits" suffix to get the calorimeter hit collection name
    return base;
}
