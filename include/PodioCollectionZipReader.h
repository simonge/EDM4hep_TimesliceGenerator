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
 * @brief A podio-based reader that provides collection zipping and vectorized operations
 * 
 * This class wraps podio::ROOTReader functionality and provides convenient methods for:
 * - Zipping multiple collections together for coordinated iteration
 * - Vectorized time offset operations on collections
 * - Batch processing of related data across different collection types
 * 
 * Unlike the base podio::ROOTReader which provides const access to collections,
 * this reader provides mutable access to collections, allowing in-place modifications.
 * This is achieved through safe const_cast operations on frames that are owned by
 * the reader, eliminating the const restriction while maintaining data safety.
 */
class PodioCollectionZipReader {
public:
    /**
     * @brief Constructor that wraps an existing podio::ROOTReader
     * @param reader Shared pointer to the underlying podio reader
     */
    explicit PodioCollectionZipReader(std::shared_ptr<podio::ROOTReader> reader);
    
    /**
     * @brief Constructor that creates a new reader for given files
     * @param input_files Vector of input file paths
     */
    explicit PodioCollectionZipReader(const std::vector<std::string>& input_files);
    
    // Basic reader interface delegation
    size_t getEntries(const std::string& category) const;
    std::vector<std::string> getAvailableCategories() const;
    std::unique_ptr<podio::Frame> readEntry(const std::string& category, size_t entry);
    
    /**
     * @brief Zip multiple collections for coordinated iteration
     * @param frame The frame containing the collections
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
            
            // Get element from specific collection by name
            template<typename T>
            auto getElement(const std::string& collection_name) const -> decltype(std::declval<T>()[0]) {
                for (size_t i = 0; i < zipped_.names.size(); ++i) {
                    if (zipped_.names[i] == collection_name) {
                        const auto* coll = dynamic_cast<const T*>(zipped_.collections[i]);
                        if (coll && index_ < coll->size()) {
                            return (*coll)[index_];
                        }
                    }
                }
                throw std::runtime_error("Collection not found or invalid type: " + collection_name);
            }
            
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
     * @brief Apply vectorized time offset to MCParticle collection
     * @param particles The MCParticle collection to modify
     * @param time_offset The time offset to add to all particles
     */
    static void addTimeOffsetVectorized(edm4hep::MCParticleCollection& particles, float time_offset);
    
    /**
     * @brief Apply vectorized time offset to SimTrackerHit collection
     * @param hits The SimTrackerHit collection to modify
     * @param time_offset The time offset to add to all hits
     */
    static void addTimeOffsetVectorized(edm4hep::SimTrackerHitCollection& hits, float time_offset);
    
    /**
     * @brief Apply vectorized time offset to SimCalorimeterHit collection
     * @param hits The SimCalorimeterHit collection to modify
     * @param time_offset The time offset to add to all hits
     */
    static void addTimeOffsetVectorized(edm4hep::SimCalorimeterHitCollection& hits, float time_offset);
    
    /**
     * @brief Apply vectorized time offset to CaloHitContribution collection
     * @param contributions The CaloHitContribution collection to modify
     * @param time_offset The time offset to add to all contributions
     */
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
    
    /**
     * @brief Get mutable access to a collection in a frame
     * @param frame The frame containing the collection
     * @param name The name of the collection
     * @return Mutable reference to the collection
     * @note This is safe because the frame is owned by the reader
     */
    template<typename T>
    static T& getMutableCollection(podio::Frame& frame, const std::string& name) {
        // Safe const_cast since we own the frame and know it's mutable
        return const_cast<T&>(frame.get<T>(name));
    }
    
    /**
     * @brief Get mutable access to a collection in a frame (pointer version)
     * @param frame The frame containing the collection
     * @param name The name of the collection
     * @return Mutable pointer to the collection, or nullptr if not found
     * @note This is safe because the frame is owned by the reader
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
     * @brief Apply a function to corresponding elements across multiple collections
     * @param zipped The zipped collections
     * @param func Function to apply that takes the current iterator
     */
    template<typename Func>
    void forEachZipped(const ZippedCollections& zipped, Func&& func) {
        for (auto it = zipped.begin(); it != zipped.end(); ++it) {
            func(it);
        }
    }

private:
    std::shared_ptr<podio::ROOTReader> reader_;
    
    // Helper to get collection by name and cast to appropriate type
    template<typename T>
    const T* getCollection(const podio::Frame& frame, const std::string& name) {
        try {
            return &frame.get<T>(name);
        } catch (const std::exception&) {
            return nullptr;
        }
    }
    
    // Helper to get mutable collection by name and cast to appropriate type
    template<typename T>
    T* getMutableCollection(podio::Frame& frame, const std::string& name) {
        try {
            // Get the collection and const_cast to mutable - this is safe since we own the frame
            return const_cast<T*>(&frame.get<T>(name));
        } catch (const std::exception&) {
            return nullptr;
        }
    }
};

#endif // PODIO_AVAILABLE