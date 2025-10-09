#include "DataSource.h"
#include "IndexOffsetHelper.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <TBranch.h>
#include <TObjArray.h>

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
    // Store references to collection names
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

void DataSource::loadEvent(size_t event_index) {
    // Load the event data from the TChain
    chain_->GetEntry(event_index);
    
}


void DataSource::setupBranches() {
    std::cout << "=== Setting up branches for source " << source_index_ << " ===" << std::endl;
    
    // Discover OneToMany relations from the file structure
    discoverOneToManyRelations();
    
    setupMCParticleBranches();
    setupTrackerBranches();
    setupCalorimeterBranches();
    setupEventHeaderBranches();
    setupGPBranches();
    
    std::cout << "=== Branch setup complete ===" << std::endl;
}

void DataSource::discoverOneToManyRelations() {
    std::cout << "Discovering OneToMany relations from file structure..." << std::endl;
    
    // Use the first input file to discover the structure
    if (config_->input_files.empty()) {
        std::cout << "Warning: No input files to discover relations from" << std::endl;
        return;
    }
    
    // Extract relations from the first file
    one_to_many_relations_ = IndexOffsetHelper::extractAllOneToManyRelations(config_->input_files[0]);
    
    // Print discovered relations
    for (const auto& [collection, fields] : one_to_many_relations_) {
        std::cout << "  Collection '" << collection << "' has OneToMany fields: ";
        for (size_t i = 0; i < fields.size(); ++i) {
            std::cout << fields[i];
            if (i < fields.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    }
    
    if (one_to_many_relations_.empty()) {
        std::cout << "  No OneToMany relations discovered" << std::endl;
    }
}

void DataSource::setupMCParticleBranches() {
    // Setup MCParticles branch
    mcparticle_branch_ = new std::vector<edm4hep::MCParticleData>();
    int result = chain_->SetBranchAddress("MCParticles", &mcparticle_branch_);

    // Setup MCParticle parent-child relationship branches using consolidated map
    std::string parents_branch_name = "_MCParticles_parents";
    std::string children_branch_name = "_MCParticles_daughters";
    
    objectid_branches_[parents_branch_name] = new std::vector<podio::ObjectID>();
    objectid_branches_[children_branch_name] = new std::vector<podio::ObjectID>();
    
    result = chain_->SetBranchAddress(parents_branch_name.c_str(), &objectid_branches_[parents_branch_name]);
    result = chain_->SetBranchAddress(children_branch_name.c_str(), &objectid_branches_[children_branch_name]);
}

void DataSource::setupTrackerBranches() {
    for (const auto& coll_name : *tracker_collection_names_) {
        tracker_hit_branches_[coll_name] = new std::vector<edm4hep::SimTrackerHitData>();
        int result = chain_->SetBranchAddress(coll_name.c_str(), &tracker_hit_branches_[coll_name]);
        
        // Also setup the particle reference branch using consolidated map
        std::string ref_branch_name = "_" + coll_name + "_particle";  
        objectid_branches_[ref_branch_name] = new std::vector<podio::ObjectID>();
        result = chain_->SetBranchAddress(ref_branch_name.c_str(), &objectid_branches_[ref_branch_name]);
    }
}

void DataSource::setupCalorimeterBranches() {
    // Setup calorimeter hit branches
    for (const auto& coll_name : *calo_collection_names_) {
        calo_hit_branches_[coll_name] = new std::vector<edm4hep::SimCalorimeterHitData>();
        int result = chain_->SetBranchAddress(coll_name.c_str(), &calo_hit_branches_[coll_name]);
        
        // Also setup the contributions reference branch using consolidated map
        std::string contrib_link_branch_name = "_" + coll_name + "_contributions";
        objectid_branches_[contrib_link_branch_name] = new std::vector<podio::ObjectID>();
        result = chain_->SetBranchAddress(contrib_link_branch_name.c_str(), &objectid_branches_[contrib_link_branch_name]);
        
        std::string contrib_branch_name = coll_name + "Contributions";
        calo_contrib_branches_[contrib_branch_name] = new std::vector<edm4hep::CaloHitContributionData>();
        result = chain_->SetBranchAddress(contrib_branch_name.c_str(), &calo_contrib_branches_[contrib_branch_name]);
        
        // Also setup the particle reference branch using consolidated map
        std::string ref_branch_name = "_" + contrib_branch_name + "_particle";  
        objectid_branches_[ref_branch_name] = new std::vector<podio::ObjectID>();
        result = chain_->SetBranchAddress(ref_branch_name.c_str(), &objectid_branches_[ref_branch_name]);
    }
}

void DataSource::setupEventHeaderBranches() {
    // Setup EventHeader if available
    std::vector<std::string> header_collections = {"EventHeader"};
    
    // Only add SubEventHeaders for non-merged sources (where they aren't already present)
    if (!config_->already_merged) {
        header_collections.push_back("SubEventHeaders");
    }
    
    for (const auto& coll_name : header_collections) {
        event_header_branches_[coll_name] = new std::vector<edm4hep::EventHeaderData>();
        int result = chain_->SetBranchAddress(coll_name.c_str(), &event_header_branches_[coll_name]);
        // Branch not available yet so returns 5
        // if (result != 0) {
        //     std::cout << "    ❌ Could not set branch address for " << coll_name << " (result: " << result << ")" << std::endl;
        // } else {
        //     std::cout << "    ✓ Successfully set up " << coll_name << std::endl;
        // }
    }
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