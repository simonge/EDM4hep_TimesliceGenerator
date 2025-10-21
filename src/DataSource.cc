#include "DataSource.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <TBranch.h>
#include <TObjArray.h>

DataSource::DataSource(const SourceConfig& config, size_t source_index)
    : config_(&config)
    , source_index_(source_index)
    , relationship_mapper_(nullptr)
    , total_entries_(0)
    , current_entry_index_(0)
    , entries_needed_(1)
    , gp_collection_names_(nullptr)
{
}

DataSource::~DataSource() {
    cleanup();
}

void DataSource::initialize(const std::vector<std::string>& tracker_collections,
                           const std::vector<std::string>& calo_collections,
                           const std::vector<std::string>& gp_collections,
                           const BranchRelationshipMapper* relationship_mapper) {
    // Store references to collection names and relationship mapper
    gp_collection_names_ = &gp_collections;
    relationship_mapper_ = relationship_mapper;
    
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

void DataSource::loadEvent(size_t event_index) {
    // Load the event data from the TChain
    chain_->GetEntry(event_index);
    
}

void* DataSource::processCollection(const std::string& collection_name, 
                                    size_t index_offset,
                                    float time_offset,
                                    int totalEventsConsumed) {
    // Get the branch data
    auto it = generic_branches_.find(collection_name);
    if (it == generic_branches_.end()) {
        std::cerr << "Warning: Collection " << collection_name << " not found in generic branches" << std::endl;
        return nullptr;
    }
    
    void* branch_ptr = it->second;
    
    // If first event and already merged, skip updating
    if (totalEventsConsumed == 0 && config_->already_merged) {
        return branch_ptr;
    }
    
    // Get collection metadata from relationship mapper
    if (!relationship_mapper_) {
        return branch_ptr;
    }
    
    auto coll_info = relationship_mapper_->getCollectionRelationships(collection_name);
    
    // Cast and process based on type
    if (coll_info.isType("MCParticleData")) {
        auto* particles = static_cast<std::vector<edm4hep::MCParticleData>*>(branch_ptr);
        for (auto& particle : *particles) {
            if (coll_info.has_time_field) {
                particle.time += time_offset;
            }
            if (coll_info.has_index_ranges) {
                particle.parents_begin += index_offset;
                particle.parents_end += index_offset;
                particle.daughters_begin += index_offset;
                particle.daughters_end += index_offset;
            }
            if (!config_->already_merged) {
                particle.generatorStatus += config_->generator_status_offset;
            }
        }
    } else if (coll_info.isType("SimTrackerHitData")) {
        auto* hits = static_cast<std::vector<edm4hep::SimTrackerHitData>*>(branch_ptr);
        if (coll_info.has_time_field && !config_->already_merged) {
            for (auto& hit : *hits) {
                hit.time += time_offset;
            }
        }
    } else if (coll_info.isType("SimCalorimeterHitData")) {
        auto* hits = static_cast<std::vector<edm4hep::SimCalorimeterHitData>*>(branch_ptr);
        if (coll_info.has_index_ranges) {
            for (auto& hit : *hits) {
                hit.contributions_begin += index_offset;
                hit.contributions_end += index_offset;
            }
        }
    } else if (coll_info.isType("CaloHitContributionData")) {
        auto* contribs = static_cast<std::vector<edm4hep::CaloHitContributionData>*>(branch_ptr);
        if (coll_info.has_time_field && !config_->already_merged) {
            for (auto& contrib : *contribs) {
                contrib.time += time_offset;
            }
        }
    }
    
    return branch_ptr;
}

void* DataSource::getBranchData(const std::string& branch_name) {
    auto it = generic_branches_.find(branch_name);
    if (it != generic_branches_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<podio::ObjectID>& DataSource::processObjectID(const std::string& branch_name, size_t index_offset, int totalEventsConsumed) {
    
    //If first event and already merged, skip updating references
    if (totalEventsConsumed == 0 && config_->already_merged) {
        return *objectid_branches_[branch_name];
    }

    // Update references with index offset
    for (auto& ref : *objectid_branches_[branch_name]) {
        ref.index += index_offset;
    }

    return *objectid_branches_[branch_name];
}

void DataSource::calculateTimeOffsetForEvent(const std::string& mcparticle_collection,
                                             float time_slice_duration,
                                             float bunch_crossing_period,
                                             std::mt19937& rng) {
    float distance = 0.0f;
    
    // Calculate time offset if not already merged
    if (!config_->already_merged) {
        // Get MCParticle data to calculate beam distance if needed
        if (config_->attach_to_beam) {
            void* mc_branch = getBranchData(mcparticle_collection);
            if (mc_branch) {
                auto* particles = static_cast<std::vector<edm4hep::MCParticleData>*>(mc_branch);
                if (!particles->empty()) {
                    distance = calculateBeamDistance(*particles);
                }
            }
        }
        current_time_offset_ = generateTimeOffset(distance, time_slice_duration, bunch_crossing_period, rng);
    } else {
        current_time_offset_ = 0.0f;
    }
}

void DataSource::setupBranches() {
    std::cout << "=== Setting up branches for source " << source_index_ << " ===" << std::endl;
    
    setupGenericBranches();
    setupRelationshipBranches();
    setupGPBranches();
    
    std::cout << "=== Branch setup complete ===" << std::endl;
}

void DataSource::setupGenericBranches() {
    if (!relationship_mapper_) {
        std::cerr << "Warning: No relationship mapper available for generic branch setup" << std::endl;
        return;
    }
    
    // Get all discovered collections
    auto all_collections = relationship_mapper_->getAllCollectionNames();
    
    for (const auto& coll_name : all_collections) {
        auto coll_info = relationship_mapper_->getCollectionRelationships(coll_name);
        
        // Allocate appropriate vector type based on data type
        void* branch_ptr = nullptr;
        
        if (coll_info.isType("MCParticleData")) {
            branch_ptr = new std::vector<edm4hep::MCParticleData>();
        } else if (coll_info.isType("SimTrackerHitData")) {
            branch_ptr = new std::vector<edm4hep::SimTrackerHitData>();
        } else if (coll_info.isType("SimCalorimeterHitData")) {
            branch_ptr = new std::vector<edm4hep::SimCalorimeterHitData>();
        } else if (coll_info.isType("CaloHitContributionData")) {
            branch_ptr = new std::vector<edm4hep::CaloHitContributionData>();
        } else if (coll_info.isType("EventHeaderData")) {
            branch_ptr = new std::vector<edm4hep::EventHeaderData>();
        } else {
            // Unknown type, skip
            continue;
        }
        
        generic_branches_[coll_name] = branch_ptr;
        int result = chain_->SetBranchAddress(coll_name.c_str(), branch_ptr);
        
        std::cout << "  Set up generic branch: " << coll_name << " (type: " << coll_info.data_type << ")" << std::endl;
    }
}

void DataSource::setupRelationshipBranches() {
    if (!relationship_mapper_) {
        std::cerr << "Warning: No relationship mapper available for relationship branch setup" << std::endl;
        return;
    }
    
    // Get all discovered collections
    auto all_collections = relationship_mapper_->getAllCollectionNames();
    
    for (const auto& coll_name : all_collections) {
        if (relationship_mapper_->hasRelationships(coll_name)) {
            auto rel_branches = relationship_mapper_->getRelationshipBranches(coll_name);
            for (const auto& branch_name : rel_branches) {
                objectid_branches_[branch_name] = new std::vector<podio::ObjectID>();
                int result = chain_->SetBranchAddress(branch_name.c_str(), &objectid_branches_[branch_name]);
                std::cout << "  Set up relationship: " << branch_name << " for " << coll_name << std::endl;
            }
        }
    }
}





void DataSource::setupGPBranches() {
    // Setup GP value branches in generic_branches_ map
    generic_branches_["GPIntValues"] = new std::vector<std::vector<int>>();
    generic_branches_["GPFloatValues"] = new std::vector<std::vector<float>>();
    generic_branches_["GPDoubleValues"] = new std::vector<std::vector<double>>();
    generic_branches_["GPStringValues"] = new std::vector<std::vector<std::string>>();
    
    int result = chain_->SetBranchAddress("GPIntValues", generic_branches_["GPIntValues"]);    
    result = chain_->SetBranchAddress("GPFloatValues", generic_branches_["GPFloatValues"]);
    result = chain_->SetBranchAddress("GPDoubleValues", generic_branches_["GPDoubleValues"]);    
    result = chain_->SetBranchAddress("GPStringValues", generic_branches_["GPStringValues"]);
    
    // Setup GP key branches in generic_branches_ map
    for (const auto& branch_name : *gp_collection_names_) {
        generic_branches_[branch_name] = new std::vector<std::string>();
        result = chain_->SetBranchAddress(branch_name.c_str(), generic_branches_[branch_name]);
    }
}

std::vector<std::string>& DataSource::processGPBranch(const std::string& branch_name) {
    // GP key branches don't need any processing, just return the data as-is
    // They contain global parameter keys that should be copied unchanged
    void* branch_ptr = getBranchData(branch_name);
    return *static_cast<std::vector<std::string>*>(branch_ptr);
}

std::vector<std::vector<int>>& DataSource::processGPIntValues() {
    // GP int values don't need any processing, just return the data as-is
    void* branch_ptr = getBranchData("GPIntValues");
    return *static_cast<std::vector<std::vector<int>>*>(branch_ptr);
}

std::vector<std::vector<float>>& DataSource::processGPFloatValues() {
    // GP float values don't need any processing, just return the data as-is
    void* branch_ptr = getBranchData("GPFloatValues");
    return *static_cast<std::vector<std::vector<float>>*>(branch_ptr);
}

std::vector<std::vector<double>>& DataSource::processGPDoubleValues() {
    // GP double values don't need any processing, just return the data as-is
    void* branch_ptr = getBranchData("GPDoubleValues");
    return *static_cast<std::vector<std::vector<double>>*>(branch_ptr);
}

std::vector<std::vector<std::string>>& DataSource::processGPStringValues() {
    // GP string values don't need any processing, just return the data as-is
    void* branch_ptr = getBranchData("GPStringValues");
    return *static_cast<std::vector<std::vector<std::string>>*>(branch_ptr);
}

void DataSource::cleanup() {
    // Clean up generic branches - need to cast and delete based on actual type
    // For now, we'll let ROOT handle cleanup when chain is destroyed
    // This is safer than trying to cast void* without knowing the exact type
    generic_branches_.clear();
    
    for (auto& [name, ptr] : objectid_branches_) {
        delete ptr;
    }
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
    std::cout << "=========================" << std::endl;
}