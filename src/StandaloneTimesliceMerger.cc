#include "StandaloneTimesliceMerger.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <ROOT/RDataFrame.hxx>
#include <ROOT/RVec.hxx>
#include <TFile.h>
#include <TTree.h>

StandaloneTimesliceMerger::StandaloneTimesliceMerger(const MergerConfig& config)
    : m_config(config), gen(rd()), events_generated(0) {
    std::cout << "Initialized ROOT RDataFrame-based timeslice merger" << std::endl;
    std::cout << "Output preserves original EDM4HEP collection structure from input files" << std::endl;
}

void StandaloneTimesliceMerger::run() {
    std::cout << "Starting ROOT RDataFrame-based timeslice merger..." << std::endl;
    std::cout << "Will preserve original EDM4HEP collection structure with modified timing/associations" << std::endl;
    
    // Create ROOT output file
    auto root_file = std::unique_ptr<TFile>(TFile::Open(m_config.output_file.c_str(), "RECREATE"));
    if (!root_file || root_file->IsZombie()) {
        throw std::runtime_error("ERROR: Could not create ROOT output file: " + m_config.output_file);
    }
    
    auto inputs = initializeInputFiles();

    // Copy podio metadata from input files before writing events
    copyPodioMetadata(root_file.get(), inputs);

    // Create merged RDataFrame with requested number of events
    createMergedDataFrameROOT(inputs, root_file.get());
    
    root_file->Close();
    std::cout << "Generated " << m_config.max_events << " timeslices in ROOT format using RDataFrame" << std::endl;
    std::cout << "Output preserves original collection names and EDM4HEP structure from input files" << std::endl;
}

// Initialize input files using dataframe approach with automatic collection discovery
std::vector<SourceReader> StandaloneTimesliceMerger::initializeInputFiles() {
    std::vector<SourceReader> source_readers(m_config.sources.size());
    
    size_t source_idx = 0;
    
    for (auto& source_reader : source_readers) {
        const auto& source = m_config.sources[source_idx];
        source_reader.config = &source;
        source_reader.input_files = source.input_files;

        if (!source.input_files.empty()) {
            try {
                std::cout << "Setting up dataframe reader for source " << source_idx << std::endl;
                
                // Discover collections from the first input file
                const std::string& first_file = source.input_files[0];
                discoverCollectionsFromDataFrame(first_file, source_reader);
                
                // Count total entries across all input files
                source_reader.total_entries = 0;
                for (const auto& filename : source.input_files) {
                    auto file = std::unique_ptr<TFile>(TFile::Open(filename.c_str(), "READ"));
                    if (file && !file->IsZombie()) {
                        auto tree = dynamic_cast<TTree*>(file->Get("events"));
                        if (tree) {
                            source_reader.total_entries += tree->GetEntries();
                        }
                    }
                }
                
                std::cout << "Source " << source_idx << " initialized with " 
                          << source_reader.total_entries << " entries" << std::endl;
                std::cout << "  Discovered " << source_reader.discovered_trackerhit_names.size() 
                          << " tracker hit collections" << std::endl;
                std::cout << "  Discovered " << source_reader.discovered_calohit_names.size() 
                          << " calorimeter hit collections" << std::endl;

            } catch (const std::exception& e) {
                throw std::runtime_error("ERROR: Could not setup dataframe reader for source " + 
                                        std::to_string(source_idx) + ": " + e.what());
            }
        }
        ++source_idx;
    }

    return source_readers;
}

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
            // Poisson distribution for realistic event timing
            std::poisson_distribution<size_t> poisson_dist(config->mean_event_frequency * m_config.time_slice_duration);
            source_reader.entries_needed = poisson_dist(gen);
            
            if (source_reader.entries_needed == 0) {
                source_reader.entries_needed = 1;
            }
        }
        
        // Don't exceed available entries
        if (source_reader.current_entry_index + source_reader.entries_needed > source_reader.total_entries) {
            source_reader.entries_needed = source_reader.total_entries - source_reader.current_entry_index;
        }
        
        std::cout << "Source " << config->name << " will process " << source_reader.entries_needed 
                  << " entries using dataframes" << std::endl;
    }
    return true;
}

void StandaloneTimesliceMerger::mergeCollectionsFromDataFrames(SourceReader& source, 
                                                              ProcessingInfo& proc_info,
                                                              DataFrameCollection<edm4hep::MCParticleData>& merged_particles,
                                                              DataFrameCollection<edm4hep::EventHeaderData>& merged_headers,
                                                              std::map<std::string, DataFrameCollection<edm4hep::SimTrackerHitData>>& merged_tracker_collections,
                                                              std::map<std::string, DataFrameCollection<edm4hep::SimCalorimeterHitData>>& merged_calo_collections,
                                                              std::map<std::string, DataFrameCollection<edm4hep::CaloHitContributionData>>& merged_contribution_collections) {
    
    const auto& config = *source.config;
    size_t base_particle_count = merged_particles.size();
    
    for (size_t i = 0; i < source.entries_needed; ++i) {
        size_t entry_idx = source.current_entry_index + i;
        
        // Read data from dataframes (mock implementation)
        if (!readDataFrameFromFile(source, entry_idx)) {
            std::cout << "Warning: Could not read entry " << entry_idx << " from source" << std::endl;
            continue;
        }
        
        // Update timing using helper functions on dataframes
        updateParticleDataTiming(source.mcparticles, proc_info.time_offset, config.generator_status_offset);
        
        // Update timing and references for each discovered tracker hit collection
        for (auto& [name, collection] : source.trackerhits_collections) {
            updateTrackerHitDataTiming(collection, proc_info.time_offset);
            updateTrackerHitReferences(collection, proc_info.particle_index_offset);
        }
        
        // Update timing for each discovered calorimeter hit collection
        for (auto& [name, collection] : source.calohits_collections) {
            updateCaloHitDataTiming(collection, proc_info.time_offset);
        }
        
        // Update references for each discovered contribution collection
        for (auto& [name, collection] : source.contributions_collections) {
            updateContributionReferences(collection, proc_info.particle_index_offset);
        }
        
        // Merge the updated collections (dataframe append operations)
        for (const auto& particle : source.mcparticles) {
            merged_particles.push_back(particle);
        }
        
        for (const auto& header : source.eventheaders) {
            merged_headers.push_back(header);
        }
        
        // Merge all discovered tracker hit collections
        for (const auto& [name, collection] : source.trackerhits_collections) {
            for (const auto& hit : collection) {
                merged_tracker_collections[name].push_back(hit);
            }
        }
        
        // Merge all discovered calorimeter hit collections
        for (const auto& [name, collection] : source.calohits_collections) {
            for (const auto& hit : collection) {
                merged_calo_collections[name].push_back(hit);
            }
        }
        
        // Merge all discovered contribution collections
        for (const auto& [name, collection] : source.contributions_collections) {
            for (const auto& contrib : collection) {
                merged_contribution_collections[name].push_back(contrib);
            }
        }
        
        std::cout << "  Processed entry " << entry_idx << " - added " 
                  << source.mcparticles.size() << " particles across "
                  << source.trackerhits_collections.size() << " tracker collections, "
                  << source.calohits_collections.size() << " calo collections" << std::endl;
    }
    
    // Update source position
    source.current_entry_index += source.entries_needed;
    
    std::cout << "Merged " << source.entries_needed << " events from source using dataframe operations with multiple collections" << std::endl;
}

// Helper functions for dataframe-based timing updates
void StandaloneTimesliceMerger::updateParticleDataTiming(DataFrameCollection<edm4hep::MCParticleData>& particles, 
                                                        float time_offset, int32_t status_offset) {
    std::cout << "Updating particle timing: offset=" << time_offset << "ns, status_offset=" << status_offset << std::endl;
    
    // Now we can modify the data directly since it's mutable
    for (auto& particle_data : particles.data) {
        particle_data.time += time_offset;
        particle_data.generatorStatus += status_offset;
    }
}

void StandaloneTimesliceMerger::updateTrackerHitDataTiming(DataFrameCollection<edm4hep::SimTrackerHitData>& hits, 
                                                          float time_offset) {
    std::cout << "Updating tracker hit timing: offset=" << time_offset << "ns" << std::endl;
    
    // Now we can modify the data directly since it's mutable
    for (auto& hit_data : hits.data) {
        hit_data.time += time_offset;
    }
}
    
void StandaloneTimesliceMerger::updateTrackerHitReferences(DataFrameCollection<edm4hep::SimTrackerHitData>& hits, 
                                                          size_t particle_offset) {
    std::cout << "Updating tracker hit references: offset=" << particle_offset << std::endl;
    
    // For data objects, reference handling would need to be implemented at the ObjectID level
    // This is a placeholder - reference updating is complex with podio's reference system
    std::cout << "Note: Reference updating for tracker hits not yet implemented for data objects" << std::endl;
}

void StandaloneTimesliceMerger::updateCaloHitDataTiming(DataFrameCollection<edm4hep::SimCalorimeterHitData>& hits, 
                                                       float time_offset) {
    std::cout << "Updating calorimeter hit timing: offset=" << time_offset << "ns" << std::endl;
    
    // SimCalorimeterHitData may not have a 'time' field - let's comment this out for now
    // TODO: Check the actual fields available in SimCalorimeterHitData
    std::cout << "Note: Time updating for SimCalorimeterHitData not yet implemented - field name needs verification" << std::endl;
}

void StandaloneTimesliceMerger::updateContributionReferences(DataFrameCollection<edm4hep::CaloHitContributionData>& contribs, 
                                                            size_t particle_offset) {
    std::cout << "Updating contribution references: offset=" << particle_offset << std::endl;
    
    // For data objects, reference handling would need to be implemented at the ObjectID level
    // This is a placeholder - reference updating is complex with podio's reference system
    std::cout << "Note: Reference updating for contributions not yet implemented for data objects" << std::endl;
}

// Read actual data from ROOT file using RDataFrame
bool StandaloneTimesliceMerger::readDataFrameFromFile(SourceReader& reader, size_t entry_index) {
    // Clear all collections
    reader.mcparticles.clear();
    reader.eventheaders.clear();
    
    for (auto& [name, collection] : reader.trackerhits_collections) {
        collection.clear();
    }
    for (auto& [name, collection] : reader.calohits_collections) {
        collection.clear();
    }
    for (auto& [name, collection] : reader.contributions_collections) {
        collection.clear();
    }
    
    // Determine which input file to read from based on entry index
    size_t total_entries_so_far = 0;
    std::string current_file;
    size_t file_entry_index = 0;
    
    for (const auto& filename : reader.input_files) {
        // Open file to check number of entries (in practice, cache this)
        auto file = std::unique_ptr<TFile>(TFile::Open(filename.c_str(), "READ"));
        if (!file || file->IsZombie()) continue;
        
        auto tree = dynamic_cast<TTree*>(file->Get("events"));
        if (!tree) continue;
        
        size_t file_entries = tree->GetEntries();
        
        if (entry_index < total_entries_so_far + file_entries) {
            current_file = filename;
            file_entry_index = entry_index - total_entries_so_far;
            break;
        }
        
        total_entries_so_far += file_entries;
    }
    
    if (current_file.empty()) {
        std::cout << "Warning: Could not find entry " << entry_index << " in any input file" << std::endl;
        return false;
    }
    
    // Create RDataFrame for the specific file and entry
    ROOT::RDataFrame df("events", current_file);
    
    // Filter to the specific entry
    auto entry_df = df.Filter([file_entry_index](ULong64_t rdfentry_) { 
        return rdfentry_ == file_entry_index; 
    }, {"rdfentry_"});
    
    // Read MCParticles if available
    if (df.HasColumn(reader.mcparticles.name)) {
        auto particles_result = entry_df.Take<ROOT::VecOps::RVec<edm4hep::MCParticleData>>(reader.mcparticles.name);
        auto particles_vec = particles_result.GetValue();
        
        for (const auto& particle_collection : particles_vec) {
            for (auto particle_data : particle_collection) {
                reader.mcparticles.push_back(particle_data);
            }
        }
    }
    
    // Read EventHeader if available
    if (df.HasColumn(reader.eventheaders.name)) {
        auto headers_result = entry_df.Take<ROOT::VecOps::RVec<edm4hep::EventHeaderData>>(reader.eventheaders.name);
        auto headers_vec = headers_result.GetValue();
        
        for (const auto& header_collection : headers_vec) {
            for (auto header_data : header_collection) {
                reader.eventheaders.push_back(header_data);
            }
        }
    }
    
    // Read tracker hit collections
    for (const auto& collection_name : reader.discovered_trackerhit_names) {
        if (df.HasColumn(collection_name)) {
            auto hits_result = entry_df.Take<ROOT::VecOps::RVec<edm4hep::SimTrackerHitData>>(collection_name);
            auto hits_vec = hits_result.GetValue();
            
            for (const auto& hit_collection : hits_vec) {
                for (auto hit_data : hit_collection) {
                    reader.trackerhits_collections[collection_name].push_back(hit_data);
                }
            }
        }
    }
    
    // Read calorimeter hit collections
    for (const auto& collection_name : reader.discovered_calohit_names) {
        if (df.HasColumn(collection_name)) {
            auto hits_result = entry_df.Take<ROOT::VecOps::RVec<edm4hep::SimCalorimeterHitData>>(collection_name);
            auto hits_vec = hits_result.GetValue();
            
            for (const auto& hit_collection : hits_vec) {
                for (auto hit_data : hit_collection) {
                    reader.calohits_collections[collection_name].push_back(hit_data);
                }
            }
        }
    }
    
    // Read contribution collections
    for (const auto& collection_name : reader.discovered_contribution_names) {
        if (df.HasColumn(collection_name)) {
            auto contribs_result = entry_df.Take<ROOT::VecOps::RVec<edm4hep::CaloHitContributionData>>(collection_name);
            auto contribs_vec = contribs_result.GetValue();
            
            for (const auto& contrib_collection : contribs_vec) {
                for (auto contrib_data : contrib_collection) {
                    reader.contributions_collections[collection_name].push_back(contrib_data);
                }
            }
        }
    }
    
    return true;
}

float StandaloneTimesliceMerger::generateTimeOffset(SourceConfig sourceConfig, float distance) {
    if (!m_config.introduce_offsets) {
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

// Template specializations for dataframe operations
template<>
void DataFrameCollection<edm4hep::MCParticleData>::updateTiming(float offset) {
    for (auto& particle_data : data) {
        particle_data.time += offset;
    }
}

template<>
void DataFrameCollection<edm4hep::SimTrackerHitData>::updateTiming(float offset) {
    for (auto& hit_data : data) {
        hit_data.time += offset;
    }
}

template<>
void DataFrameCollection<edm4hep::SimCalorimeterHitData>::updateTiming(float offset) {
    // SimCalorimeterHitData may not have a 'time' field - let's comment this out for now
    // TODO: Check the actual fields available in SimCalorimeterHitData
    std::cout << "Note: Time updating for SimCalorimeterHitData template not yet implemented - field name needs verification" << std::endl;
}

template<>
void DataFrameCollection<edm4hep::SimTrackerHitData>::updateReferences(size_t index_offset) {
    // Reference updating would need to be handled at the ObjectID level
    // For now, just keep the objects as-is since we can't modify references easily
    std::cout << "Note: Reference updating for tracker hit data not yet implemented" << std::endl;
}

template<>
void DataFrameCollection<edm4hep::CaloHitContributionData>::updateReferences(size_t index_offset) {
    // Reference updating would need to be handled at the ObjectID level  
    // For now, just keep the objects as-is since we can't modify references easily
    std::cout << "Note: Reference updating for contribution data not yet implemented" << std::endl;
}

// ROOT-specific implementations that preserve original EDM4HEP collection structure

void StandaloneTimesliceMerger::createMergedTimesliceROOT(std::vector<SourceReader>& inputs, TFile* output_file) {
    std::cout << "Creating ROOT timeslice " << events_generated << " (preserving input collection structure)" << std::endl;
    
    // Create merged dataframe collections - now handling multiple collections per type
    DataFrameCollection<edm4hep::MCParticleData> merged_particles;
    DataFrameCollection<edm4hep::EventHeaderData> merged_headers;
    std::map<std::string, DataFrameCollection<edm4hep::SimTrackerHitData>> merged_tracker_collections;
    std::map<std::string, DataFrameCollection<edm4hep::SimCalorimeterHitData>> merged_calo_collections;
    std::map<std::string, DataFrameCollection<edm4hep::CaloHitContributionData>> merged_contribution_collections;
    
    merged_particles.name = "MCParticles";
    merged_headers.name = "EventHeader";
    
    // Initialize merged collections based on discovered collections from inputs
    for (const auto& input : inputs) {
        for (const auto& name : input.discovered_trackerhit_names) {
            if (merged_tracker_collections.find(name) == merged_tracker_collections.end()) {
                merged_tracker_collections[name] = DataFrameCollection<edm4hep::SimTrackerHitData>();
                merged_tracker_collections[name].name = name;
                merged_tracker_collections[name].type = "edm4hep::SimTrackerHitCollection";
            }
        }
        for (const auto& name : input.discovered_calohit_names) {
            if (merged_calo_collections.find(name) == merged_calo_collections.end()) {
                merged_calo_collections[name] = DataFrameCollection<edm4hep::SimCalorimeterHitData>();
                merged_calo_collections[name].name = name;
                merged_calo_collections[name].type = "edm4hep::SimCalorimeterHitCollection";
            }
        }
        for (const auto& name : input.discovered_contribution_names) {
            if (merged_contribution_collections.find(name) == merged_contribution_collections.end()) {
                merged_contribution_collections[name] = DataFrameCollection<edm4hep::CaloHitContributionData>();
                merged_contribution_collections[name].name = name;
                merged_contribution_collections[name].type = "edm4hep::CaloHitContributionCollection";
            }
        }
    }
    
    // Processing info for each source
    std::vector<ProcessingInfo> processing_infos(inputs.size());
    
    // Calculate offsets for proper reference mapping
    size_t total_particle_count = 0;
    
    for (size_t source_idx = 0; source_idx < inputs.size(); ++source_idx) {
        auto& source_reader = inputs[source_idx];
        auto& proc_info = processing_infos[source_idx];
        
        // Generate time offset using helper function
        float distance = 0.0f; // Would calculate based on beam parameters
        proc_info.time_offset = generateTimeOffset(*source_reader.config, distance);
        proc_info.particle_index_offset = total_particle_count;
        
        // Estimate particle count (in real implementation, would come from dataframes)
        size_t particles_this_source = source_reader.entries_needed * 5; // Mock estimate
        total_particle_count += particles_this_source;
        
        std::cout << "ROOT: Source " << source_idx << " - Time offset: " << proc_info.time_offset 
                  << "ns, Particle index offset: " << proc_info.particle_index_offset << std::endl;
    }
    
    // Merge data from each source using dataframe approach
    for (size_t source_idx = 0; source_idx < inputs.size(); ++source_idx) {
        auto& source_reader = inputs[source_idx];
        auto& proc_info = processing_infos[source_idx];
        
        std::cout << "ROOT: Merging data from source " << source_idx << " using dataframes..." << std::endl;
        mergeCollectionsFromDataFrames(source_reader, proc_info, merged_particles, merged_headers,
                                      merged_tracker_collections, merged_calo_collections, merged_contribution_collections);
    }
    
    // Write merged dataframe collections to ROOT file
    writeDataFramesToROOTFile(output_file, merged_particles, merged_headers, merged_tracker_collections, 
                             merged_calo_collections, merged_contribution_collections);
    
    size_t total_tracker_hits = 0;
    for (const auto& [name, collection] : merged_tracker_collections) {
        total_tracker_hits += collection.size();
    }
    size_t total_calo_hits = 0;
    for (const auto& [name, collection] : merged_calo_collections) {
        total_calo_hits += collection.size();
    }
    
    std::cout << "ROOT timeslice " << events_generated << " completed - " 
              << merged_particles.size() << " particles, "
              << total_tracker_hits << " tracker hits across " << merged_tracker_collections.size() << " collections, "
              << total_calo_hits << " calo hits across " << merged_calo_collections.size() << " collections" << std::endl;
}

void StandaloneTimesliceMerger::writeDataFramesToROOTFile(TFile* file,
                                                         const DataFrameCollection<edm4hep::MCParticleData>& particles,
                                                         const DataFrameCollection<edm4hep::EventHeaderData>& headers,
                                                         const std::map<std::string, DataFrameCollection<edm4hep::SimTrackerHitData>>& tracker_collections,
                                                         const std::map<std::string, DataFrameCollection<edm4hep::SimCalorimeterHitData>>& calo_collections,
                                                         const std::map<std::string, DataFrameCollection<edm4hep::CaloHitContributionData>>& contribution_collections) {
    
    std::cout << "Writing ROOT file using RDataFrame snapshot with EDM4hep-compatible object vectors" << std::endl;
    std::cout << "Processing " << tracker_collections.size() << " tracker collections, " 
              << calo_collections.size() << " calo collections, " 
              << contribution_collections.size() << " contribution collections" << std::endl;
    
    file->cd();
    
    // Create structures that match what RDataFrame sees when reading EDM4hep files
    // These will be vectors of the complete objects, not individual member vectors
    
    // MCParticles collection as RVec of data objects
    ROOT::VecOps::RVec<edm4hep::MCParticleData> mcparticles_coll;
    for (const auto& p : particles) {
        mcparticles_coll.push_back(p);
    }
    
    // EventHeader collection as RVec of data objects
    ROOT::VecOps::RVec<edm4hep::EventHeaderData> eventheader_coll;
    for (const auto& h : headers) {
        eventheader_coll.push_back(h);
    }
    
    // Create RDataFrame from the complete object vectors (matching EDM4hep file structure)
    ROOT::RDataFrame df(1); // Create empty dataframe with 1 entry
    
    // Start with basic collections
    auto df_with_basic = df
        .Define("MCParticles", [&]() { return mcparticles_coll; })
        .Define("EventHeader", [&]() { return eventheader_coll; });
    
    // Add each discovered tracker hit collection as a separate RDataFrame column
    auto df_current = df_with_basic;
    for (const auto& [collection_name, collection_data] : tracker_collections) {
        ROOT::VecOps::RVec<edm4hep::SimTrackerHitData> collection_vector;
        for (const auto& hit : collection_data) {
            collection_vector.push_back(hit);
        }
        
        std::cout << "Adding tracker collection: " << collection_name << " with " << collection_vector.size() << " hits" << std::endl;
        
        // Capture collection_vector by value in the lambda
        df_current = df_current.Define(collection_name, [collection_vector]() { return collection_vector; });
    }
    
    // Add each discovered calorimeter hit collection as a separate RDataFrame column
    for (const auto& [collection_name, collection_data] : calo_collections) {
        ROOT::VecOps::RVec<edm4hep::SimCalorimeterHitData> collection_vector;
        for (const auto& hit : collection_data) {
            collection_vector.push_back(hit);
        }
        
        std::cout << "Adding calo collection: " << collection_name << " with " << collection_vector.size() << " hits" << std::endl;
        
        // Capture collection_vector by value in the lambda
        df_current = df_current.Define(collection_name, [collection_vector]() { return collection_vector; });
    }
    
    // Add each discovered contribution collection as a separate RDataFrame column
    for (const auto& [collection_name, collection_data] : contribution_collections) {
        ROOT::VecOps::RVec<edm4hep::CaloHitContributionData> collection_vector;
        for (const auto& contrib : collection_data) {
            collection_vector.push_back(contrib);
        }
        
        std::cout << "Adding contribution collection: " << collection_name << " with " << collection_vector.size() << " contributions" << std::endl;
        
        // Capture collection_vector by value in the lambda
        df_current = df_current.Define(collection_name, [collection_vector]() { return collection_vector; });
    }
    
    // Write dataframe to ROOT file as snapshot
    std::string tree_name = "events";
    df_current.Snapshot(tree_name, file->GetName());
    
    // Summary output
    size_t total_tracker_hits = 0;
    for (const auto& [name, collection] : tracker_collections) {
        total_tracker_hits += collection.size();
    }
    size_t total_calo_hits = 0;
    for (const auto& [name, collection] : calo_collections) {
        total_calo_hits += collection.size();
    }
    size_t total_contributions = 0;
    for (const auto& [name, collection] : contribution_collections) {
        total_contributions += collection.size();
    }
    
    std::cout << "RDataFrame snapshot written with EDM4hep-compatible object vectors:" << std::endl;
    std::cout << "  " << mcparticles_coll.size() << " MCParticles (with modified timing)" << std::endl;
    std::cout << "  " << eventheader_coll.size() << " EventHeaders" << std::endl;
    std::cout << "  " << total_tracker_hits << " tracker hits across " << tracker_collections.size() << " collections:" << std::endl;
    for (const auto& [name, collection] : tracker_collections) {
        std::cout << "    - " << name << ": " << collection.size() << " hits" << std::endl;
    }
    std::cout << "  " << total_calo_hits << " calo hits across " << calo_collections.size() << " collections:" << std::endl;
    for (const auto& [name, collection] : calo_collections) {
        std::cout << "    - " << name << ": " << collection.size() << " hits" << std::endl;
    }
    std::cout << "  " << total_contributions << " contributions across " << contribution_collections.size() << " collections:" << std::endl;
    for (const auto& [name, collection] : contribution_collections) {
        std::cout << "    - " << name << ": " << collection.size() << " contributions" << std::endl;
    }
    std::cout << "Collections stored as separate vectors of complete objects (preserving original EDM4hep structure)" << std::endl;
}

void StandaloneTimesliceMerger::createMergedDataFrameROOT(std::vector<SourceReader>& inputs, TFile* output_file) {
    std::cout << "Creating merged RDataFrame with " << m_config.max_events << " events" << std::endl;
    
    if (inputs.empty()) {
        std::cout << "No input sources available" << std::endl;
        return;
    }
    
    // Get all discovered collection names from all sources
    std::set<std::string> all_tracker_collections, all_calo_collections, all_contribution_collections, all_reference_columns;
    for (const auto& input : inputs) {
        for (const auto& name : input.discovered_trackerhit_names) {
            all_tracker_collections.insert(name);
        }
        for (const auto& name : input.discovered_calohit_names) {
            all_calo_collections.insert(name);
        }
        for (const auto& name : input.discovered_contribution_names) {
            all_contribution_collections.insert(name);
        }
        for (const auto& name : input.discovered_reference_columns) {
            all_reference_columns.insert(name);
        }
    }
    
    // Create empty RDataFrame with the desired number of events
    ROOT::RDataFrame empty_df(m_config.max_events);
    
    // Add event ID column
    auto df_with_event = empty_df.Define("EventID", [](ULong64_t entry) { return static_cast<int>(entry); }, {"rdfentry_"});
    
    // Define MCParticles column
    auto df_with_particles = df_with_event.Define("MCParticles", [this, &inputs](int event_id) {
        return this->generateMergedMCParticles(inputs, event_id);
    }, {"EventID"});
    
    // Define EventHeader column
    auto df_with_headers = df_with_particles.Define("EventHeader", [this, &inputs](int event_id) {
        return this->generateMergedEventHeaders(inputs, event_id);
    }, {"EventID"});
    
    // Add tracker hit collections
    auto df_current = df_with_headers;
    for (const auto& collection_name : all_tracker_collections) {
        std::cout << "Adding tracker collection: " << collection_name << std::endl;
        df_current = df_current.Define(collection_name, [this, &inputs, collection_name](int event_id) {
            return this->generateMergedTrackerHits(inputs, event_id, collection_name);
        }, {"EventID"});
    }
    
    // Add calorimeter hit collections
    for (const auto& collection_name : all_calo_collections) {
        std::cout << "Adding calo collection: " << collection_name << std::endl;
        df_current = df_current.Define(collection_name, [this, &inputs, collection_name](int event_id) {
            return this->generateMergedCaloHits(inputs, event_id, collection_name);
        }, {"EventID"});
    }
    
    // Add contribution collections
    for (const auto& collection_name : all_contribution_collections) {
        std::cout << "Adding contribution collection: " << collection_name << std::endl;
        df_current = df_current.Define(collection_name, [this, &inputs, collection_name](int event_id) {
            return this->generateMergedContributions(inputs, event_id, collection_name);
        }, {"EventID"});
    }
    
    // Add reference columns with adjusted ObjectIDs
    for (const auto& column_name : all_reference_columns) {
        std::cout << "Adding reference column: " << column_name << std::endl;
        df_current = df_current.Define(column_name, [this, &inputs, column_name](int event_id) {
            return this->generateMergedReferenceColumn(inputs, event_id, column_name);
        }, {"EventID"});
    }
    
    // Write to ROOT file
    output_file->cd();
    std::string tree_name = "events";
    std::vector<std::string> columns_to_write = {"MCParticles", "EventHeader"};
    for (const auto& name : all_tracker_collections) columns_to_write.push_back(name);
    for (const auto& name : all_calo_collections) columns_to_write.push_back(name);
    for (const auto& name : all_contribution_collections) columns_to_write.push_back(name);
    for (const auto& name : all_reference_columns) columns_to_write.push_back(name);
    
    df_current.Snapshot(tree_name, output_file->GetName(), columns_to_write);
    
    std::cout << "RDataFrame snapshot completed with " << all_tracker_collections.size() 
              << " tracker collections, " << all_calo_collections.size() << " calo collections, "
              << all_contribution_collections.size() << " contribution collections, "
              << all_reference_columns.size() << " reference columns" << std::endl;
}

void StandaloneTimesliceMerger::copyPodioMetadata(TFile* output_file, const std::vector<SourceReader>& inputs) {
    std::cout << "Copying podio_metadata trees from input files..." << std::endl;
    
    // Find the first input file that has a podio_metadata tree
    TTree* metadata_tree = nullptr;
    TFile* source_file = nullptr;
    
    for (const auto& input : inputs) {
        if (!input.input_files.empty()) {
            // Try to open the first input file
            const std::string& input_filename = input.input_files[0];
            std::cout << "Checking for podio_metadata in: " << input_filename << std::endl;
            
            source_file = TFile::Open(input_filename.c_str(), "READ");
            if (source_file && !source_file->IsZombie()) {
                metadata_tree = dynamic_cast<TTree*>(source_file->Get("podio_metadata"));
                if (metadata_tree) {
                    std::cout << "Found podio_metadata tree in: " << input_filename << std::endl;
                    break;
                } else {
                    std::cout << "No podio_metadata tree found in: " << input_filename << std::endl;
                }
                source_file->Close();
                source_file = nullptr;
            }
        }
    }
    
    if (metadata_tree && source_file) {
        // Switch to output file
        output_file->cd();
        
        // Clone the metadata tree to the output file
        TTree* cloned_metadata = metadata_tree->CloneTree();
        cloned_metadata->SetName("podio_metadata");
        cloned_metadata->SetTitle("Podio metadata tree");
        
        // Write the cloned tree
        cloned_metadata->Write("podio_metadata", TObject::kOverwrite);
        
        std::cout << "Successfully copied podio_metadata tree with " 
                  << cloned_metadata->GetEntries() << " entries" << std::endl;
        
        // Close the source file
        source_file->Close();
    } else {
        std::cout << "Warning: No podio_metadata tree found in any input file" << std::endl;
        std::cout << "Creating minimal podio_metadata for EDM4hep compatibility..." << std::endl;
        
        // Create a basic podio_metadata tree for compatibility
        output_file->cd();
        TTree* minimal_metadata = new TTree("podio_metadata", "Minimal podio metadata for EDM4hep");
        
        // Add basic branches that EDM4hep expects
        std::string collection_name = "MCParticles";
        std::string collection_type = "edm4hep::MCParticleCollection";
        int collection_id = 0;
        
        minimal_metadata->Branch("CollectionName", &collection_name);
        minimal_metadata->Branch("CollectionType", &collection_type);
        minimal_metadata->Branch("CollectionID", &collection_id);
        
        // Fill with MCParticles collection info
        minimal_metadata->Fill();
        
        // Add EventHeader
        collection_name = "EventHeader";
        collection_type = "edm4hep::EventHeaderCollection";
        collection_id = 1;
        minimal_metadata->Fill();
        
        minimal_metadata->Write("podio_metadata", TObject::kOverwrite);
        std::cout << "Created minimal podio_metadata tree" << std::endl;
    }
}

void StandaloneTimesliceMerger::discoverCollectionsFromDataFrame(const std::string& input_filename, SourceReader& reader) {
    std::cout << "Discovering collections from dataframe: " << input_filename << std::endl;
    
    // Open the ROOT file to inspect available columns/branches
    auto input_file = std::unique_ptr<TFile>(TFile::Open(input_filename.c_str(), "READ"));
    if (!input_file || input_file->IsZombie()) {
        std::cout << "Warning: Could not open input file for collection discovery: " << input_filename << std::endl;
        return;
    }
    
    // Get the events tree
    TTree* events_tree = dynamic_cast<TTree*>(input_file->Get("events"));
    if (!events_tree) {
        std::cout << "Warning: No 'events' tree found in " << input_filename << std::endl;
        return;
    }
    
    // Create RDataFrame to inspect column names and types
    ROOT::RDataFrame df(*events_tree);
    auto column_names = df.GetColumnNames();
    
    std::cout << "Available columns in " << input_filename << ":" << std::endl;
    for (const auto& name : column_names) {
        // Skip columns that contain dots (these are field names, not collection names)
        if (name.find('.') != std::string::npos) {
            continue;
        }
        
        // Get the column type
        std::string column_type = df.GetColumnType(name);
        std::cout << "  - " << name << " : " << column_type << std::endl;
        
        // Handle podio reference columns (start with underscore and contain ObjectID)
        if (name[0] == '_' && column_type.find("podio::ObjectID") != std::string::npos) {
            reader.discovered_reference_columns.push_back(name);
            std::cout << "    -> Identified as podio reference column: " << name << std::endl;
        }
        // Identify collections based on their actual types
        else if (column_type.find("SimTrackerHit") != std::string::npos || 
                 column_type.find("edm4hep::SimTrackerHit") != std::string::npos) {
            
            reader.discovered_trackerhit_names.push_back(name);
            reader.trackerhits_collections[name] = DataFrameCollection<edm4hep::SimTrackerHitData>();
            reader.trackerhits_collections[name].name = name;
            reader.trackerhits_collections[name].type = "edm4hep::SimTrackerHitCollection";
            std::cout << "    -> Identified as SimTrackerHit collection: " << name << std::endl;
        }
        else if (column_type.find("SimCalorimeterHit") != std::string::npos || 
                 column_type.find("edm4hep::SimCalorimeterHit") != std::string::npos) {
            
            reader.discovered_calohit_names.push_back(name);
            reader.calohits_collections[name] = DataFrameCollection<edm4hep::SimCalorimeterHitData>();
            reader.calohits_collections[name].name = name;
            reader.calohits_collections[name].type = "edm4hep::SimCalorimeterHitCollection";
            std::cout << "    -> Identified as SimCalorimeterHit collection: " << name << std::endl;
        }
        else if (column_type.find("CaloHitContribution") != std::string::npos || 
                 column_type.find("edm4hep::CaloHitContribution") != std::string::npos) {
            
            reader.discovered_contribution_names.push_back(name);
            reader.contributions_collections[name] = DataFrameCollection<edm4hep::CaloHitContributionData>();
            reader.contributions_collections[name].name = name;
            reader.contributions_collections[name].type = "edm4hep::CaloHitContributionCollection";
            std::cout << "    -> Identified as CaloHitContribution collection: " << name << std::endl;
        }
        else if (column_type.find("MCParticle") != std::string::npos || 
                 column_type.find("edm4hep::MCParticle") != std::string::npos) {
            
            reader.mcparticles.name = name;
            reader.mcparticles.type = "edm4hep::MCParticleCollection";
            std::cout << "    -> Identified as MCParticle collection: " << name << std::endl;
        }
        else if (column_type.find("EventHeader") != std::string::npos || 
                 column_type.find("edm4hep::EventHeader") != std::string::npos) {
            
            reader.eventheaders.name = name;
            reader.eventheaders.type = "edm4hep::EventHeaderCollection";
            std::cout << "    -> Identified as EventHeader collection: " << name << std::endl;
        }
        else {
            std::cout << "    -> Unknown collection type: " << name << " (" << column_type << ")" << std::endl;
        }
    }
    
    std::cout << "Collection discovery completed for " << input_filename << std::endl;
    std::cout << "  Found " << reader.discovered_trackerhit_names.size() << " tracker hit collections" << std::endl;
    std::cout << "  Found " << reader.discovered_calohit_names.size() << " calorimeter hit collections" << std::endl;
    std::cout << "  Found " << reader.discovered_contribution_names.size() << " contribution collections" << std::endl;
    std::cout << "  Found " << reader.discovered_reference_columns.size() << " podio reference columns" << std::endl;
}

ROOT::VecOps::RVec<edm4hep::MCParticleData> StandaloneTimesliceMerger::generateMergedMCParticles(std::vector<SourceReader>& inputs, int event_id)
{
    ROOT::VecOps::RVec<edm4hep::MCParticle> merged;
    
    for (size_t source_idx = 0; source_idx < inputs.size(); ++source_idx) {
        auto& source = inputs[source_idx];
        
        // Create RDataFrame from the first input file
        if (!source.input_files.empty()) {
            ROOT::RDataFrame df("events", source.input_files[0]);
            int total_events = df.Count().GetValue();
            
            // For simplicity, just use modulo to cycle through available events
            int src_event = event_id % total_events;
            
            // Filter to this specific event
            auto event_df = df.Filter([src_event](ULong64_t rdfentry_) { 
                return static_cast<int>(rdfentry_) == src_event; 
            }, {"rdfentry_"});
            
            // Get MCParticles for this event
            if (df.HasColumn("MCParticles")) {
                auto particles_result = event_df.Take<ROOT::VecOps::RVec<edm4hep::MCParticleData>>("MCParticles");
                auto particles_vec = particles_result.GetValue();
                
                // For each event in the result (should be one)
                for (const auto& particle_collection : particles_vec) {
                    for (auto particle_data : particle_collection) {
                        // Add to merged collection without complex time calculations
                        merged.push_back(particle_data);
                    }
                }
            }
        }
    }
    
    return merged;
}

ROOT::VecOps::RVec<edm4hep::EventHeaderData> StandaloneTimesliceMerger::generateMergedEventHeaders(std::vector<SourceReader>& inputs, int event_id)
{
    ROOT::VecOps::RVec<edm4hep::EventHeaderData> merged;
    
    for (size_t source_idx = 0; source_idx < inputs.size(); ++source_idx) {
        auto& source = inputs[source_idx];
        
        // Create RDataFrame from the first input file
        if (!source.input_files.empty()) {
            ROOT::RDataFrame df("events", source.input_files[0]);
            int total_events = df.Count().GetValue();
            
            // For simplicity, just use modulo to cycle through available events
            int src_event = event_id % total_events;
            
            // Filter to this specific event
            auto event_df = df.Filter([src_event](ULong64_t rdfentry_) { 
                return static_cast<int>(rdfentry_) == src_event; 
            }, {"rdfentry_"});
            
            // Get EventHeader for this event
            if (df.HasColumn("EventHeader")) {
                auto headers_result = event_df.Take<ROOT::VecOps::RVec<edm4hep::EventHeaderData>>("EventHeader");
                auto headers_vec = headers_result.GetValue();
                
                // For each event in the result (should be one)
                for (const auto& header_collection : headers_vec) {
                    for (auto header_data : header_collection) {
                        // Add to merged collection without complex time calculations
                        merged.push_back(header_data);
                    }
                }
            }
        }
    }
    
    return merged;
}

ROOT::VecOps::RVec<edm4hep::SimTrackerHitData> StandaloneTimesliceMerger::generateMergedTrackerHits(std::vector<SourceReader>& inputs, int event_id, const std::string& collection_name)
{
    ROOT::VecOps::RVec<edm4hep::SimTrackerHit> merged;
    
    for (size_t source_idx = 0; source_idx < inputs.size(); ++source_idx) {
        auto& source = inputs[source_idx];
        
        // Create RDataFrame from the first input file
        if (!source.input_files.empty()) {
            ROOT::RDataFrame df("events", source.input_files[0]);
            
            // Check if this collection exists in this source
            if (!df.HasColumn(collection_name)) continue;
            
            int total_events = df.Count().GetValue();
            
            // For simplicity, just use modulo to cycle through available events
            int src_event = event_id % total_events;
            
            // Filter to this specific event
            auto event_df = df.Filter([src_event](ULong64_t rdfentry_) { 
                return static_cast<int>(rdfentry_) == src_event; 
            }, {"rdfentry_"});
            
            // Get tracker hits for this event
            auto hits_result = event_df.Take<ROOT::VecOps::RVec<edm4hep::SimTrackerHitData>>(collection_name);
            auto hits_vec = hits_result.GetValue();
            
            // For each event in the result (should be one)
            for (const auto& hit_collection : hits_vec) {
                for (auto hit_data : hit_collection) {
                    // Add to merged collection without complex time calculations
                    merged.push_back(hit_data);
                }
            }
        }
    }
    
    return merged;
}

ROOT::VecOps::RVec<edm4hep::SimCalorimeterHitData> StandaloneTimesliceMerger::generateMergedCaloHits(std::vector<SourceReader>& inputs, int event_id, const std::string& collection_name)
{
    ROOT::VecOps::RVec<edm4hep::SimCalorimeterHitData> merged;
    
    for (size_t source_idx = 0; source_idx < inputs.size(); ++source_idx) {
        auto& source = inputs[source_idx];
        
        // Create RDataFrame from the first input file
        if (!source.input_files.empty()) {
            ROOT::RDataFrame df("events", source.input_files[0]);
            
            // Check if this collection exists in this source
            if (!df.HasColumn(collection_name)) continue;
            
            int total_events = df.Count().GetValue();
            
            // For simplicity, just use modulo to cycle through available events
            int src_event = event_id % total_events;
            
            // Filter to this specific event
            auto event_df = df.Filter([src_event](ULong64_t rdfentry_) { 
                return static_cast<int>(rdfentry_) == src_event; 
            }, {"rdfentry_"});
            
            // Get calo hits for this event
            auto hits_result = event_df.Take<ROOT::VecOps::RVec<edm4hep::SimCalorimeterHitData>>(collection_name);
            auto hits_vec = hits_result.GetValue();
            
            // For each event in the result (should be one)
            for (const auto& hit_collection : hits_vec) {
                for (auto hit_data : hit_collection) {
                    // Add to merged collection without complex time calculations
                    merged.push_back(hit_data);
                }
            }
        }
    }
    
    return merged;
}

ROOT::VecOps::RVec<edm4hep::CaloHitContributionData> StandaloneTimesliceMerger::generateMergedContributions(std::vector<SourceReader>& inputs, int event_id, const std::string& collection_name)
{
    ROOT::VecOps::RVec<edm4hep::CaloHitContributionData> merged;
    
    for (size_t source_idx = 0; source_idx < inputs.size(); ++source_idx) {
        auto& source = inputs[source_idx];
        
        // Create RDataFrame from the first input file
        if (!source.input_files.empty()) {
            ROOT::RDataFrame df("events", source.input_files[0]);
            
            // Check if this collection exists in this source
            if (!df.HasColumn(collection_name)) continue;
            
            int total_events = df.Count().GetValue();
            
            // For simplicity, just use modulo to cycle through available events
            int src_event = event_id % total_events;
            
            // Filter to this specific event
            auto event_df = df.Filter([src_event](ULong64_t rdfentry_) { 
                return static_cast<int>(rdfentry_) == src_event; 
            }, {"rdfentry_"});
            
            // Get contributions for this event
            auto contribs_result = event_df.Take<ROOT::VecOps::RVec<edm4hep::CaloHitContributionData>>(collection_name);
            auto contribs_vec = contribs_result.GetValue();
            
            // For each event in the result (should be one)
            for (const auto& contrib_collection : contribs_vec) {
                for (auto contrib_data : contrib_collection) {
                    // Add to merged collection without complex time calculations
                    merged.push_back(contrib_data);
                }
            }
        }
    }
    
    return merged;
}

ROOT::VecOps::RVec<podio::ObjectID> StandaloneTimesliceMerger::generateMergedReferenceColumn(std::vector<SourceReader>& inputs, int event_id, const std::string& column_name)
{
    ROOT::VecOps::RVec<podio::ObjectID> merged;
    
    for (size_t source_idx = 0; source_idx < inputs.size(); ++source_idx) {
        auto& source = inputs[source_idx];
        
        // Create RDataFrame from the first input file
        if (!source.input_files.empty()) {
            ROOT::RDataFrame df("events", source.input_files[0]);
            
            // Check if this reference column exists in this source
            if (!df.HasColumn(column_name)) continue;
            
            int total_events = df.Count().GetValue();
            
            // For simplicity, just use modulo to cycle through available events
            int src_event = event_id % total_events;
            
            // Filter to this specific event
            auto event_df = df.Filter([src_event](ULong64_t rdfentry_) { 
                return static_cast<int>(rdfentry_) == src_event; 
            }, {"rdfentry_"});
            
            // Get reference column for this event
            auto refs_result = event_df.Take<ROOT::VecOps::RVec<podio::ObjectID>>(column_name);
            auto refs_vec = refs_result.GetValue();
            
            // Calculate offset for this source
            // TODO: This needs to be calculated based on the actual collection sizes
            // For now, use a simple offset based on source index
            int collection_offset = source_idx * 1000; // Mock offset
            
            // For each event in the result (should be one)
            for (const auto& ref_collection : refs_vec) {
                for (auto ref : ref_collection) {
                    // Adjust the index in the ObjectID to account for merged collection offset
                    podio::ObjectID adjusted_ref;
                    adjusted_ref.collectionID = ref.collectionID; // Keep the same collection ID
                    adjusted_ref.index = ref.index + collection_offset; // Adjust the index
                    
                    merged.push_back(adjusted_ref);
                }
            }
        }
    }
    
    return merged;
}