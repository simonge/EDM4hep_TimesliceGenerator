#pragma once

#include "DataHandler.h"
#include "DataSource.h"
#include <podio/ROOTReader.h>
#include <podio/ROOTFrameWriter.h>
#include <podio/Frame.h>
#include <podio/CollectionBase.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <edm4hep/EventHeaderCollection.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>

// Forward declarations
class PodioFrameZipperDataSource;

/**
 * @struct MergedFrameData
 * @brief Holds merged collections for efficient frame assembly
 * 
 * This structure accumulates collections from multiple source frames
 * by directly appending collection data and offsetting ObjectID references.
 */
struct MergedFrameData {
    // Merged EDM4hep collections - use actual collection types
    std::unique_ptr<edm4hep::MCParticleCollection> mcparticles;
    std::unique_ptr<edm4hep::EventHeaderCollection> event_headers;
    std::unique_ptr<edm4hep::EventHeaderCollection> sub_event_headers;
    
    // Dynamic hit collections - keyed by collection name
    std::unordered_map<std::string, std::unique_ptr<edm4hep::SimTrackerHitCollection>> tracker_hits;
    std::unordered_map<std::string, std::unique_ptr<edm4hep::SimCalorimeterHitCollection>> calo_hits;
    std::unordered_map<std::string, std::unique_ptr<edm4hep::CaloHitContributionCollection>> calo_contributions;
    
    // Track collection sizes for offsetting ObjectIDs
    std::unordered_map<std::string, size_t> collection_sizes;
    
    // Generic parameters
    std::unordered_map<std::string, std::vector<int>> gp_int_params;
    std::unordered_map<std::string, std::vector<float>> gp_float_params;
    std::unordered_map<std::string, std::vector<double>> gp_double_params;
    std::unordered_map<std::string, std::vector<std::string>> gp_string_params;
    
    void clear();
    void initialize();
};

/**
 * @class PodioFrameZipperDataHandler
 * @brief Efficient frame merging using podio's Frame API
 * 
 * This handler uses podio's Frame API to merge events by directly appending
 * collection data and offsetting ObjectID indices, avoiding expensive
 * setReferences() calls.
 * 
 * Key advantages:
 * - Performance: Works with collections directly, minimal overhead
 * - Generic: Works with any EDM4hep datamodel
 * - Memory Efficient: Streams data without unnecessary copies
 * - Maintains Associations: Properly offsets ObjectIDs
 */
class PodioFrameZipperDataHandler : public DataHandler {
public:
    PodioFrameZipperDataHandler() = default;
    ~PodioFrameZipperDataHandler() override = default;

    std::vector<std::unique_ptr<DataSource>> initializeDataSources(
        const std::string& filename,
        const std::vector<SourceConfig>& source_configs) override;
    
    void prepareTimeslice() override;
    
    void writeTimeslice() override;
    
    void finalize() override;
    
    std::string getFormatName() const override { return "PodioFrameZipper"; }

protected:
    void processEvent(DataSource& source) override;

private:
    // Podio readers for each source file
    std::vector<std::unique_ptr<podio::ROOTReader>> readers_;
    
    // Podio writer for output
    std::unique_ptr<podio::ROOTFrameWriter> writer_;
    
    // Output filename
    std::string output_filename_;
    
    // Merged frame data
    MergedFrameData merged_data_;
    
    // Source configuration storage
    std::vector<SourceConfig> source_configs_;
    
    // Source index to reader index mapping
    std::unordered_map<size_t, size_t> source_to_reader_map_;
    
    // Current event indices for each reader
    std::vector<size_t> current_event_indices_;
    
    // Collection names discovered from first source
    std::vector<std::string> tracker_collection_names_;
    std::vector<std::string> calo_collection_names_;
    
    // Helper methods
    void openReaders(const std::vector<SourceConfig>& source_configs);
    void discoverCollections();
    void mergeCollectionFromFrame(const podio::Frame& frame, 
                                   const std::string& collection_name,
                                   size_t mcparticle_offset);
    void applyTimeOffset(edm4hep::MCParticle& particle, float time_offset);
    void applyTimeOffset(edm4hep::SimTrackerHit& hit, float time_offset);
    void applyTimeOffset(edm4hep::SimCalorimeterHit& hit, float time_offset);
    void offsetObjectID(podio::ObjectID& obj_id, size_t offset);
    
    // Generic parameter merging
    void mergeGenericParameters(const podio::Frame& frame);
};
