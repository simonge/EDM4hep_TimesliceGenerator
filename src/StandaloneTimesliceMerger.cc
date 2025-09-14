#include "StandaloneTimesliceMerger.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <TBranch.h>
#include <TObjArray.h>

void MergedCollections::clear() {
    mcparticles.clear();
    event_headers.clear();
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
        calo_contrib_collection_names_ = discoverCollectionNames(*data_sources[0], "CaloHitContribution");
        
        std::cout << "Global collection names discovered:" << std::endl;
        std::cout << "  Tracker: ";
        for (const auto& name : tracker_collection_names_) std::cout << name << " ";
        std::cout << std::endl;
        std::cout << "  Calo: ";
        for (const auto& name : calo_collection_names_) std::cout << name << " ";
        std::cout << std::endl;
        std::cout << "  Calo contributions: ";
        for (const auto& name : calo_contrib_collection_names_) std::cout << name << " ";
        std::cout << std::endl;
        
        // Initialize all data sources with discovered collection names
        for (auto& data_source : data_sources) {
            data_source->initialize(tracker_collection_names_, calo_collection_names_, calo_contrib_collection_names_);
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
            
            // Read and merge event data from this source
            data_source->mergeEventData(data_source->getCurrentEntryIndex(), 
                                      particle_index_offset,
                                      m_config.time_slice_duration,
                                      m_config.bunch_crossing_period,
                                      gen,
                                      merged_collections_.mcparticles,
                                      merged_collections_.tracker_hits,
                                      merged_collections_.calo_hits,
                                      merged_collections_.calo_contributions,
                                      merged_collections_.mcparticle_parents_refs,
                                      merged_collections_.mcparticle_children_refs,
                                      merged_collections_.tracker_hit_particle_refs,
                                      merged_collections_.calo_contrib_particle_refs,
                                      merged_collections_.calo_hit_contributions_refs);

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
    // Create a list of all collections and their reference branches in alphabetical order
    struct BranchInfo {
        std::string name;
        std::string type; // "header", "mcparticle", "tracker", "calo", "calocontrib"
        bool is_reference;
        std::string ref_suffix;
    };
    
    std::vector<BranchInfo> branches_to_create;
    
    // Add fixed header branches first
    branches_to_create.push_back({"EventHeader", "header", false, ""});
    branches_to_create.push_back({"SubEventHeaders", "header", false, ""});
    
    // Add MCParticles and its reference branches
    branches_to_create.push_back({"MCParticles", "mcparticle", false, ""});
    branches_to_create.push_back({"_MCParticles_daughters", "mcparticle", true, "daughters"});
    branches_to_create.push_back({"_MCParticles_parents", "mcparticle", true, "parents"});
    
    // Collect all other collections and sort them alphabetically
    std::vector<std::string> all_collections;
    
    // Add tracker collections
    for (const auto& name : tracker_collection_names_) {
        all_collections.push_back(name + "|tracker");
    }
    
    // Add calo collections  
    for (const auto& name : calo_collection_names_) {
        all_collections.push_back(name + "|calo");
    }
    
    // Add calo contribution collections
    for (const auto& name : calo_contrib_collection_names_) {
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
                tree->Branch("EventHeader", &merged_collections_.event_headers);
            }
            // SubEventHeaders are commented out for now
            // else if (branch.name == "SubEventHeaders") {
            //     tree->Branch("SubEventHeaders", &merged_collections_.sub_event_headers);
            // }
        } else if (branch.type == "mcparticle" && !branch.is_reference) {
            tree->Branch("MCParticles", &merged_collections_.mcparticles);
        } else if (branch.type == "mcparticle" && branch.is_reference) {
            if (branch.ref_suffix == "parents") {
                tree->Branch("_MCParticles_parents", &merged_collections_.mcparticle_parents_refs);
            } else if (branch.ref_suffix == "daughters") {
                tree->Branch("_MCParticles_daughters", &merged_collections_.mcparticle_children_refs);
            }
        } else if (branch.type == "tracker" && !branch.is_reference) {
            tree->Branch(branch.name.c_str(), &merged_collections_.tracker_hits[branch.name]);
        } else if (branch.type == "tracker" && branch.is_reference) {
            std::string collection_name = branch.name.substr(1, branch.name.length() - 10); // Remove "_" and "_particle"
            tree->Branch(branch.name.c_str(), &merged_collections_.tracker_hit_particle_refs[collection_name]);
        } else if (branch.type == "calo" && !branch.is_reference) {
            tree->Branch(branch.name.c_str(), &merged_collections_.calo_hits[branch.name]);
        } else if (branch.type == "calo" && branch.is_reference) {
            std::string collection_name = branch.name.substr(1, branch.name.length() - 15); // Remove "_" and "_contributions"
            tree->Branch(branch.name.c_str(), &merged_collections_.calo_hit_contributions_refs[collection_name]);
        } else if (branch.type == "calocontrib" && !branch.is_reference) {
            // Find the corresponding calo collection name for this contribution collection
            for (const auto& calo_name : calo_collection_names_) {
                if (getCorrespondingContributionCollection(calo_name) == branch.name) {
                    tree->Branch(branch.name.c_str(), &merged_collections_.calo_contributions[calo_name]);
                    break;
                }
            }
        } else if (branch.type == "calocontrib" && branch.is_reference) {
            // Find the corresponding calo collection name for this contribution collection
            std::string contrib_collection_name = branch.name.substr(1, branch.name.length() - 10); // Remove "_" and "_particle"
            for (const auto& calo_name : calo_collection_names_) {
                if (getCorrespondingContributionCollection(calo_name) == contrib_collection_name) {
                    tree->Branch(branch.name.c_str(), &merged_collections_.calo_contrib_particle_refs[calo_name]);
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
    std::cout << "  MCParticles: " << merged_collections_.mcparticles.size() << std::endl;
    std::cout << "  MCParticle parents: " << merged_collections_.mcparticle_parents_refs.size() << std::endl;
    std::cout << "  MCParticle daughters: " << merged_collections_.mcparticle_children_refs.size() << std::endl;

    std::cout << "  Tracker collections (" << merged_collections_.tracker_hits.size() << "):" << std::endl;
    for (const auto& [name, hits] : merged_collections_.tracker_hits) {
        std::cout << "    " << name << ": " << hits.size() << " hits, " 
                  << merged_collections_.tracker_hit_particle_refs[name].size() << " particle refs" << std::endl;
    }
    
    std::cout << "  Calo collections (" << merged_collections_.calo_hits.size() << "):" << std::endl;
    for (const auto& [name, hits] : merged_collections_.calo_hits) {
        size_t contrib_refs_size = 0;
        if (merged_collections_.calo_hit_contributions_refs.count(name)) {
            contrib_refs_size = merged_collections_.calo_hit_contributions_refs.at(name).size();
        }
        std::cout << "    " << name << ": " << hits.size() << " hits, " 
                  << contrib_refs_size << " contribution refs" << std::endl;
    }
    
    std::cout << "  Calo contribution collections (" << merged_collections_.calo_contributions.size() << "):" << std::endl;
    for (const auto& [name, contribs] : merged_collections_.calo_contributions) {
        std::cout << "    " << name << ": " << contribs.size() << " contributions, " 
                  << merged_collections_.calo_contrib_particle_refs[name].size() << " particle refs" << std::endl;
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
        if (branch_name.find("_") == 0) continue;
        
        // Look for collections based on data type rather than branch name
        if (branch_pattern.find("SimTrackerHit") != std::string::npos) {
            if (branch_type.find("vector<edm4hep::SimTrackerHitData>") != std::string::npos ||
                branch_type.find("SimTrackerHitData") != std::string::npos) {
                names.push_back(branch_name);
                std::cout << "  ✓ MATCHED TRACKER: " << branch_name << " (type: " << branch_type << ")" << std::endl;
            }
        } else if (branch_pattern.find("SimCalorimeterHit") != std::string::npos) {
            if (branch_type.find("vector<edm4hep::SimCalorimeterHitData>") != std::string::npos ||
                branch_type.find("SimCalorimeterHitData") != std::string::npos) {
                names.push_back(branch_name);
                std::cout << "  ✓ MATCHED CALO: " << branch_name << " (type: " << branch_type << ")" << std::endl;
            }
        } else if (branch_pattern.find("CaloHitContribution") != std::string::npos) {
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