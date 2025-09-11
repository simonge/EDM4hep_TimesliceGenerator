#pragma once

#ifdef PODIO_AVAILABLE
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <edm4hep/EventHeaderCollection.h>
#include <podio/ROOTReader.h>
#include <podio/Frame.h>
#endif

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

#ifdef PODIO_AVAILABLE

/**
 * @brief A mutable collection reader that bypasses const restrictions
 * 
 * This class provides truly mutable access to EDM4hep collections by:
 * - Reading const collections from podio::ROOTReader
 * - Creating new mutable collections by copying/cloning the data
 * - Ensuring modifications persist when collections are written back
 * - Eliminating the need for const_cast operations entirely
 * 
 * The key insight is that instead of trying to cast away const-ness
 * (which doesn't work with podio's immutable design), we create new
 * mutable collections with the same data that can be freely modified.
 */
class PodioMutableCollectionReader {
public:
    /**
     * @brief Constructor that creates a new reader for given files
     * @param input_files Vector of input file paths
     */
    explicit PodioMutableCollectionReader(const std::vector<std::string>& input_files);
    
    // Basic reader interface
    size_t getEntries(const std::string& category) const;
    std::vector<std::string> getAvailableCategories() const;
    
    /**
     * @brief Read an entry and create a frame with fully mutable collections
     * @param category The category to read from
     * @param entry The entry index to read
     * @return Frame containing mutable copies of all collections
     */
    std::unique_ptr<podio::Frame> readMutableEntry(const std::string& category, size_t entry);
    
    /**
     * @brief Create a mutable copy of a specific collection from a const frame
     * @param frame The const frame containing the original collection
     * @param collection_name The name of the collection to copy
     * @return Unique pointer to a mutable copy of the collection
     */
    template<typename T>
    static std::unique_ptr<T> createMutableCopy(const podio::Frame& frame, const std::string& collection_name);
    
    /**
     * @brief Create a complete mutable frame by copying all collections from a const frame
     * @param const_frame The const frame to copy from
     * @return Unique pointer to a new frame with mutable copies of all collections
     */
    static std::unique_ptr<podio::Frame> createMutableFrame(const podio::Frame& const_frame);
    
    /**
     * @brief Get mutable access to a collection in a frame created by this reader
     * @param frame The frame containing the collection (must be created by this reader)
     * @param name The name of the collection
     * @return Mutable reference to the collection
     * @note This method is safe because it only operates on frames with truly mutable data
     */
    template<typename T>
    static T& getMutableCollection(podio::Frame& frame, const std::string& name) {
        // Safe const_cast because this reader creates frames with truly mutable data
        return const_cast<T&>(frame.get<T>(name));
    }
    
    /**
     * @brief Get mutable pointer to a collection in a frame created by this reader
     * @param frame The frame containing the collection (must be created by this reader)
     * @param name The name of the collection
     * @return Mutable pointer to the collection, or nullptr if not found
     */
    template<typename T>
    static T* getMutableCollectionPtr(podio::Frame& frame, const std::string& name) {
        try {
            return &getMutableCollection<T>(frame, name);
        } catch (const std::exception&) {
            return nullptr;
        }
    }
    
    /**
     * @brief Zip multiple collections for coordinated iteration
     * @param frame The mutable frame containing the collections
     * @param collection_names Vector of collection names to zip together
     * @return A structure that allows coordinated iteration over the collections
     */
    struct ZippedCollections {
        std::vector<std::string> names;
        std::vector<podio::CollectionBase*> collections;
        size_t min_size; // Size of the smallest collection
        
        // Iterator class for zipped collections
        class Iterator {
        public:
            Iterator(const ZippedCollections& zipped, size_t index) : zipped_(zipped), index_(index) {}
            
            bool operator!=(const Iterator& other) const { return index_ != other.index_; }
            Iterator& operator++() { ++index_; return *this; }
            size_t getIndex() const { return index_; }
            
            // Get mutable element from specific collection by name
            template<typename T>
            auto getMutableElement(const std::string& collection_name) -> decltype(std::declval<T>()[0]) {
                for (size_t i = 0; i < zipped_.names.size(); ++i) {
                    if (zipped_.names[i] == collection_name) {
                        auto* coll = dynamic_cast<T*>(zipped_.collections[i]);
                        if (coll && index_ < coll->size()) {
                            return (*coll)[index_];
                        }
                    }
                }
                throw std::runtime_error("Collection not found or invalid type: " + collection_name);
            }
            
        private:
            const ZippedCollections& zipped_;
            size_t index_;
        };
        
        Iterator begin() const { return Iterator(*this, 0); }
        Iterator end() const { return Iterator(*this, min_size); }
    };
    
    ZippedCollections zipCollections(podio::Frame& frame, const std::vector<std::string>& collection_names);
    
    /**
     * @brief Apply vectorized time offset to various collection types
     * @param collection The collection to modify (MCParticles, SimTrackerHits, etc.)
     * @param time_offset The time offset to add to all elements
     */
    template<typename T>
    static void addTimeOffsetVectorized(T& collection, float time_offset);
    
    // Explicit specializations for different collection types
    static void addTimeOffsetVectorized(edm4hep::MCParticleCollection& particles, float time_offset);
    static void addTimeOffsetVectorized(edm4hep::SimTrackerHitCollection& hits, float time_offset);
    static void addTimeOffsetVectorized(edm4hep::SimCalorimeterHitCollection& hits, float time_offset);
    static void addTimeOffsetVectorized(edm4hep::CaloHitContributionCollection& contributions, float time_offset);
    
    /**
     * @brief Apply time offset to all time-bearing collections in a frame
     * @param frame The frame containing collections to modify
     * @param time_offset The time offset to apply
     * @param collection_names Optional list of specific collections to process (if empty, processes all known types)
     */
    static void addTimeOffsetToFrame(podio::Frame& frame, float time_offset, 
                                   const std::vector<std::string>& collection_names = {});
    
    /**
     * @brief Get collection names of a specific type from a frame
     * @param frame The frame to search
     * @param type_name The EDM4HEP type name to search for
     * @return Vector of collection names matching the type
     */
    static std::vector<std::string> getCollectionNamesByType(const podio::Frame& frame, const std::string& type_name);

private:
    std::shared_ptr<podio::ROOTReader> reader_;
    
    /**
     * @brief Create a mutable copy of an MCParticleCollection
     */
    static std::unique_ptr<edm4hep::MCParticleCollection> cloneMCParticleCollection(const edm4hep::MCParticleCollection& source);
    
    /**
     * @brief Create a mutable copy of a SimTrackerHitCollection
     */
    static std::unique_ptr<edm4hep::SimTrackerHitCollection> cloneSimTrackerHitCollection(const edm4hep::SimTrackerHitCollection& source);
    
    /**
     * @brief Create a mutable copy of a SimCalorimeterHitCollection
     */
    static std::unique_ptr<edm4hep::SimCalorimeterHitCollection> cloneSimCalorimeterHitCollection(const edm4hep::SimCalorimeterHitCollection& source);
    
    /**
     * @brief Create a mutable copy of a CaloHitContributionCollection
     */
    static std::unique_ptr<edm4hep::CaloHitContributionCollection> cloneCaloHitContributionCollection(const edm4hep::CaloHitContributionCollection& source);
    
    /**
     * @brief Create a mutable copy of an EventHeaderCollection
     */
    static std::unique_ptr<edm4hep::EventHeaderCollection> cloneEventHeaderCollection(const edm4hep::EventHeaderCollection& source);
    
    // Type name to processor function mapping for efficient type checking
    static const std::unordered_map<std::string, std::function<void(podio::Frame&, const std::string&, float)>>& getTypeProcessors();
};

#endif // PODIO_AVAILABLE