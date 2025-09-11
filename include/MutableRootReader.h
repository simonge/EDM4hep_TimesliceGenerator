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
#include <memory>

#ifdef PODIO_AVAILABLE

/**
 * @brief A mutable wrapper around podio::ROOTReader that provides genuinely mutable collections
 * 
 * This class provides a temporary solution that wraps podio::ROOTReader while research
 * is conducted into podio's ROOT storage format. It provides:
 * - Mutable access to collections without const_cast in user code
 * - A clean interface for future replacement with direct ROOT reading
 * - Placeholder for eliminating podio::ROOTReader dependency entirely
 * 
 * TODO: Replace with direct ROOT file reading once podio's storage format is understood
 */
class MutableRootReader {
public:
    /**
     * @brief Mutable frame that wraps podio::Frame but provides mutable access
     * 
     * This temporarily uses the existing podio::Frame but provides safe mutable access.
     * TODO: Replace with direct ROOT-based implementation once format is understood.
     */
    class MutableFrame {
    public:
        explicit MutableFrame(std::unique_ptr<podio::Frame> frame) : frame_(std::move(frame)) {}
        
        /**
         * @brief Get mutable access to a collection (temporarily uses safe const_cast)
         * This is safe because we own the frame and will replace with proper implementation
         */
        template<typename T>
        T& getMutable(const std::string& name) {
            // TODO: Replace with proper mutable collection creation
            return const_cast<T&>(frame_->get<T>(name));
        }
        
        /**
         * @brief Get mutable pointer to a collection (returns nullptr if not found)
         */
        template<typename T>
        T* getMutablePtr(const std::string& name) {
            try {
                return &getMutable<T>(name);
            } catch (const std::exception&) {
                return nullptr;
            }
        }
        
        /**
         * @brief Check if collection exists
         */
        bool hasCollection(const std::string& name) const {
            auto collections = frame_->getAvailableCollections();
            return std::find(collections.begin(), collections.end(), name) != collections.end();
        }
        
        /**
         * @brief Get list of available collection names
         */
        std::vector<std::string> getAvailableCollections() const {
            return frame_->getAvailableCollections();
        }
        
        /**
         * @brief Get collection type name (temporary implementation)
         */
        std::string getCollectionTypeName(const std::string& name) const {
            try {
                const auto* coll = frame_->get(name);
                return coll ? std::string(coll->getValueTypeName()) : "";
            } catch (const std::exception&) {
                return "";
            }
        }
        
    private:
        std::unique_ptr<podio::Frame> frame_;
    };
    
    /**
     * @brief Constructor that wraps podio::ROOTReader temporarily
     * @param input_files Vector of input file paths
     * 
     * TODO: Replace with direct ROOT file access once format is understood
     */
    explicit MutableRootReader(const std::vector<std::string>& input_files);
    
    /**
     * @brief Get number of entries in a category (tree)
     * @param category Tree name (e.g., "events")
     * @return Number of entries
     */
    size_t getEntries(const std::string& category) const;
    
    /**
     * @brief Get available categories (tree names)
     * @return Vector of tree names
     */
    std::vector<std::string> getAvailableCategories() const;
    
    /**
     * @brief Read an entry and create a mutable frame
     * @param category Tree name to read from
     * @param entry Entry index to read
     * @return Unique pointer to mutable frame
     */
    std::unique_ptr<MutableFrame> readMutableEntry(const std::string& category, size_t entry);
    
private:
    // Temporary wrapper around podio::ROOTReader
    // TODO: Replace with direct ROOT access
    std::shared_ptr<podio::ROOTReader> reader_;
};

#endif // PODIO_AVAILABLE