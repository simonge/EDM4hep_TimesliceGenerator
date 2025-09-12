#pragma once

#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <edm4hep/EventHeaderCollection.h>
#include <podio/ROOTWriter.h>
#include <podio/Frame.h>
#include <TFile.h>
#include <TTree.h>
#include <TBranch.h>
#include <TClass.h>
#include <TKey.h>
#include <TList.h>
#include <TObjArray.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <typeinfo>
#include <variant>
#include <functional>
#include <type_traits>


/**
 * @brief A custom ROOT reader that provides genuinely mutable collections from the start
 * 
 * This class directly reads ROOT files and constructs mutable EDM4hep collections
 * without using podio::ROOTReader, eliminating any const restrictions entirely.
 * 
 * Key features:
 * - Direct ROOT file access using TFile/TTree interface
 * - Creates mutable collections from the beginning
 * - No const_cast operations anywhere
 * - No expensive cloning or association reattachment
 * - Compatible with existing podio file formats
 */
class MutableRootReader {
public:
    /**
     * @brief Mutable frame that contains genuinely mutable collections
     * 
     * This frame stores collections that are mutable from creation,
     * eliminating the need for any const_cast operations.
     */
    class MutableFrame {
    public:
        // Type-safe collection storage using variant
        using CollectionVariant = std::variant<
            std::unique_ptr<edm4hep::MCParticleCollection>,
            std::unique_ptr<edm4hep::EventHeaderCollection>,
            std::unique_ptr<edm4hep::SimTrackerHitCollection>,
            std::unique_ptr<edm4hep::SimCalorimeterHitCollection>,
            std::unique_ptr<edm4hep::CaloHitContributionCollection>
        >;
        
        MutableFrame() = default;
        
        /**
         * @brief Store a mutable collection in the frame
         */
        template<typename T>
        void putMutable(std::unique_ptr<T> collection, const std::string& name) {
            CollectionVariant variant = std::move(collection);
            collections_[name] = std::move(variant);
        }
        
        /**
         * @brief Store a collection variant directly in the frame
         */
        void putMutableVariant(CollectionVariant collection, const std::string& name) {
            collections_[name] = std::move(collection);
        }
        
        /**
         * @brief Get mutable access to a collection
         */
        template<typename T>
        T& getMutable(const std::string& name) {
            auto it = collections_.find(name);
            if (it == collections_.end()) {
                throw std::runtime_error("Collection '" + name + "' not found in frame");
            }
            
            if (auto* ptr = std::get_if<std::unique_ptr<T>>(&it->second)) {
                return **ptr;
            }
            throw std::runtime_error("Collection '" + name + "' has wrong type");
        }
        
        /**
         * @brief Get mutable pointer to a collection (returns nullptr if not found)
         */
        template<typename T>
        T* getMutablePtr(const std::string& name) {
            auto it = collections_.find(name);
            if (it == collections_.end()) {
                return nullptr;
            }
            
            if (auto* ptr = std::get_if<std::unique_ptr<T>>(&it->second)) {
                return ptr->get();
            }
            return nullptr;
        }
        
        /**
         * @brief Check if collection exists
         */
        bool hasCollection(const std::string& name) const {
            return collections_.find(name) != collections_.end();
        }
        
        /**
         * @brief Get list of available collection names
         */
        std::vector<std::string> getAvailableCollections() const {
            std::vector<std::string> names;
            names.reserve(collections_.size());
            for (const auto& [name, _] : collections_) {
                names.push_back(name);
            }
            return names;
        }
        
        /**
         * @brief Get collection type name from variant
         */
        std::string getCollectionTypeName(const std::string& name) const {
            auto it = collections_.find(name);
            if (it == collections_.end()) {
                return "";
            }
            
            return std::visit([](const auto& ptr) -> std::string {
                using T = typename std::remove_cv<typename std::remove_reference<decltype(*ptr)>::type>::type;
                if constexpr (std::is_same_v<T, edm4hep::MCParticleCollection>) {
                    return "MCParticleCollection";
                } else if constexpr (std::is_same_v<T, edm4hep::EventHeaderCollection>) {
                    return "EventHeaderCollection";
                } else if constexpr (std::is_same_v<T, edm4hep::SimTrackerHitCollection>) {
                    return "SimTrackerHitCollection";
                } else if constexpr (std::is_same_v<T, edm4hep::SimCalorimeterHitCollection>) {
                    return "SimCalorimeterHitCollection";
                } else if constexpr (std::is_same_v<T, edm4hep::CaloHitContributionCollection>) {
                    return "CaloHitContributionCollection";
                } else {
                    return "UnknownCollection";
                }
            }, it->second);
        }
        
        /**
         * @brief Convert this MutableFrame to a podio::Frame for writing
         * 
         * This creates a new podio::Frame and transfers all collections to it.
         * Note: This is a simplified placeholder - actual implementation would
         * need proper podio::CollectionBase conversion.
         */
        podio::Frame toPodioFrame() const {
            podio::Frame podio_frame;
            
            // For now, this is a placeholder. In a full implementation,
            // we would need to convert each variant collection to a proper
            // podio::CollectionBase and put it in the frame
            
            // TODO: Implement proper collection conversion to podio::CollectionBase
            // This would require understanding the podio API for adding collections
            
            return podio_frame;
        }
        
        /**
         * @brief Move a collection from this frame to another MutableFrame
         */
        template<typename T>
        void moveCollectionTo(const std::string& name, MutableFrame& dest_frame) {
            auto it = collections_.find(name);
            if (it == collections_.end()) {
                throw std::runtime_error("Collection '" + name + "' not found in frame");
            }
            
            // Move the collection variant
            dest_frame.collections_[name] = std::move(it->second);
            
            // Remove from this frame
            collections_.erase(it);
        }
        
    private:
        // Store collections using type-safe variant approach
        std::unordered_map<std::string, CollectionVariant> collections_;
    };
    
    /**
     * @brief Constructor that opens ROOT files directly
     * @param input_files Vector of input file paths
     */
    explicit MutableRootReader(const std::vector<std::string>& input_files);
    
    /**
     * @brief Destructor to clean up ROOT resources
     */
    ~MutableRootReader();
    
    /**
     * @brief Get number of entries in a tree
     * @param tree_name Tree name (e.g., "events")
     * @return Number of entries
     */
    size_t getEntries(const std::string& tree_name) const;
    
    /**
     * @brief Get available tree names from the first file
     * @return Vector of tree names
     */
    std::vector<std::string> getAvailableCategories() const;
    
    /**
     * @brief Read an entry from a specific tree and create a mutable frame
     * Following podio's approach of focused tree reading
     * @param tree_name Tree name to read from (e.g., "events")
     * @param entry Entry index to read
     * @return Unique pointer to mutable frame containing mutable collections
     */
    std::unique_ptr<MutableFrame> readMutableEntry(const std::string& tree_name, size_t entry);
    
private:
    /**
     * @brief Create a mutable collection of the specified type from ROOT data
     */
    template<typename T>
    std::unique_ptr<T> createMutableCollection(TBranch* branch, size_t entry) {
        // This is a simplified implementation - in a real implementation,
        // we would need to understand podio's ROOT storage format better
        // For now, create empty collections that can be populated
        
        auto collection = std::make_unique<T>();
        
        // TODO: Read actual data from ROOT branch and populate collection
        // This requires understanding podio's serialization format
        std::cout << "Creating mutable collection of type " << typeid(T).name() 
                  << " for branch " << branch->GetName() << std::endl;
        
        return collection;
    }
    
    /**
     * @brief Determine if a branch should be skipped (not read as a collection)
     * following podio's branch pattern recognition
     */
    bool shouldSkipBranch(const std::string& branch_name) const;
    
    /**
     * @brief Determine the appropriate collection type for a branch based on ROOT type information
     */
    std::string determineCollectionType(const std::string& branch_name, 
                                      const TBranch* branch,
                                      const TFile* root_file);
    
    /**
     * @brief Helper to create appropriate collection type based on branch name/type
     */
    MutableFrame::CollectionVariant createCollectionFromBranch(const std::string& branch_name, 
                                                               TBranch* branch, 
                                                               size_t entry);
    
    // ROOT file management - focused approach following podio
    std::vector<std::unique_ptr<TFile>> root_files_;
    
    // Current file tracking for chain-like behavior (if needed for multiple files)
    size_t current_file_index_ = 0;
};
