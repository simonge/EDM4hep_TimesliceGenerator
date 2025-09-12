#include "StandaloneTimesliceMerger.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <TFile.h>
#include <TTree.h>
#include <TBranch.h>
#include <TLeaf.h>

// Simple structures representing EDM4HEP data for direct ROOT access
// These are simplified versions - in practice you'd want the full EDM4HEP structures

struct MCParticleData {
    int32_t PDG;
    int32_t generatorStatus;
    int32_t simulatorStatus;
    float charge;
    float time;
    double mass;
    // Position and momentum vectors
    double vertex_x, vertex_y, vertex_z;
    double momentum_x, momentum_y, momentum_z;
    // Parent/daughter indices would be handled separately
};

struct SimTrackerHitData {
    uint64_t cellID;
    float EDep;
    float time;
    float pathLength;
    int32_t quality;
    // Position
    double pos_x, pos_y, pos_z;
    // MCParticle reference index
    int32_t mc_particle_idx;
};

struct SimCalorimeterHitData {
    uint64_t cellID;
    float energy;
    float time;
    // Position  
    double pos_x, pos_y, pos_z;
    // Contributions are stored separately
};

struct CaloHitContributionData {
    int32_t PDG;
    float energy;
    float time;
    // Position
    double pos_x, pos_y, pos_z;
    // MCParticle reference index
    int32_t mc_particle_idx;
};

StandaloneTimesliceMerger::StandaloneTimesliceMerger(const MergerConfig& config)
    : m_config(config), gen(rd()), events_generated(0) {
    
}

void StandaloneTimesliceMerger::run() {
    std::cout << "Starting ROOT-based timeslice merger..." << std::endl;
    std::cout << "Sources: " << m_config.sources.size() << std::endl;
    std::cout << "Output file: " << m_config.output_file << std::endl;
    std::cout << "Max events: " << m_config.max_events << std::endl;
    std::cout << "Timeslice duration: " << m_config.time_slice_duration << std::endl;
    
    // Open output file
    auto output_file = std::unique_ptr<TFile>(TFile::Open(m_config.output_file.c_str(), "RECREATE"));
    if (!output_file || output_file->IsZombie()) {
        throw std::runtime_error("ERROR: Could not create output file: " + m_config.output_file);
    }

    auto inputs = initializeInputFiles();

    while (events_generated < m_config.max_events) {
        // Update number of events needed per source
        if (!updateInputNEvents(inputs)) break;
        createMergedTimeslice(inputs, output_file.get());

        events_generated++;
    }
    
    output_file->Close();
    std::cout << "Generated " << events_generated << " timeslices using ROOT dataframes" << std::endl;
}

// Initialize input files and validate sources using ROOT
std::vector<SourceReader> StandaloneTimesliceMerger::initializeInputFiles() {
    std::vector<SourceReader> source_readers(m_config.sources.size());
    
    // Initialize readers for each source using ROOT
    size_t source_idx = 0;

    // vector of collections to read
    std::vector<std::string> collections_to_read;

    for (auto& source_reader : source_readers) {
        const auto& source = m_config.sources[source_idx];
        source_reader.config = &source;

        if (!source.input_files.empty()) {
            try {
                // Create TChain for multiple files
                source_reader.chain = std::make_unique<TChain>(source.tree_name.c_str());
                
                for (const auto& file : source.input_files) {
                    std::cout << "Adding file: " << file << std::endl;
                    source_reader.chain->Add(file.c_str());
                }
                
                source_reader.total_entries = source_reader.chain->GetEntries();
                std::cout << "Total entries for source " << source_idx << ": " << source_reader.total_entries << std::endl;

                if (source_reader.total_entries == 0) {
                    throw std::runtime_error("ERROR: No entries found in tree '" + source.tree_name + "' for source " + std::to_string(source_idx));
                }

                // Get list of branches (collections) from the first file
                auto file_list = source_reader.chain->GetListOfFiles();
                if (file_list->GetEntries() > 0) {
                    // Open first file to inspect structure
                    auto first_file_obj = file_list->At(0);
                    auto first_file_name = first_file_obj->GetTitle();
                    auto temp_file = std::unique_ptr<TFile>(TFile::Open(first_file_name, "READ"));
                    if (temp_file && !temp_file->IsZombie()) {
                        auto temp_tree = dynamic_cast<TTree*>(temp_file->Get(source.tree_name.c_str()));
                        if (temp_tree) {
                            auto branches = temp_tree->GetListOfBranches();
                            std::cout << "Available branches in source " << source_idx << ":" << std::endl;
                            
                            for (int i = 0; i < branches->GetEntries(); ++i) {
                                auto branch = dynamic_cast<TBranch*>(branches->At(i));
                                if (branch) {
                                    std::string branch_name = branch->GetName();
                                    std::cout << "  - " << branch_name << std::endl;
                                    
                                    // Check if this is a collection we want to merge
                                    for (const auto& coll_name : collections_to_merge) {
                                        if (branch_name == coll_name) {
                                            source_reader.collection_names_to_read.push_back(branch_name);
                                            // Determine type from branch structure
                                            auto branch_title = branch->GetTitle();
                                            source_reader.collection_types_to_read.push_back(std::string(branch_title));
                                            if (source_idx == 0) {
                                                collections_to_read.push_back(coll_name);
                                            }
                                            break;
                                        }
                                    }
                                    
                                    // Also check for hit collections by name patterns
                                    if (!m_config.merge_particles) {
                                        for (const auto& hit_type : hit_collection_types) {
                                            if (branch_name.find(hit_type) != std::string::npos) {
                                                source_reader.collection_names_to_read.push_back(branch_name);
                                                source_reader.collection_types_to_read.push_back(hit_type);
                                                if (source_idx == 0) {
                                                    collections_to_read.push_back(branch_name);
                                                }
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        temp_file->Close();
                    }
                }

                // Validate collections found
                if (source_reader.collection_names_to_read.empty()) {
                    throw std::runtime_error("ERROR: No valid collections found in source " + std::to_string(source_idx));
                }

                // Setup branch addresses for efficient reading
                setupBranches(source_reader);

            } catch (const std::exception& e) {
                throw std::runtime_error("ERROR: Could not open input files for source " + std::to_string(source_idx) + ": " + e.what());
            }
        }
        ++source_idx;
    }

    return source_readers;
}

void StandaloneTimesliceMerger::setupBranches(SourceReader& reader) {
    for (const auto& collection_name : reader.collection_names_to_read) {
        // Initialize collection data storage
        CollectionData& coll_data = reader.collection_data[collection_name];
        coll_data.name = collection_name;
        
        // Get branch and setup for reading
        TBranch* branch = reader.chain->GetBranch(collection_name.c_str());
        if (branch) {
            reader.branches[collection_name] = branch;
            
            // Allocate buffer based on expected size
            // This is a simplified approach - in practice you'd want to dynamically resize
            coll_data.data.resize(1024 * 1024); // 1MB initial buffer
            
            // Set branch address to our buffer
            reader.chain->SetBranchAddress(collection_name.c_str(), coll_data.data.data());
        }
    }
}

// Update number of events needed per source for next timeslice
bool StandaloneTimesliceMerger::updateInputNEvents(std::vector<SourceReader>& inputs) {
    for (auto& source_reader : inputs) {
        const auto& config = source_reader.config;
        
        if (source_reader.current_entry_index >= source_reader.total_entries) {
            std::cout << "Source " << config->name << " exhausted." << std::endl;
            return false;
        }

        if (config->static_number_of_events) {
            source_reader.entries_needed = config->static_events_per_timeslice;
        } else {
            // Poisson distribution based on mean frequency
            std::poisson_distribution<size_t> poisson_dist(config->mean_event_frequency * m_config.time_slice_duration);
            source_reader.entries_needed = poisson_dist(gen);
            
            // Ensure at least one event
            if (source_reader.entries_needed == 0) {
                source_reader.entries_needed = 1;
            }
        }
        
        // Don't exceed available entries
        if (source_reader.current_entry_index + source_reader.entries_needed > source_reader.total_entries) {
            source_reader.entries_needed = source_reader.total_entries - source_reader.current_entry_index;
        }
        
        std::cout << "Source " << config->name << " will process " << source_reader.entries_needed << " entries" << std::endl;
    }
    return true;
}

void StandaloneTimesliceMerger::createMergedTimeslice(std::vector<SourceReader>& inputs, TFile* output_file) {
    std::cout << "Creating timeslice " << events_generated << std::endl;
    
    // Storage for merged collections
    std::unordered_map<std::string, std::vector<char>> merged_collections;
    
    // Processing info for each source
    std::vector<ProcessingInfo> processing_infos(inputs.size());
    
    // Initialize base indices for reference mapping
    size_t total_particle_count = 0;
    
    // First pass: calculate offsets and generate time offsets
    for (size_t source_idx = 0; source_idx < inputs.size(); ++source_idx) {
        auto& source_reader = inputs[source_idx];
        auto& proc_info = processing_infos[source_idx];
        
        // Generate time offset for this source
        float distance = 0.0f; // Would calculate based on beam parameters
        proc_info.time_offset = generateTimeOffset(*source_reader.config, distance);
        proc_info.particle_index_offset = total_particle_count;
        
        // Estimate particle count for this source (simplified)
        // In practice, you'd read the actual count
        total_particle_count += source_reader.entries_needed * 10; // Rough estimate
        
        std::cout << "Source " << source_idx << " time offset: " << proc_info.time_offset 
                  << " particle offset: " << proc_info.particle_index_offset << std::endl;
    }
    
    // Second pass: read and merge data from each source
    for (size_t source_idx = 0; source_idx < inputs.size(); ++source_idx) {
        auto& source_reader = inputs[source_idx];
        auto& proc_info = processing_infos[source_idx];
        
        mergeCollectionsFromRoot(source_reader, proc_info, merged_collections);
    }
    
    // Write merged collections to output file
    output_file->cd();
    TTree* output_tree = new TTree("events", "Merged timeslice events");
    
    for (const auto& [collection_name, data] : merged_collections) {
        std::string branch_name = collection_name;
        // Create branch for this collection
        TBranch* branch = output_tree->Branch(branch_name.c_str(), 
                                            const_cast<char*>(data.data()), 
                                            32000); // Use default buffer size
        if (!branch) {
            std::cerr << "WARNING: Could not create branch for " << collection_name << std::endl;
        }
    }
    
    // Fill the tree with one entry (one timeslice)
    output_tree->Fill();
    output_tree->Write();
    
    std::cout << "Timeslice " << events_generated << " created with " << merged_collections.size() << " collections" << std::endl;
}

void StandaloneTimesliceMerger::mergeCollectionsFromRoot(SourceReader& source, 
                                                       ProcessingInfo& proc_info,
                                                       std::unordered_map<std::string, std::vector<char>>& merged_collections) {
    
    const auto& config = *source.config;
    
    for (size_t i = 0; i < source.entries_needed; ++i) {
        // Read entry
        Long64_t entry_idx = source.current_entry_index + i;
        source.chain->GetEntry(entry_idx);
        
        // Process each collection for this entry
        for (const auto& collection_name : source.collection_names_to_read) {
            readCollectionData(source, collection_name);
            
            // Get the collection data
            auto& coll_data = source.collection_data[collection_name];
            
            // Apply time updates and reference mapping
            if (collection_name == "MCParticles") {
                updateParticleTiming(coll_data.data, proc_info.time_offset, config.generator_status_offset);
            } else if (collection_name.find("TrackerHit") != std::string::npos) {
                updateHitTiming(coll_data.data, "SimTrackerHit", proc_info.time_offset);
                updateHitReferences(coll_data.data, "SimTrackerHit", proc_info.particle_index_offset);
            } else if (collection_name.find("CalorimeterHit") != std::string::npos) {
                updateHitTiming(coll_data.data, "SimCalorimeterHit", proc_info.time_offset);
            } else if (collection_name.find("Contribution") != std::string::npos) {
                updateContributionReferences(coll_data.data, proc_info.particle_index_offset);
            }
            
            // Merge with output collection
            auto& merged_data = merged_collections[collection_name];
            merged_data.insert(merged_data.end(), coll_data.data.begin(), 
                              coll_data.data.begin() + coll_data.size);
        }
    }
    
    // Update source position
    source.current_entry_index += source.entries_needed;
}

// Helper functions for time updates and reference handling
void StandaloneTimesliceMerger::updateParticleTiming(std::vector<char>& particle_data, float time_offset, int32_t status_offset) {
    // Cast to MCParticleData array and update timing
    // This is a simplified implementation - real EDM4HEP would have more complex structures
    MCParticleData* particles = reinterpret_cast<MCParticleData*>(particle_data.data());
    size_t num_particles = particle_data.size() / sizeof(MCParticleData);
    
    for (size_t i = 0; i < num_particles; ++i) {
        particles[i].time += time_offset;
        particles[i].generatorStatus += status_offset;
    }
}

void StandaloneTimesliceMerger::updateHitTiming(std::vector<char>& hit_data, const std::string& hit_type, float time_offset) {
    if (hit_type == "SimTrackerHit") {
        SimTrackerHitData* hits = reinterpret_cast<SimTrackerHitData*>(hit_data.data());
        size_t num_hits = hit_data.size() / sizeof(SimTrackerHitData);
        
        for (size_t i = 0; i < num_hits; ++i) {
            hits[i].time += time_offset;
        }
    } else if (hit_type == "SimCalorimeterHit") {
        SimCalorimeterHitData* hits = reinterpret_cast<SimCalorimeterHitData*>(hit_data.data());
        size_t num_hits = hit_data.size() / sizeof(SimCalorimeterHitData);
        
        for (size_t i = 0; i < num_hits; ++i) {
            hits[i].time += time_offset;
        }
    }
}

void StandaloneTimesliceMerger::updateHitReferences(std::vector<char>& hit_data, const std::string& hit_type, size_t particle_offset) {
    if (hit_type == "SimTrackerHit") {
        SimTrackerHitData* hits = reinterpret_cast<SimTrackerHitData*>(hit_data.data());
        size_t num_hits = hit_data.size() / sizeof(SimTrackerHitData);
        
        for (size_t i = 0; i < num_hits; ++i) {
            if (hits[i].mc_particle_idx >= 0) {
                hits[i].mc_particle_idx += particle_offset;
            }
        }
    }
}

void StandaloneTimesliceMerger::updateContributionReferences(std::vector<char>& contrib_data, size_t particle_offset) {
    CaloHitContributionData* contribs = reinterpret_cast<CaloHitContributionData*>(contrib_data.data());
    size_t num_contribs = contrib_data.size() / sizeof(CaloHitContributionData);
    
    for (size_t i = 0; i < num_contribs; ++i) {
        if (contribs[i].mc_particle_idx >= 0) {
            contribs[i].mc_particle_idx += particle_offset;
        }
    }
}

void StandaloneTimesliceMerger::readCollectionData(SourceReader& reader, const std::string& collection_name) {
    // The data is already read into the buffer by GetEntry()
    // We just need to determine the actual size used
    
    auto& coll_data = reader.collection_data[collection_name];
    TBranch* branch = reader.branches[collection_name];
    
    if (branch) {
        // Get the number of bytes read for this branch in the last GetEntry call
        coll_data.size = branch->GetEntry();
    }
}

float StandaloneTimesliceMerger::generateTimeOffset(SourceConfig sourceConfig, float distance) {
    if (!sourceConfig.introduce_offsets) {
        return 0.0f;
    }
    
    std::uniform_real_distribution<float> offset_dist(0.0f, m_config.time_slice_duration);
    float time_offset = offset_dist(gen);
    
    if (sourceConfig.use_bunch_crossing) {
        // Discretize to bunch crossing boundaries
        time_offset = std::floor(time_offset / m_config.bunch_crossing_period) * m_config.bunch_crossing_period;
    }
    
    if (sourceConfig.attach_to_beam) {
        // Add time-of-flight correction
        float tof = distance / sourceConfig.beam_speed;
        
        // Add Gaussian smearing
        if (sourceConfig.beam_spread > 0.0f) {
            std::normal_distribution<float> beam_dist(0.0f, sourceConfig.beam_spread);
            tof += beam_dist(gen);
        }
        
        time_offset += tof;
    }
    
    return time_offset;
}

std::vector<std::string> StandaloneTimesliceMerger::getCollectionNames(const SourceReader& reader, const std::string& type) {
    std::vector<std::string> names;
    
    for (size_t i = 0; i < reader.collection_types_to_read.size(); ++i) {
        if (reader.collection_types_to_read[i] == type) {
            names.push_back(reader.collection_names_to_read[i]);
        }
    }
    
    return names;
}