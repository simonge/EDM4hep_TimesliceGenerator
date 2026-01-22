#include "PodioFrameZipperDataHandler.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>

// Simple DataSource that wraps a podio::ROOTReader for frame-based access
class PodioFrameZipperDataSource : public DataSource {
public:
    PodioFrameZipperDataSource(const SourceConfig& config, size_t source_index, 
                               podio::ROOTReader* reader, size_t reader_index)
        : reader_(reader), reader_index_(reader_index) {
        config_ = &config;
        source_index_ = source_index;
        
        // Get total number of entries
        total_entries_ = reader_->getEntries("events");
        current_entry_index_ = 0;
    }
    
    bool hasMoreEntries() const override {
        return current_entry_index_ < total_entries_;
    }
    
    bool loadNextEvent() override {
        if (!hasMoreEntries()) {
            return false;
        }
        current_entry_index_++;
        return true;
    }
    
    void loadEvent(size_t event_index) override {
        // Event loading is handled directly by the handler
        // This is just a placeholder to satisfy the interface
        current_entry_index_ = event_index;
    }
    
    void printStatus() const override {
        std::cout << "PodioFrameZipperDataSource: " << current_entry_index_ 
                  << "/" << total_entries_ << " entries" << std::endl;
    }
    
    bool isInitialized() const override {
        return reader_ != nullptr;
    }
    
    std::string getFormatName() const override {
        return "PodioFrameZipper";
    }
    
    size_t getReaderIndex() const {
        return reader_index_;
    }
    
protected:
    VertexPosition getBeamVertexPosition() const override {
        // For now, return zero vertex
        // In a full implementation, this would extract from the current frame
        return VertexPosition{0.0f, 0.0f, 0.0f};
    }

private:
    podio::ROOTReader* reader_;  // Non-owning pointer
    size_t reader_index_;
};

void MergedFrameData::clear() {
    if (mcparticles) mcparticles->clear();
    if (event_headers) event_headers->clear();
    if (sub_event_headers) sub_event_headers->clear();
    
    for (auto& [name, coll] : tracker_hits) {
        if (coll) coll->clear();
    }
    for (auto& [name, coll] : calo_hits) {
        if (coll) coll->clear();
    }
    for (auto& [name, coll] : calo_contributions) {
        if (coll) coll->clear();
    }
    
    collection_sizes.clear();
    
    gp_int_params.clear();
    gp_float_params.clear();
    gp_double_params.clear();
    gp_string_params.clear();
}

void MergedFrameData::initialize() {
    mcparticles = std::make_unique<edm4hep::MCParticleCollection>();
    event_headers = std::make_unique<edm4hep::EventHeaderCollection>();
    sub_event_headers = std::make_unique<edm4hep::EventHeaderCollection>();
}

std::vector<std::unique_ptr<DataSource>> PodioFrameZipperDataHandler::initializeDataSources(
    const std::string& filename,
    const std::vector<SourceConfig>& source_configs) {
    
    std::cout << "Initializing PodioFrameZipper data handler for: " << filename << std::endl;
    
    output_filename_ = filename;
    source_configs_ = source_configs;
    
    // Open all readers
    openReaders(source_configs);
    
    // Create data sources
    std::vector<std::unique_ptr<DataSource>> data_sources;
    data_sources.reserve(source_configs.size());
    
    for (size_t source_idx = 0; source_idx < source_configs.size(); ++source_idx) {
        const auto& config = source_configs[source_idx];
        
        // Map this source to its reader
        size_t reader_idx = source_idx;  // Simple 1:1 mapping for now
        source_to_reader_map_[source_idx] = reader_idx;
        
        auto data_source = std::make_unique<PodioFrameZipperDataSource>(
            config, source_idx, readers_[reader_idx].get(), reader_idx);
        
        std::cout << "Created PodioFrameZipperDataSource for: " << config.name << std::endl;
        data_sources.push_back(std::move(data_source));
    }
    
    // Discover collections from first reader
    discoverCollections();
    
    // Initialize merged data structures
    merged_data_.initialize();
    
    // Create output writer
    writer_ = std::make_unique<podio::ROOTFrameWriter>(output_filename_);
    
    std::cout << "PodioFrameZipper data handler initialized successfully" << std::endl;
    
    return data_sources;
}

void PodioFrameZipperDataHandler::openReaders(const std::vector<SourceConfig>& source_configs) {
    readers_.clear();
    current_event_indices_.clear();
    
    for (const auto& config : source_configs) {
        if (config.input_files.empty()) {
            throw std::runtime_error("Source " + config.name + " has no input files");
        }
        
        auto reader = std::make_unique<podio::ROOTReader>();
        
        // Open the first file (for simplicity, assuming single file per source)
        // In a full implementation, this would handle multiple files
        const std::string& first_file = config.input_files[0];
        reader->openFile(first_file);
        
        std::cout << "Opened reader for: " << first_file << std::endl;
        std::cout << "  Entries: " << reader->getEntries("events") << std::endl;
        
        readers_.push_back(std::move(reader));
        current_event_indices_.push_back(0);
    }
}

void PodioFrameZipperDataHandler::discoverCollections() {
    if (readers_.empty()) {
        std::cout << "Warning: No readers available for collection discovery" << std::endl;
        return;
    }
    
    // Read first frame to discover collections
    auto& first_reader = readers_[0];
    if (first_reader->getEntries("events") == 0) {
        std::cout << "Warning: First reader has no entries" << std::endl;
        return;
    }
    
    auto frame = first_reader->readEntry("events", 0);
    auto collections = frame.getAvailableCollections();
    
    std::cout << "Discovering collections from first frame:" << std::endl;
    
    for (const auto& collection_name : collections) {
        std::cout << "  - " << collection_name;
        
        // Categorize collections
        if (collection_name.find("SimTrackerHit") != std::string::npos &&
            collection_name.find("Contribution") == std::string::npos &&
            collection_name != "MCParticles" &&
            collection_name != "EventHeader" &&
            collection_name != "SubEventHeaders") {
            tracker_collection_names_.push_back(collection_name);
            merged_data_.tracker_hits[collection_name] = 
                std::make_unique<edm4hep::SimTrackerHitCollection>();
            std::cout << " [Tracker]";
        } else if (collection_name.find("SimCalorimeterHit") != std::string::npos &&
                   collection_name.find("Contribution") == std::string::npos) {
            calo_collection_names_.push_back(collection_name);
            merged_data_.calo_hits[collection_name] = 
                std::make_unique<edm4hep::SimCalorimeterHitCollection>();
            
            // Also prepare contribution collection
            std::string contrib_name = collection_name + "Contributions";
            merged_data_.calo_contributions[collection_name] = 
                std::make_unique<edm4hep::CaloHitContributionCollection>();
            std::cout << " [Calorimeter]";
        }
        
        std::cout << std::endl;
    }
    
    std::cout << "Collection discovery complete:" << std::endl;
    std::cout << "  Tracker collections: " << tracker_collection_names_.size() << std::endl;
    std::cout << "  Calorimeter collections: " << calo_collection_names_.size() << std::endl;
}

void PodioFrameZipperDataHandler::prepareTimeslice() {
    merged_data_.clear();
}

void PodioFrameZipperDataHandler::processEvent(DataSource& source) {
    auto* podio_source = dynamic_cast<PodioFrameZipperDataSource*>(&source);
    if (!podio_source) {
        throw std::runtime_error("PodioFrameZipperDataHandler: Expected PodioFrameZipperDataSource");
    }
    
    size_t reader_idx = podio_source->getReaderIndex();
    size_t event_idx = source.getCurrentEntryIndex();
    
    // Read frame from reader
    auto frame = readers_[reader_idx]->readEntry("events", event_idx);
    
    float time_offset = source.getCurrentTimeOffset();
    
    // Calculate MCParticle offset for this event
    size_t mcparticle_offset = merged_data_.mcparticles ? merged_data_.mcparticles->size() : 0;
    
    // Store collection size before merging for contribution offset calculation
    merged_data_.collection_sizes["MCParticles"] = mcparticle_offset;
    
    // Merge MCParticles (two-pass approach to handle forward references)
    if (frame.get("MCParticles") != nullptr) {
        const auto& source_particles = frame.get<edm4hep::MCParticleCollection>("MCParticles");
        
        // First pass: Copy all POD data
        std::vector<size_t> new_particle_indices;
        for (const auto& particle : source_particles) {
            size_t new_idx = merged_data_.mcparticles->size();
            new_particle_indices.push_back(new_idx);
            
            auto merged_particle = merged_data_.mcparticles->create();
            
            // Copy all POD data
            merged_particle.setPDG(particle.getPDG());
            merged_particle.setGeneratorStatus(particle.getGeneratorStatus());
            merged_particle.setSimulatorStatus(particle.getSimulatorStatus());
            merged_particle.setCharge(particle.getCharge());
            merged_particle.setTime(particle.getTime() + time_offset);  // Apply time offset
            merged_particle.setMass(particle.getMass());
            merged_particle.setVertex(particle.getVertex());
            merged_particle.setEndpoint(particle.getEndpoint());
            merged_particle.setMomentum(particle.getMomentum());
            merged_particle.setMomentumAtEndpoint(particle.getMomentumAtEndpoint());
            merged_particle.setSpin(particle.getSpin());
            merged_particle.setColorFlow(particle.getColorFlow());
        }
        
        // Second pass: Set references with proper offsetting
        for (size_t i = 0; i < source_particles.size(); ++i) {
            const auto& particle = source_particles[i];
            auto& merged_particle = (*merged_data_.mcparticles)[new_particle_indices[i]];
            
            // Copy and offset parent references
            for (const auto& parent : particle.getParents()) {
                if (parent.isAvailable()) {
                    // Find the index of the parent in the source collection
                    for (size_t j = 0; j < source_particles.size(); ++j) {
                        if (source_particles[j].id() == parent.id()) {
                            merged_particle.addToParents((*merged_data_.mcparticles)[new_particle_indices[j]]);
                            break;
                        }
                    }
                }
            }
            
            // Copy and offset daughter references
            for (const auto& daughter : particle.getDaughters()) {
                if (daughter.isAvailable()) {
                    for (size_t j = 0; j < source_particles.size(); ++j) {
                        if (source_particles[j].id() == daughter.id()) {
                            merged_particle.addToDaughters((*merged_data_.mcparticles)[new_particle_indices[j]]);
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Create a SubEventHeader to track this source/event
    const auto& config = source.getConfig();
    if (!config.already_merged) {
        auto sub_header = merged_data_.sub_event_headers->create();
        sub_header.setEventNumber(static_cast<int>(event_idx));
        sub_header.setRunNumber(static_cast<int>(source.getSourceIndex()));
        sub_header.setTimeStamp(static_cast<uint64_t>(mcparticle_offset));
        sub_header.setWeight(time_offset);
    } else {
        // For already merged sources, copy existing SubEventHeaders
        if (frame.get("SubEventHeaders") != nullptr) {
            const auto& source_sub_headers = frame.get<edm4hep::EventHeaderCollection>("SubEventHeaders");
            for (const auto& sub_header : source_sub_headers) {
                auto merged_sub_header = merged_data_.sub_event_headers->create();
                merged_sub_header.setEventNumber(sub_header.getEventNumber());
                merged_sub_header.setRunNumber(sub_header.getRunNumber());
                merged_sub_header.setTimeStamp(sub_header.getTimeStamp() + mcparticle_offset);
                merged_sub_header.setWeight(sub_header.getWeight() + time_offset);
            }
        }
    }
    
    // Merge tracker hits
    for (const auto& tracker_name : tracker_collection_names_) {
        if (frame.get(tracker_name) != nullptr) {
            const auto& source_hits = frame.get<edm4hep::SimTrackerHitCollection>(tracker_name);
            
            auto& merged_hits = merged_data_.tracker_hits[tracker_name];
            
            for (const auto& hit : source_hits) {
                auto merged_hit = merged_hits->create();
                
                // Copy POD data with time offset
                merged_hit.setCellID(hit.getCellID());
                merged_hit.setEDep(hit.getEDep());
                merged_hit.setTime(hit.getTime() + time_offset);  // Apply time offset
                merged_hit.setPathLength(hit.getPathLength());
                merged_hit.setQuality(hit.getQuality());
                merged_hit.setPosition(hit.getPosition());
                merged_hit.setMomentum(hit.getMomentum());
                
                // Copy and offset particle reference
                auto particle_ref = hit.getParticle();
                if (particle_ref.isAvailable()) {
                    // Find index in source collection
                    if (frame.get("MCParticles") != nullptr) {
                        const auto& source_particles = frame.get<edm4hep::MCParticleCollection>("MCParticles");
                        for (size_t i = 0; i < source_particles.size(); ++i) {
                            if (source_particles[i].id() == particle_ref.id()) {
                                size_t offset_idx = mcparticle_offset + i;
                                if (offset_idx < merged_data_.mcparticles->size()) {
                                    merged_hit.setParticle((*merged_data_.mcparticles)[offset_idx]);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Merge calorimeter hits and contributions
    for (const auto& calo_name : calo_collection_names_) {
        std::string contrib_name = calo_name + "Contributions";
        
        // Get current contribution collection size for offsetting
        size_t contrib_offset = merged_data_.calo_contributions[calo_name] ? 
                                merged_data_.calo_contributions[calo_name]->size() : 0;
        
        // First merge contributions
        if (frame.get(contrib_name) != nullptr) {
            const auto& source_contribs = frame.get<edm4hep::CaloHitContributionCollection>(contrib_name);
            
            auto& merged_contribs = merged_data_.calo_contributions[calo_name];
            
            for (const auto& contrib : source_contribs) {
                auto merged_contrib = merged_contribs->create();
                
                merged_contrib.setPDG(contrib.getPDG());
                merged_contrib.setEnergy(contrib.getEnergy());
                merged_contrib.setTime(contrib.getTime() + time_offset);  // Apply time offset
                merged_contrib.setStepPosition(contrib.getStepPosition());
                
                // Copy and offset particle reference
                auto particle_ref = contrib.getParticle();
                if (particle_ref.isAvailable()) {
                    if (frame.get("MCParticles") != nullptr) {
                        const auto& source_particles = frame.get<edm4hep::MCParticleCollection>("MCParticles");
                        for (size_t i = 0; i < source_particles.size(); ++i) {
                            if (source_particles[i].id() == particle_ref.id()) {
                                size_t offset_idx = mcparticle_offset + i;
                                if (offset_idx < merged_data_.mcparticles->size()) {
                                    merged_contrib.setParticle((*merged_data_.mcparticles)[offset_idx]);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        // Then merge calorimeter hits
        if (frame.get(calo_name) != nullptr) {
            const auto& source_hits = frame.get<edm4hep::SimCalorimeterHitCollection>(calo_name);
            
            auto& merged_hits = merged_data_.calo_hits[calo_name];
            
            for (const auto& hit : source_hits) {
                auto merged_hit = merged_hits->create();
                
                merged_hit.setCellID(hit.getCellID());
                merged_hit.setEnergy(hit.getEnergy());
                merged_hit.setPosition(hit.getPosition());
                
                // Copy and offset contribution references
                for (const auto& contrib_ref : hit.getContributions()) {
                    if (contrib_ref.isAvailable() && frame.get(contrib_name) != nullptr) {
                        const auto& source_contribs = frame.get<edm4hep::CaloHitContributionCollection>(contrib_name);
                        for (size_t i = 0; i < source_contribs.size(); ++i) {
                            if (source_contribs[i].id() == contrib_ref.id()) {
                                size_t offset_idx = contrib_offset + i;
                                auto& merged_contribs = merged_data_.calo_contributions[calo_name];
                                if (offset_idx < merged_contribs->size()) {
                                    merged_hit.addToContributions((*merged_contribs)[offset_idx]);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Merge generic parameters
    mergeGenericParameters(frame);
}

void PodioFrameZipperDataHandler::mergeGenericParameters(const podio::Frame& frame) {
    // Get all parameter keys from the frame
    auto int_keys = frame.getAvailableCollections();  // Simplified - in reality would need proper GP API
    
    // For now, skip GP merging as it requires deeper knowledge of podio's GP API
    // This would be implemented similar to EDM4hepDataHandler's GP handling
}

void PodioFrameZipperDataHandler::writeTimeslice() {
    if (!writer_) {
        throw std::runtime_error("Output writer not initialized");
    }
    
    // Create a new frame for this timeslice
    podio::Frame output_frame;
    
    // Create main EventHeader
    auto event_header = merged_data_.event_headers->create();
    event_header.setEventNumber(static_cast<int>(current_timeslice_number_));
    event_header.setRunNumber(0);
    event_header.setTimeStamp(current_timeslice_number_);
    
    // Put all collections into the frame
    output_frame.put(std::move(merged_data_.mcparticles), "MCParticles");
    output_frame.put(std::move(merged_data_.event_headers), "EventHeader");
    output_frame.put(std::move(merged_data_.sub_event_headers), "SubEventHeaders");
    
    // Put tracker collections
    for (auto& [name, coll] : merged_data_.tracker_hits) {
        output_frame.put(std::move(coll), name);
    }
    
    // Put calorimeter collections
    for (auto& [name, coll] : merged_data_.calo_hits) {
        output_frame.put(std::move(coll), name);
    }
    
    // Put contribution collections
    for (const auto& calo_name : calo_collection_names_) {
        std::string contrib_name = calo_name + "Contributions";
        auto& coll = merged_data_.calo_contributions[calo_name];
        output_frame.put(std::move(coll), contrib_name);
    }
    
    // Write the frame
    writer_->writeFrame(output_frame, "events");
    
    std::cout << "=== Timeslice " << current_timeslice_number_ << " written ===" << std::endl;
    
    // Re-initialize all collections for next timeslice
    merged_data_.initialize();
    
    // Re-create dynamic collections
    for (const auto& tracker_name : tracker_collection_names_) {
        merged_data_.tracker_hits[tracker_name] = 
            std::make_unique<edm4hep::SimTrackerHitCollection>();
    }
    
    for (const auto& calo_name : calo_collection_names_) {
        merged_data_.calo_hits[calo_name] = 
            std::make_unique<edm4hep::SimCalorimeterHitCollection>();
        merged_data_.calo_contributions[calo_name] = 
            std::make_unique<edm4hep::CaloHitContributionCollection>();
    }
}

void PodioFrameZipperDataHandler::finalize() {
    if (writer_) {
        writer_->finish();
    }
    std::cout << "PodioFrameZipper output finalized" << std::endl;
}
