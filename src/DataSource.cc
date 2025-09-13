#include "DataSource.h"
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
    , calo_contrib_collection_names_(nullptr)
    , mcparticle_branch_(nullptr)
    , mcparticle_parents_refs_(nullptr)
    , mcparticle_children_refs_(nullptr)
{
}

DataSource::~DataSource() {
    cleanup();
}

void DataSource::initialize(const std::vector<std::string>& tracker_collections,
                           const std::vector<std::string>& calo_collections, 
                           const std::vector<std::string>& calo_contrib_collections) {
    // Store references to collection names
    tracker_collection_names_ = &tracker_collections;
    calo_collection_names_ = &calo_collections;
    calo_contrib_collection_names_ = &calo_contrib_collections;
    
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
    return (current_entry_index_ + entries_needed_) <= total_entries_;
}

bool DataSource::loadNextEvent() {
    if (current_entry_index_ >= total_entries_) {
        return false;
    }
    
    chain_->GetEntry(current_entry_index_);
    return true;
}

float DataSource::generateTimeOffset(float distance, std::mt19937& rng) const {
    float time_offset = 0.0f;
    
    if (!config_->already_merged) {
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

void DataSource::mergeEventData(size_t event_index,
                               size_t particle_index_offset,
                               std::vector<edm4hep::MCParticleData>& merged_mcparticles,
                               std::unordered_map<std::string, std::vector<edm4hep::SimTrackerHitData>>& merged_tracker_hits,
                               std::unordered_map<std::string, std::vector<edm4hep::SimCalorimeterHitData>>& merged_calo_hits,
                               std::unordered_map<std::string, std::vector<edm4hep::CaloHitContributionData>>& merged_calo_contributions,
                               std::vector<podio::ObjectID>& merged_mcparticle_parents_refs,
                               std::vector<podio::ObjectID>& merged_mcparticle_children_refs,
                               std::unordered_map<std::string, std::vector<podio::ObjectID>>& merged_tracker_hit_particle_refs,
                               std::unordered_map<std::string, std::vector<podio::ObjectID>>& merged_calo_contrib_particle_refs,
                               std::unordered_map<std::string, std::vector<podio::ObjectID>>& merged_calo_hit_contributions_refs) {
    // Load the event data from the TChain
    chain_->GetEntry(event_index);
    
    float time_offset = 0.0f;
    float distance = 0.0f;
    
    // Get the particles vector for reference handling
    auto& particles = *mcparticle_branch_;
    auto& parents_refs = *mcparticle_parents_refs_;
    auto& children_refs = *mcparticle_children_refs_;

    // Calculate time offset if not already merged
    if (!config_->already_merged) {
        if (config_->attach_to_beam && !particles.empty()) {
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
        }
    }

    // Process particles if this is first source or not already merged
    if(!config_->already_merged && merged_mcparticles.empty()) {
        // Process MCParticles
        for (auto& particle : particles) {
            particle.time += time_offset;
            // Update generator status offset
            particle.generatorStatus += config_->generator_status_offset;
            particle.parents_begin   += particle_index_offset;
            particle.parents_end     += particle_index_offset;
            particle.daughters_begin += particle_index_offset;
            particle.daughters_end   += particle_index_offset;
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

        // Process edm4hep::SimTrackerHits
        for (const auto& name : *tracker_collection_names_) {
            if (tracker_hit_branches_.count(name) && tracker_hit_particle_refs_.count(name)) {
                auto& hits = *tracker_hit_branches_[name];
                auto& particle_refs = *tracker_hit_particle_refs_[name];
                
                for (size_t i = 0; i < hits.size(); ++i) {
                    auto& hit = hits[i];
                    auto& particle_ref = particle_refs[i];
                    hit.time += time_offset;            
                    particle_ref.index = particle_index_offset + particle_ref.index;
                }
            }
        }

        // Process calorimeter hits and contributions together
        for (const auto& calo_name : *calo_collection_names_) {
            // Only process if the branches exist for this source
            if (!calo_hit_branches_.count(calo_name)) {
                continue; // Skip this collection if not available in this source
            }

            //Get size of contributions before adding new ones
            size_t existing_contrib_size = merged_calo_contributions[calo_name].size();
                    
            auto& hits = *calo_hit_branches_[calo_name];
            
            // Check if contribution references exist for this source
            if (calo_hit_contributions_refs_.count(calo_name)) {
                auto& contributions_refs = *calo_hit_contributions_refs_[calo_name];
                
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
                calo_contrib_branches_.count(corresponding_contrib_collection) &&
                calo_contrib_particle_refs_.count(corresponding_contrib_collection)) {
                
                auto& contribs = *calo_contrib_branches_[corresponding_contrib_collection];
                auto& particle_refs = *calo_contrib_particle_refs_[corresponding_contrib_collection];
                
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

    for (const auto& name : *tracker_collection_names_) {
        if (tracker_hit_branches_.count(name)) {
            const auto& hits = *tracker_hit_branches_[name];
            merged_tracker_hits[name].insert(merged_tracker_hits[name].end(), std::make_move_iterator(hits.begin()), std::make_move_iterator(hits.end()));
        }
        if (tracker_hit_particle_refs_.count(name)) {
            const auto& refs = *tracker_hit_particle_refs_[name];
            merged_tracker_hit_particle_refs[name].insert(merged_tracker_hit_particle_refs[name].end(), std::make_move_iterator(refs.begin()), std::make_move_iterator(refs.end()));
        }
    }

    for (const auto& calo_name : *calo_collection_names_) {
        if (calo_hit_branches_.count(calo_name)) {
            const auto& hits = *calo_hit_branches_[calo_name];
            merged_calo_hits[calo_name].insert(merged_calo_hits[calo_name].end(), std::make_move_iterator(hits.begin()), std::make_move_iterator(hits.end()));
        }
        if (calo_hit_contributions_refs_.count(calo_name)) {
            const auto& refs = *calo_hit_contributions_refs_[calo_name];
            merged_calo_hit_contributions_refs[calo_name].insert(merged_calo_hit_contributions_refs[calo_name].end(), std::make_move_iterator(refs.begin()), std::make_move_iterator(refs.end()));
        }

        // Also merge contributions from corresponding contribution collection using proper key mapping
        std::string corresponding_contrib_collection = getCorrespondingContributionCollection(calo_name);
        if (!corresponding_contrib_collection.empty() && calo_contrib_branches_.count(corresponding_contrib_collection)) {
            const auto& contribs = *calo_contrib_branches_[corresponding_contrib_collection];
            merged_calo_contributions[calo_name].insert(merged_calo_contributions[calo_name].end(), std::make_move_iterator(contribs.begin()), std::make_move_iterator(contribs.end()));
        }
        if (!corresponding_contrib_collection.empty() && calo_contrib_particle_refs_.count(corresponding_contrib_collection)) {
            const auto& refs = *calo_contrib_particle_refs_[corresponding_contrib_collection];
            merged_calo_contrib_particle_refs[calo_name].insert(merged_calo_contrib_particle_refs[calo_name].end(), std::make_move_iterator(refs.begin()), std::make_move_iterator(refs.end()));
        }
    }
}

void DataSource::setupBranches() {
    std::cout << "=== Setting up branches for source " << source_index_ << " ===" << std::endl;
    
    setupMCParticleBranches();
    setupTrackerBranches();
    setupCalorimeterBranches();
    setupEventHeaderBranches();
    
    std::cout << "=== Branch setup complete ===" << std::endl;
}

void DataSource::setupMCParticleBranches() {
    // Setup MCParticles branch
    mcparticle_branch_ = new std::vector<edm4hep::MCParticleData>();
    int result = chain_->SetBranchAddress("MCParticles", &mcparticle_branch_);
    if (result != 0) {
        std::cout << "    ❌ Could not set branch address for MCParticles (result: " << result << ")" << std::endl;
    } else {
        std::cout << "    ✓ Successfully set up MCParticles" << std::endl;
    }

    // Setup MCParticle parent-child relationship branches
    std::string parents_branch_name = "_MCParticles_parents";
    std::string children_branch_name = "_MCParticles_daughters";
    
    mcparticle_parents_refs_ = new std::vector<podio::ObjectID>();
    mcparticle_children_refs_ = new std::vector<podio::ObjectID>();
    
    result = chain_->SetBranchAddress(parents_branch_name.c_str(), &mcparticle_parents_refs_);
    if (result != 0) {
        std::cout << "    ⚠️  Could not set branch address for " << parents_branch_name << " (result: " << result << ")" << std::endl;
        std::cout << "       This may be normal if the input file doesn't contain MCParticle relationship information" << std::endl;
    } else {
        std::cout << "    ✓ Successfully set up " << parents_branch_name << std::endl;
    }

    result = chain_->SetBranchAddress(children_branch_name.c_str(), &mcparticle_children_refs_);
    if (result != 0) {
        std::cout << "    ⚠️  Could not set branch address for " << children_branch_name << " (result: " << result << ")" << std::endl;
        std::cout << "       This may be normal if the input file doesn't contain MCParticle relationship information" << std::endl;
    } else {
        std::cout << "    ✓ Successfully set up " << children_branch_name << std::endl;
    }
}

void DataSource::setupTrackerBranches() {
    for (const auto& coll_name : *tracker_collection_names_) {
        tracker_hit_branches_[coll_name] = new std::vector<edm4hep::SimTrackerHitData>();
        int result = chain_->SetBranchAddress(coll_name.c_str(), &tracker_hit_branches_[coll_name]);
        if (result != 0) {
            std::cout << "    ❌ Could not set branch address for " << coll_name << " (result: " << result << ")" << std::endl;
        } else {
            std::cout << "    ✓ Successfully set up tracker collection " << coll_name << std::endl;
        }
        
        // Also setup the particle reference branch
        std::string ref_branch_name = "_" + coll_name + "_particle";  
        tracker_hit_particle_refs_[coll_name] = new std::vector<podio::ObjectID>();
        result = chain_->SetBranchAddress(ref_branch_name.c_str(), &tracker_hit_particle_refs_[coll_name]);
        if (result != 0) {
            std::cout << "    ❌ Could not set branch address for " << ref_branch_name << " (result: " << result << ")" << std::endl;
        } else {
            std::cout << "    ✓ Successfully set up particle ref " << ref_branch_name << std::endl;
        }
    }
}

void DataSource::setupCalorimeterBranches() {
    // Setup calorimeter hit branches
    for (const auto& coll_name : *calo_collection_names_) {
        calo_hit_branches_[coll_name] = new std::vector<edm4hep::SimCalorimeterHitData>();
        int result = chain_->SetBranchAddress(coll_name.c_str(), &calo_hit_branches_[coll_name]);
        if (result != 0) {
            std::cout << "    ❌ Could not set branch address for " << coll_name << " (result: " << result << ")" << std::endl;
        } else {
            std::cout << "    ✓ Successfully set up calo collection " << coll_name << std::endl;
        }
        
        // Also setup the contributions reference branch
        std::string contrib_branch_name = "_" + coll_name + "_contributions";
        calo_hit_contributions_refs_[coll_name] = new std::vector<podio::ObjectID>();
        result = chain_->SetBranchAddress(contrib_branch_name.c_str(), &calo_hit_contributions_refs_[coll_name]);
        if (result != 0) {
            std::cout << "    ⚠️  Could not set branch address for " << contrib_branch_name << " (result: " << result << ")" << std::endl;
            std::cout << "       This may be normal if the input file doesn't contain calorimeter hit contribution relationships" << std::endl;
        } else {
            std::cout << "    ✓ Successfully set up contributions ref " << contrib_branch_name << std::endl;
        }
    }
    
    // Setup calorimeter contribution branches
    for (const auto& coll_name : *calo_contrib_collection_names_) {
        calo_contrib_branches_[coll_name] = new std::vector<edm4hep::CaloHitContributionData>();
        int result = chain_->SetBranchAddress(coll_name.c_str(), &calo_contrib_branches_[coll_name]);
        if (result != 0) {
            std::cout << "    ❌ Could not set branch address for " << coll_name << " (result: " << result << ")" << std::endl;
        } else {
            std::cout << "    ✓ Successfully set up calo contrib collection " << coll_name << std::endl;
        }
        
        // Also setup the particle reference branch
        std::string ref_branch_name = "_" + coll_name + "_particle";  
        calo_contrib_particle_refs_[coll_name] = new std::vector<podio::ObjectID>();
        result = chain_->SetBranchAddress(ref_branch_name.c_str(), &calo_contrib_particle_refs_[coll_name]);
        if (result != 0) {
            std::cout << "    ❌ Could not set branch address for " << ref_branch_name << " (result: " << result << ")" << std::endl;
        } else {
            std::cout << "    ✓ Successfully set up particle ref " << ref_branch_name << std::endl;
        }
    }
}

void DataSource::setupEventHeaderBranches() {
    // Setup EventHeader if available
    std::vector<std::string> header_collections = {"EventHeader", "SubEventHeaders"};
    for (const auto& coll_name : header_collections) {
        event_header_branches_[coll_name] = new std::vector<edm4hep::EventHeaderData>();
        int result = chain_->SetBranchAddress(coll_name.c_str(), &event_header_branches_[coll_name]);
        if (result != 0) {
            std::cout << "    ❌ Could not set branch address for " << coll_name << " (result: " << result << ")" << std::endl;
        } else {
            std::cout << "    ✓ Successfully set up " << coll_name << std::endl;
        }
    }
}

void DataSource::cleanup() {
    // Clean up dynamically allocated vectors
    delete mcparticle_branch_;
    delete mcparticle_parents_refs_;
    delete mcparticle_children_refs_;
    
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
    for (auto& [name, ptr] : tracker_hit_particle_refs_) {
        delete ptr;
    }
    for (auto& [name, ptr] : calo_contrib_particle_refs_) {
        delete ptr;
    }
    for (auto& [name, ptr] : calo_hit_contributions_refs_) {
        delete ptr;
    }
}

std::string DataSource::getCorrespondingContributionCollection(const std::string& calo_collection_name) const {
    // Mapping between calo collection names and contribution collection names
    // This is a simplified mapping - in a real application this might be more complex
    if (calo_collection_name.find("EcalBarrel") != std::string::npos) {
        return "EcalBarrelContributions";
    } else if (calo_collection_name.find("EcalEndcap") != std::string::npos) {
        return "EcalEndcapContributions";
    } else if (calo_collection_name.find("HcalBarrel") != std::string::npos) {
        return "HcalBarrelContributions";
    } else if (calo_collection_name.find("HcalEndcap") != std::string::npos) {
        return "HcalEndcapContributions";
    }
    return "";
}

std::string DataSource::getCorrespondingCaloCollection(const std::string& contrib_collection_name) const {
    // Reverse mapping for contribution collection names to calo collection names
    if (contrib_collection_name.find("EcalBarrelContributions") != std::string::npos) {
        return "EcalBarrelHits";
    } else if (contrib_collection_name.find("EcalEndcapContributions") != std::string::npos) {
        return "EcalEndcapHits";
    } else if (contrib_collection_name.find("HcalBarrelContributions") != std::string::npos) {
        return "HcalBarrelHits";
    } else if (contrib_collection_name.find("HcalEndcapContributions") != std::string::npos) {
        return "HcalEndcapHits";
    }
    return "";
}