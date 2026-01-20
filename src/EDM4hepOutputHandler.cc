#include "EDM4hepOutputHandler.h"
#include <iostream>
#include <algorithm>
#include <TBranch.h>
#include <TObjArray.h>
#include <TChain.h>

void EDM4hepMergedCollections::clear() {
    // Use clear() but preserve capacity to avoid repeated memory allocations
    mcparticles.clear();
    event_headers.clear();
    event_header_weights.clear();
    sub_event_headers.clear();
    sub_event_header_weights.clear();
    
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
    mcparticle_daughters_refs.clear();
    
    for (auto& [name, vec] : calo_hit_contributions_refs) {
        vec.clear();
    }
    
    // Clear GP branches
    for (auto& [name, vec] : gp_key_branches) {
        vec.clear();
    }
    gp_int_values.clear();
    gp_float_values.clear();
    gp_double_values.clear();
    gp_string_values.clear();
}

void EDM4hepOutputHandler::initialize(const std::string& filename, 
                                      const std::vector<std::unique_ptr<DataSource>>& sources) {
    std::cout << "Initializing EDM4hep output handler for: " << filename << std::endl;
    
    // Open output file
    output_file_ = std::make_unique<TFile>(filename.c_str(), "RECREATE");
    if (!output_file_ || output_file_->IsZombie()) {
        throw std::runtime_error("Could not create output file: " + filename);
    }
    
    // Set ROOT I/O optimizations
    output_file_->SetCompressionLevel(1);
    
    // Create output tree
    output_tree_ = new TTree("events", "Merged timeslices");
    
    // Discover collections from sources
    discoverCollections(sources);
    
    // Setup output tree branches
    setupOutputTree();
    
    // Copy metadata from first source
    copyPodioMetadata(sources);
    
    std::cout << "EDM4hep output handler initialized successfully" << std::endl;
}

void EDM4hepOutputHandler::prepareTimeslice() {
    collections_.clear();
}

void EDM4hepOutputHandler::mergeEvents(std::vector<std::unique_ptr<DataSource>>& sources,
                                      size_t timeslice_number,
                                      float time_slice_duration,
                                      float bunch_crossing_period,
                                      std::mt19937& gen) {
    current_timeslice_number_ = timeslice_number;
    int totalEventsConsumed = 0;

    // Loop over sources and read needed events
    for(auto& data_source : sources) {
        const auto& config = data_source->getConfig();
        int sourceEventsConsumed = 0;

        for (size_t i = 0; i < data_source->getEntriesNeeded(); ++i) {
            // Calculate particle index offset for this event
            size_t particle_index_offset   = collections_.mcparticles.size();
            size_t particle_parents_offset = collections_.mcparticle_parents_refs.size();
            size_t particle_daughters_offset = collections_.mcparticle_daughters_refs.size();

            // Load the event data from this source
            data_source->loadEvent(data_source->getCurrentEntryIndex());

            // Generate time offset for this event
            data_source->UpdateTimeOffset(time_slice_duration, bunch_crossing_period, gen);
            
            // Process MCParticles - use move semantics to avoid copying
            auto& processed_particles = data_source->processMCParticles(particle_parents_offset, particle_daughters_offset, totalEventsConsumed);
            collections_.mcparticles.insert(collections_.mcparticles.end(), 
                                                  std::make_move_iterator(processed_particles.begin()), 
                                                  std::make_move_iterator(processed_particles.end()));
            
            // Process MCParticle references - use move semantics
            std::string parent_ref_branch_name = "_MCParticles_parents";
            auto& processed_parents = data_source->processObjectID(parent_ref_branch_name, particle_index_offset,totalEventsConsumed);
            collections_.mcparticle_parents_refs.insert(collections_.mcparticle_parents_refs.end(),
                                                              std::make_move_iterator(processed_parents.begin()), 
                                                              std::make_move_iterator(processed_parents.end()));

            std::string daughters_ref_branch_name = "_MCParticles_daughters";
            auto& processed_daughters = data_source->processObjectID(daughters_ref_branch_name, particle_index_offset,totalEventsConsumed);
            collections_.mcparticle_daughters_refs.insert(collections_.mcparticle_daughters_refs.end(),
                                                               std::make_move_iterator(processed_daughters.begin()), 
                                                               std::make_move_iterator(processed_daughters.end()));

            // Process SubEventHeaders for non-merged sources to track which MCParticles came from this source
            if (!config.already_merged) {
                // Create a SubEventHeader for this source/event combination
                edm4hep::EventHeaderData sub_header;
                sub_header.eventNumber = totalEventsConsumed;
                sub_header.runNumber = data_source->getSourceIndex();
                sub_header.timeStamp = particle_index_offset;
                sub_header.weight = data_source->getCurrentTimeOffset();

                collections_.sub_event_headers.push_back(sub_header);
                collections_.sub_event_header_weights.push_back(sub_header.weight);
            } else {
                // For already merged sources, process existing SubEventHeaders if available
                auto& existing_sub_headers = data_source->processEventHeaders("SubEventHeaders");
                for (auto& sub_header : existing_sub_headers) {
                    float original_offset = sub_header.weight;
                    sub_header.weight += static_cast<float>(particle_index_offset);
                    collections_.sub_event_headers.push_back(sub_header);
                    collections_.sub_event_header_weights.push_back(sub_header.weight);
                }
            }
            
            // Process tracker hits
            for (const auto& name : tracker_collection_names_) {
                auto& processed_hits = data_source->processTrackerHits(name, particle_index_offset,totalEventsConsumed);
                collections_.tracker_hits[name].insert(collections_.tracker_hits[name].end(),
                                                             std::make_move_iterator(processed_hits.begin()), 
                                                             std::make_move_iterator(processed_hits.end()));

                std::string ref_branch_name = "_" + name + "_particle";
                auto& processed_refs = data_source->processObjectID(ref_branch_name, particle_index_offset,totalEventsConsumed);
                collections_.tracker_hit_particle_refs[name].insert(collections_.tracker_hit_particle_refs[name].end(),
                                                                          std::make_move_iterator(processed_refs.begin()), 
                                                                          std::make_move_iterator(processed_refs.end()));
            }
            
            // Process calorimeter hits
            for (const auto& name : calo_collection_names_) {
                size_t existing_contrib_size = collections_.calo_contributions[name].size();

                auto& processed_hits = data_source->processCaloHits(name, existing_contrib_size,totalEventsConsumed);
                collections_.calo_hits[name].insert(collections_.calo_hits[name].end(),
                                                          std::make_move_iterator(processed_hits.begin()), 
                                                          std::make_move_iterator(processed_hits.end()));
                
                std::string ref_branch_name = "_" + name + "_contributions";
                auto& processed_contrib_refs = data_source->processObjectID(ref_branch_name, existing_contrib_size,totalEventsConsumed);
                collections_.calo_hit_contributions_refs[name].insert(collections_.calo_hit_contributions_refs[name].end(),
                                                                            std::make_move_iterator(processed_contrib_refs.begin()), 
                                                                            std::make_move_iterator(processed_contrib_refs.end()));
                
                // Process contributions
                std::string contrib_branch_name = name + "Contributions";
                auto& processed_contribs = data_source->processCaloContributions(contrib_branch_name, particle_index_offset,totalEventsConsumed);
                collections_.calo_contributions[name].insert(collections_.calo_contributions[name].end(),
                                                                   std::make_move_iterator(processed_contribs.begin()), 
                                                                   std::make_move_iterator(processed_contribs.end()));
                
                std::string ref_branch_name_contrib = "_" + contrib_branch_name + "_particle";
                auto& processed_contrib_particle_refs = data_source->processObjectID(ref_branch_name_contrib, particle_index_offset,totalEventsConsumed);
                collections_.calo_contrib_particle_refs[name].insert(collections_.calo_contrib_particle_refs[name].end(),
                                                                           std::make_move_iterator(processed_contrib_particle_refs.begin()), 
                                                                           std::make_move_iterator(processed_contrib_particle_refs.end()));
            }
            
            // Process GP (Global Parameter) branches
            for (const auto& name : gp_collection_names_) {
                auto& gp_keys = data_source->processGPBranch(name);
                collections_.gp_key_branches[name].insert(collections_.gp_key_branches[name].end(),
                                                                    std::make_move_iterator(gp_keys.begin()), std::make_move_iterator(gp_keys.end()));
            }

            // Process GP value branches
            auto& gp_int_values = data_source->processGPIntValues();
            collections_.gp_int_values.insert(collections_.gp_int_values.end(),
                std::make_move_iterator(gp_int_values.begin()), std::make_move_iterator(gp_int_values.end()));

            auto& gp_float_values = data_source->processGPFloatValues();
            collections_.gp_float_values.insert(collections_.gp_float_values.end(),
                std::make_move_iterator(gp_float_values.begin()), std::make_move_iterator(gp_float_values.end()));

            auto& gp_double_values = data_source->processGPDoubleValues();
            collections_.gp_double_values.insert(collections_.gp_double_values.end(),
                std::make_move_iterator(gp_double_values.begin()), std::make_move_iterator(gp_double_values.end()));

            auto& gp_string_values = data_source->processGPStringValues();
            collections_.gp_string_values.insert(collections_.gp_string_values.end(),
                std::make_move_iterator(gp_string_values.begin()), std::make_move_iterator(gp_string_values.end()));

            data_source->setCurrentEntryIndex(data_source->getCurrentEntryIndex() + 1);
            sourceEventsConsumed++;
            totalEventsConsumed++;
        }

        std::cout << "Merged " << sourceEventsConsumed << " events, totalling " 
                  << data_source->getCurrentEntryIndex() << " from source " << config.name << std::endl;
    }

    // Create main timeslice header
    edm4hep::EventHeaderData header;
    header.eventNumber = current_timeslice_number_;
    header.runNumber = 0;
    header.timeStamp = current_timeslice_number_;
    collections_.event_headers.push_back(header);
}

void EDM4hepOutputHandler::writeTimeslice() {
    if (!output_tree_) {
        throw std::runtime_error("Output tree not initialized");
    }
    
    output_tree_->Fill();
    std::cout << "=== Timeslice written ===" << std::endl;
}

void EDM4hepOutputHandler::finalize() {
    if (output_tree_) {
        output_tree_->Write();
    }
    if (output_file_) {
        output_file_->Close();
    }
    std::cout << "EDM4hep output finalized" << std::endl;
}

void EDM4hepOutputHandler::setupOutputTree() {
    if (!output_tree_) {
        throw std::runtime_error("Cannot setup output tree - tree is null");
    }
    
    // Create all required branches
    output_tree_->Branch("EventHeader", &collections_.event_headers);
    output_tree_->Branch("_EventHeader_weights", &collections_.event_header_weights);
    output_tree_->Branch("SubEventHeaders", &collections_.sub_event_headers);
    output_tree_->Branch("_SubEventHeader_weights", &collections_.sub_event_header_weights);
    output_tree_->Branch("MCParticles", &collections_.mcparticles);
    output_tree_->Branch("_MCParticles_daughters", &collections_.mcparticle_daughters_refs);
    output_tree_->Branch("_MCParticles_parents", &collections_.mcparticle_parents_refs);

    // Tracker collections and their references
    for (const auto& name : tracker_collection_names_) {
        output_tree_->Branch(name.c_str(), &collections_.tracker_hits[name]);        
        std::string ref_name = "_" + name + "_particle";
        output_tree_->Branch(ref_name.c_str(), &collections_.tracker_hit_particle_refs[name]);
    }

    // Calorimeter collections and their references
    for (const auto& name : calo_collection_names_) {
        output_tree_->Branch(name.c_str(), &collections_.calo_hits[name]);        
        std::string ref_name = "_" + name + "_contributions";
        output_tree_->Branch(ref_name.c_str(), &collections_.calo_hit_contributions_refs[name]);        
        std::string contrib_name = name + "Contributions";
        output_tree_->Branch(contrib_name.c_str(), &collections_.calo_contributions[name]);        
        std::string ref_name_contrib = "_" + contrib_name + "_particle";
        output_tree_->Branch(ref_name_contrib.c_str(), &collections_.calo_contrib_particle_refs[name]);
    }
    
    // GP (Global Parameter) branches
    for (const auto& name : gp_collection_names_) {
        output_tree_->Branch(name.c_str(), &collections_.gp_key_branches[name]);
    }
    
    output_tree_->Branch("GPIntValues", &collections_.gp_int_values);    
    output_tree_->Branch("GPFloatValues", &collections_.gp_float_values);    
    output_tree_->Branch("GPDoubleValues", &collections_.gp_double_values);    
    output_tree_->Branch("GPStringValues", &collections_.gp_string_values);
    
    std::cout << "Total branches created: " << output_tree_->GetListOfBranches()->GetEntries() << std::endl;
}

void EDM4hepOutputHandler::discoverCollections(const std::vector<std::unique_ptr<DataSource>>& sources) {
    if (sources.empty() || sources[0]->getConfig().input_files.empty()) {
        std::cout << "Warning: No sources available for collection discovery" << std::endl;
        return;
    }
    
    tracker_collection_names_ = discoverCollectionNames(*sources[0], "SimTrackerHit");
    calo_collection_names_ = discoverCollectionNames(*sources[0], "SimCalorimeterHit");
    gp_collection_names_ = discoverGPBranches(*sources[0]);
    
    std::cout << "EDM4hep collection names discovered:" << std::endl;
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
    for (const auto& data_source : sources) {
        const_cast<DataSource&>(*data_source).initialize(tracker_collection_names_, calo_collection_names_, gp_collection_names_);
    }
}

std::vector<std::string> EDM4hepOutputHandler::discoverCollectionNames(DataSource& source, const std::string& branch_pattern) {
    std::vector<std::string> names;
    
    if (source.getConfig().input_files.empty()) {
        return names;
    }
    
    auto temp_chain = std::make_unique<TChain>(source.getConfig().tree_name.c_str());
    temp_chain->Add(source.getConfig().input_files[0].c_str());
    
    TObjArray* branches = temp_chain->GetListOfBranches();
    if (!branches) {
        return names;
    }
    
    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = (TBranch*)branches->At(i);
        if (!branch) continue;
        
        std::string branch_name = branch->GetName();
        std::string branch_type = "";
        
        TClass* expectedClass = nullptr;
        EDataType expectedType;
        if (branch->GetExpectedType(expectedClass, expectedType) == 0 && expectedClass && expectedClass->GetName()) {
            branch_type = expectedClass->GetName();
        }
        
        if (branch_name.find("_") == 0) continue;
        if (branch_type.find(branch_pattern) == std::string::npos) continue;
        
        if (branch_type == "vector<edm4hep::SimTrackerHitData>" || 
            branch_type == "vector<edm4hep::SimCalorimeterHitData>") {
            names.push_back(branch_name);
        }
    }
    
    return names;
}

std::vector<std::string> EDM4hepOutputHandler::discoverGPBranches(DataSource& source) {
    std::vector<std::string> names;
    
    if (source.getConfig().input_files.empty()) {
        return names;
    }
    
    auto temp_chain = std::make_unique<TChain>(source.getConfig().tree_name.c_str());
    temp_chain->Add(source.getConfig().input_files[0].c_str());
    
    TObjArray* branches = temp_chain->GetListOfBranches();
    if (!branches) {
        return names;
    }
    
    std::vector<std::string> gp_patterns = {"GPIntKeys", "GPFloatKeys", "GPStringKeys", "GPDoubleKeys"};

    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = (TBranch*)branches->At(i);
        if (!branch) continue;
        
        std::string branch_name = branch->GetName();
        
        for (const auto& pattern : gp_patterns) {
            if (branch_name.find(pattern) == 0) {
                names.push_back(branch_name);
                break;
            }
        }
    }
    
    return names;
}

void EDM4hepOutputHandler::copyPodioMetadata(const std::vector<std::unique_ptr<DataSource>>& sources) {
    if (sources.empty() || !output_file_) {
        return;
    }
    
    const auto& first_source = sources[0];
    if (first_source->getConfig().input_files.empty()) {
        return;
    }
    
    std::string first_file = first_source->getConfig().input_files[0];
    auto source_file = std::make_unique<TFile>(first_file.c_str(), "READ");
    if (!source_file || source_file->IsZombie()) {
        return;
    }
    
    output_file_->cd();
    
    std::vector<std::string> metadata_trees = {"podio_metadata", "runs", "meta", "metadata"};
    
    for (const auto& tree_name : metadata_trees) {
        TTree* metadata_tree = dynamic_cast<TTree*>(source_file->Get(tree_name.c_str()));
        if (metadata_tree) {
            if (tree_name == "podio_metadata") {
                copyAndUpdatePodioMetadataTree(metadata_tree, output_file_.get());
            } else {
                TTree* output_metadata = metadata_tree->CloneTree(-1, "fast");
                if (output_metadata) {
                    output_metadata->Write();
                }
            }
        }
    }
    
    source_file->Close();
}

void EDM4hepOutputHandler::copyAndUpdatePodioMetadataTree(TTree* source_metadata_tree, TFile* output_file) {
    if (!source_metadata_tree || !output_file) {
        return;
    }
    
    TTree* output_metadata = source_metadata_tree->CloneTree(-1, "fast");
    if (output_metadata) {
        output_metadata->Write();
        std::cout << "Successfully copied podio_metadata tree" << std::endl;
    }
}

std::string EDM4hepOutputHandler::getCorrespondingContributionCollection(const std::string& calo_collection_name) const {
    return calo_collection_name + "Contributions";
}

std::string EDM4hepOutputHandler::getCorrespondingCaloCollection(const std::string& contrib_collection_name) const {
    std::string base = contrib_collection_name;
    if (base.length() > 13 && base.substr(base.length() - 13) == "Contributions") {
        base = base.substr(0, base.length() - 13);
    }
    return base;
}
