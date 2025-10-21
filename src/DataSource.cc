#include "DataSource.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <TBranch.h>
#include <TObjArray.h>
#include <podio/DatamodelRegistry.h>

DataSource::DataSource(const SourceConfig& config, size_t source_index)
    : config_(&config)
    , source_index_(source_index)
    , total_entries_(0)
    , current_entry_index_(0)
    , entries_needed_(1)
    , tracker_collection_names_(nullptr)
    , calo_collection_names_(nullptr)
    , mcparticle_branch_(nullptr)
    , gp_int_branch_(nullptr)
    , gp_float_branch_(nullptr)
    , gp_double_branch_(nullptr)
    , gp_string_branch_(nullptr)
{
}

DataSource::~DataSource() {
    cleanup();
}

void DataSource::initialize(const std::vector<std::string>& tracker_collections,
                           const std::vector<std::string>& calo_collections,
                           const std::vector<std::string>& gp_collections) {
    // Store references to collection names (kept for backward compatibility but will be deprecated)
    tracker_collection_names_ = &tracker_collections;
    calo_collection_names_ = &calo_collections;
    gp_collection_names_ = &gp_collections;
    
    if (!config_->input_files.empty()) {
        try {
            // Create TChain for this source
            chain_ = std::make_unique<TChain>(config_->tree_name.c_str());
            
            // Add all input files to the chain
            for (const auto& file : config_->input_files) {
                int result = chain_->Add(file.c_str());
                if (result == 0) {
                    throw std::runtime_error("Failed to add file: " + file);
                }
                std::cout << "Added file to source " << source_index_ << ": " << file << std::endl;
            }
            
            total_entries_ = chain_->GetEntries();
            
            if (total_entries_ == 0) {
                throw std::runtime_error("No entries found in source " + std::to_string(source_index_));
            }

            std::cout << "Source " << source_index_ << " has " << total_entries_ << " entries" << std::endl;
            
            // Setup branch addresses
            setupBranches();
            
            std::cout << "Successfully initialized source " << source_index_ << " (" << config_->name << ")" << std::endl;

        } catch (const std::exception& e) {
            throw std::runtime_error("ERROR: Could not open input files for source " + 
                                   std::to_string(source_index_) + ": " + e.what());
        }
    }
}

bool DataSource::hasMoreEntries() const {
    if(config_->repeat_on_eof && total_entries_ > 0) {
        return true;
    }
    return (current_entry_index_ + entries_needed_) <= total_entries_;
}

bool DataSource::loadNextEvent() {
    if (current_entry_index_ >= total_entries_) {
        if(config_->repeat_on_eof) {
            current_entry_index_ = 0;
        }
        return false;
    }
    
    chain_->GetEntry(current_entry_index_);
    return true;
}

float DataSource::generateTimeOffset(float distance, float time_slice_duration, float bunch_crossing_period, std::mt19937& rng) const {
    std::uniform_real_distribution<float> uniform(0.0f, time_slice_duration);
    float time_offset = uniform(rng);
    
    if (!config_->already_merged) {
        // Apply bunch crossing if enabled
        if (config_->use_bunch_crossing) {
            time_offset = std::floor(time_offset / bunch_crossing_period) * bunch_crossing_period;
        }
        
        // Apply beam effects if enabled
        if (config_->attach_to_beam) {
            // Add time offset based on distance along beam
            time_offset += distance / config_->beam_speed;
            
            // Add Gaussian spread if specified
            if (config_->beam_spread > 0.0f) {
                std::normal_distribution<float> spread_dist(0.0f, config_->beam_spread);
                time_offset += spread_dist(rng);
            }
        }
    }
    
    return time_offset;
}

EventData* DataSource::loadEvent(size_t event_index, float time_slice_duration, 
                                   float bunch_crossing_period, std::mt19937& rng) {
    // Load the event data from the TChain
    chain_->GetEntry(event_index);
    
    // Create new EventData
    current_event_data_ = std::make_unique<EventData>();
    
    // Calculate time offset using MCParticles if available
    float distance = 0.0f;
    if (!config_->already_merged && config_->attach_to_beam && mcparticle_branch_ && !mcparticle_branch_->empty()) {
        distance = calculateBeamDistance(*mcparticle_branch_);
    }
    current_event_data_->time_offset = generateTimeOffset(distance, time_slice_duration, bunch_crossing_period, rng);
    
    // Add MCParticles to the map if available
    if (mcparticle_branch_) {
        current_event_data_->collections["MCParticles"] = *mcparticle_branch_;
        current_event_data_->collection_sizes["MCParticles"] = mcparticle_branch_->size();
        
        // Add MCParticle reference collections using podio metadata
        const auto& registry = podio::DatamodelRegistry::instance();
        auto mcParticleInfo = registry.getRelationNames("MCParticle");
        for (const auto& relName : mcParticleInfo.relations) {
            std::string branch_name = "_MCParticles_" + std::string(relName);
            if (objectid_branches_.find(branch_name) != objectid_branches_.end()) {
                current_event_data_->collections[branch_name] = *objectid_branches_[branch_name];
                current_event_data_->collection_sizes[branch_name] = objectid_branches_[branch_name]->size();
            }
        }
    }
    
    // Add tracker hits and their references automatically
    const auto& registry = podio::DatamodelRegistry::instance();
    auto trackerHitInfo = registry.getRelationNames("SimTrackerHit");
    for (const auto& [name, branch_ptr] : tracker_hit_branches_) {
        if (branch_ptr) {
            current_event_data_->collections[name] = *branch_ptr;
            current_event_data_->collection_sizes[name] = branch_ptr->size();
            
            // Add relation branches
            for (const auto& relName : trackerHitInfo.relations) {
                std::string ref_name = "_" + name + "_" + std::string(relName);
                if (objectid_branches_.find(ref_name) != objectid_branches_.end()) {
                    current_event_data_->collections[ref_name] = *objectid_branches_[ref_name];
                    current_event_data_->collection_sizes[ref_name] = objectid_branches_[ref_name]->size();
                }
            }
        }
    }
    
    // Add calorimeter hits and their references automatically
    auto caloHitInfo = registry.getRelationNames("SimCalorimeterHit");
    for (const auto& [name, branch_ptr] : calo_hit_branches_) {
        if (branch_ptr) {
            current_event_data_->collections[name] = *branch_ptr;
            current_event_data_->collection_sizes[name] = branch_ptr->size();
            
            // Add relation branches
            for (const auto& relName : caloHitInfo.relations) {
                std::string ref_name = "_" + name + "_" + std::string(relName);
                if (objectid_branches_.find(ref_name) != objectid_branches_.end()) {
                    current_event_data_->collections[ref_name] = *objectid_branches_[ref_name];
                    current_event_data_->collection_sizes[ref_name] = objectid_branches_[ref_name]->size();
                }
            }
        }
    }
    
    // Add calorimeter contributions and their references automatically
    auto contribInfo = registry.getRelationNames("CaloHitContribution");
    for (const auto& [name, branch_ptr] : calo_contrib_branches_) {
        if (branch_ptr) {
            current_event_data_->collections[name] = *branch_ptr;
            current_event_data_->collection_sizes[name] = branch_ptr->size();
            
            // Add relation branches
            for (const auto& relName : contribInfo.relations) {
                std::string ref_name = "_" + name + "_" + std::string(relName);
                if (objectid_branches_.find(ref_name) != objectid_branches_.end()) {
                    current_event_data_->collections[ref_name] = *objectid_branches_[ref_name];
                    current_event_data_->collection_sizes[ref_name] = objectid_branches_[ref_name]->size();
                }
            }
        }
    }
    
    // Add EventHeaders automatically
    for (const auto& [name, branch_ptr] : event_header_branches_) {
        if (branch_ptr) {
            current_event_data_->collections[name] = *branch_ptr;
            current_event_data_->collection_sizes[name] = branch_ptr->size();
        }
    }
    
    // Add GP branches
    for (const auto& name : *gp_collection_names_) {
        current_event_data_->collections[name] = *gp_key_branches_[name];
        current_event_data_->collection_sizes[name] = gp_key_branches_[name]->size();
    }
    
    // Add GP value branches
    current_event_data_->collections["GPIntValues"] = *gp_int_branch_;
    current_event_data_->collection_sizes["GPIntValues"] = gp_int_branch_->size();
    
    current_event_data_->collections["GPFloatValues"] = *gp_float_branch_;
    current_event_data_->collection_sizes["GPFloatValues"] = gp_float_branch_->size();
    
    current_event_data_->collections["GPDoubleValues"] = *gp_double_branch_;
    current_event_data_->collection_sizes["GPDoubleValues"] = gp_double_branch_->size();
    
    current_event_data_->collections["GPStringValues"] = *gp_string_branch_;
    current_event_data_->collection_sizes["GPStringValues"] = gp_string_branch_->size();
    
    return current_event_data_.get();
}


void DataSource::setupBranches() {
    std::cout << "=== Setting up branches for source " << source_index_ << " ===" << std::endl;
    
    // Get podio registry for automatic relation discovery
    const auto& registry = podio::DatamodelRegistry::instance();
    
    // Get all branches from the chain
    TObjArray* branches = chain_->GetListOfBranches();
    if (!branches) {
        std::cout << "Warning: No branches found in chain" << std::endl;
        return;
    }
    
    std::cout << "Total branches in chain: " << branches->GetEntries() << std::endl;
    
    // Discover and setup object collection branches automatically
    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = (TBranch*)branches->At(i);
        if (!branch) continue;
        
        std::string branch_name = branch->GetName();
        
        // Skip branches that start with "_" - these are relation/reference branches
        if (branch_name.find("_") == 0) continue;
        
        // Skip GP value branches (these are handled separately)
        if (branch_name.find("GPIntValues") != std::string::npos ||
            branch_name.find("GPFloatValues") != std::string::npos ||
            branch_name.find("GPDoubleValues") != std::string::npos ||
            branch_name.find("GPStringValues") != std::string::npos) {
            continue;
        }
        
        // Get the branch data type
        TClass* expectedClass = nullptr;
        EDataType expectedType;
        std::string branch_type = "";
        if (branch->GetExpectedType(expectedClass, expectedType) == 0 && expectedClass && expectedClass->GetName()) {
            branch_type = expectedClass->GetName();
        }
        
        // Setup branches based on their type
        if (branch_type.find("edm4hep::MCParticleData") != std::string::npos) {
            setupObjectBranch(branch_name, "MCParticle", mcparticle_branch_);
        }
        else if (branch_type.find("edm4hep::SimTrackerHitData") != std::string::npos) {
            auto& branch_ptr = tracker_hit_branches_[branch_name];
            setupObjectBranch(branch_name, "SimTrackerHit", branch_ptr);
        }
        else if (branch_type.find("edm4hep::SimCalorimeterHitData") != std::string::npos) {
            auto& branch_ptr = calo_hit_branches_[branch_name];
            setupObjectBranch(branch_name, "SimCalorimeterHit", branch_ptr);
        }
        else if (branch_type.find("edm4hep::CaloHitContributionData") != std::string::npos) {
            auto& branch_ptr = calo_contrib_branches_[branch_name];
            setupObjectBranch(branch_name, "CaloHitContribution", branch_ptr);
        }
        else if (branch_type.find("edm4hep::EventHeaderData") != std::string::npos) {
            auto& branch_ptr = event_header_branches_[branch_name];
            setupObjectBranch(branch_name, "EventHeader", branch_ptr);
        }
    }
    
    // Setup GP branches separately (different pattern)
    setupGPBranches();
    
    std::cout << "=== Branch setup complete ===" << std::endl;
    std::cout << "  - Tracker hit collections: " << tracker_hit_branches_.size() << std::endl;
    std::cout << "  - Calorimeter hit collections: " << calo_hit_branches_.size() << std::endl;
    std::cout << "  - Calorimeter contribution collections: " << calo_contrib_branches_.size() << std::endl;
    std::cout << "  - ObjectID reference branches: " << objectid_branches_.size() << std::endl;
}

// Generic function to setup an object branch and its associated relation branches
template<typename T>
void DataSource::setupObjectBranch(const std::string& collection_name, const std::string& type_name, T*& branch_ptr) {
    // Setup the main object branch
    branch_ptr = new T();
    int result = chain_->SetBranchAddress(collection_name.c_str(), &branch_ptr);
    if (result != 0) {
        std::cout << "Warning: Could not set branch address for " << collection_name << " (result: " << result << ")" << std::endl;
        delete branch_ptr;
        branch_ptr = nullptr;
        return;
    }
    
    std::cout << "  ✓ Setup " << type_name << " branch: " << collection_name << std::endl;
    
    // Get relation names from podio's datamodel registry
    const auto& registry = podio::DatamodelRegistry::instance();
    auto relationInfo = registry.getRelationNames(type_name);
    
    // Setup relation branches automatically based on podio metadata
    for (const auto& relName : relationInfo.relations) {
        std::string rel_branch_name = "_" + collection_name + "_" + std::string(relName);
        objectid_branches_[rel_branch_name] = new std::vector<podio::ObjectID>();
        result = chain_->SetBranchAddress(rel_branch_name.c_str(), &objectid_branches_[rel_branch_name]);
        if (result == 0) {
            std::cout << "    ✓ Setup relation branch: " << rel_branch_name << std::endl;
        }
    }
    
    // Setup vector member branches if any
    for (const auto& vecName : relationInfo.vectorMembers) {
        // Vector members would be handled here if needed
        // For now, EDM4hep doesn't use this feature for our use case
    }
}

void DataSource::setupMCParticleBranches() {
    // DEPRECATED: This function is kept for reference but is no longer called
    // Branch setup is now done automatically in setupBranches()
}

void DataSource::setupTrackerBranches() {
    // DEPRECATED: This function is kept for reference but is no longer called
    // Branch setup is now done automatically in setupBranches()
}

void DataSource::setupCalorimeterBranches() {
    // DEPRECATED: This function is kept for reference but is no longer called
    // Branch setup is now done automatically in setupBranches()
}

void DataSource::setupEventHeaderBranches() {
    // DEPRECATED: This function is kept for reference but is no longer called
    // Branch setup is now done automatically in setupBranches()
}

void DataSource::setupGPBranches() {
    // Initialize GP value branch pointers
    gp_int_branch_ = new std::vector<std::vector<int>>();
    gp_float_branch_ = new std::vector<std::vector<float>>();
    gp_double_branch_ = new std::vector<std::vector<double>>();
    gp_string_branch_ = new std::vector<std::vector<std::string>>();
    
    // Setup the fixed GP value branches
    int result = chain_->SetBranchAddress("GPIntValues", &gp_int_branch_);    
    result = chain_->SetBranchAddress("GPFloatValues", &gp_float_branch_);
    result = chain_->SetBranchAddress("GPDoubleValues", &gp_double_branch_);    
    result = chain_->SetBranchAddress("GPStringValues", &gp_string_branch_);
    
    // Setup GP key branches
    for (const auto& branch_name : *gp_collection_names_) {
        gp_key_branches_[branch_name] = new std::vector<std::string>();
        result = chain_->SetBranchAddress(branch_name.c_str(), &gp_key_branches_[branch_name]);
    }
}

void DataSource::cleanup() {
    // Clean up dynamically allocated vectors
    delete mcparticle_branch_;
    
    for (auto& [name, ptr] : tracker_hit_branches_) {
        delete ptr;
    }
    for (auto& [name, ptr] : calo_hit_branches_) {
        delete ptr;
    }
    for (auto& [name, ptr] : calo_contrib_branches_) {
        delete ptr;
    }
    for (auto& [name, ptr] : event_header_branches_) {
        delete ptr;
    }
    for (auto& [name, ptr] : objectid_branches_) {
        delete ptr;
    }
    for (auto& [name, ptr] : gp_key_branches_) {
        delete ptr;
    }
    
    // Clean up GP value branch pointers
    delete gp_int_branch_;
    delete gp_float_branch_;
    delete gp_double_branch_;
    delete gp_string_branch_;
    
}


float DataSource::calculateBeamDistance(const std::vector<edm4hep::MCParticleData>& particles) const {
    float distance = 0.0f;
    
    // Get position of first particle with generatorStatus 1
    try {
        for (const auto& particle : particles) {
            if (particle.generatorStatus == 1) {
                // Distance is dot product of position vector relative to rotation around y of beam relative to z-axis
                distance = particle.vertex.z * std::cos(config_->beam_angle) + 
                          particle.vertex.x * std::sin(config_->beam_angle);
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Warning: Could not access MCParticles for beam attachment: " << e.what() << std::endl;
    }
    
    return distance;
}

void DataSource::printStatus() const {
    std::cout << "=== DataSource Status ===" << std::endl;
    std::cout << "Source: " << source_index_ << " (" << config_->name << ")" << std::endl;
    std::cout << "Total entries: " << total_entries_ << std::endl;
    std::cout << "Current entry: " << current_entry_index_ << std::endl;
    std::cout << "Entries needed: " << entries_needed_ << std::endl;
    std::cout << "Initialized: " << (isInitialized() ? "Yes" : "No") << std::endl;
    
    if (tracker_collection_names_) {
        std::cout << "Tracker collections: " << tracker_collection_names_->size() << std::endl;
    }
    if (calo_collection_names_) {
        std::cout << "Calorimeter collections: " << calo_collection_names_->size() << std::endl;
    }
    std::cout << "=========================" << std::endl;
}