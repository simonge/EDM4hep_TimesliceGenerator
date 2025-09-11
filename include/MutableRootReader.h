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
        MutableFrame() = default;
        
        /**
         * @brief Store a mutable collection in the frame
         */
        template<typename T>
        void putMutable(std::unique_ptr<T> collection, const std::string& name) {
            auto deleter = [](void* ptr) { delete static_cast<T*>(ptr); };
            mutable_collections_[name] = std::unique_ptr<void, void(*)(void*)>(
                collection.release(), deleter
            );
            collection_types_[name] = typeid(T).name();
        }
        
        /**
         * @brief Store a type-erased mutable collection in the frame
         */
        void putMutable(std::unique_ptr<void, void(*)(void*)> collection, 
                       const std::string& name, const std::string& type_name) {
            mutable_collections_[name] = std::move(collection);
            collection_types_[name] = type_name;
        }
        
        /**
         * @brief Get mutable access to a collection
         */
        template<typename T>
        T& getMutable(const std::string& name) {
            auto it = mutable_collections_.find(name);
            if (it == mutable_collections_.end()) {
                throw std::runtime_error("Collection '" + name + "' not found in frame");
            }
            return *static_cast<T*>(it->second.get());
        }
        
        /**
         * @brief Get mutable pointer to a collection (returns nullptr if not found)
         */
        template<typename T>
        T* getMutablePtr(const std::string& name) {
            auto it = mutable_collections_.find(name);
            if (it == mutable_collections_.end()) {
                return nullptr;
            }
            return static_cast<T*>(it->second.get());
        }
        
        /**
         * @brief Check if collection exists
         */
        bool hasCollection(const std::string& name) const {
            return mutable_collections_.find(name) != mutable_collections_.end();
        }
        
        /**
         * @brief Get list of available collection names
         */
        std::vector<std::string> getAvailableCollections() const {
            std::vector<std::string> names;
            for (const auto& [name, _] : mutable_collections_) {
                names.push_back(name);
            }
            return names;
        }
        
        /**
         * @brief Get collection type name
         */
        std::string getCollectionTypeName(const std::string& name) const {
            auto it = collection_types_.find(name);
            return it != collection_types_.end() ? it->second : "";
        }
        
        /**
         * @brief Convert this MutableFrame to a podio::Frame for writing
         * 
         * This creates a new podio::Frame and transfers all collections to it.
         * Collections are moved, so this MutableFrame becomes empty after conversion.
         */
        std::unique_ptr<podio::Frame> toPodioFrame() {
            auto podio_frame = std::make_unique<podio::Frame>();
            
            // Move all collections to the podio frame
            for (auto& [name, collection] : mutable_collections_) {
                auto type_it = collection_types_.find(name);
                if (type_it == collection_types_.end()) continue;
                
                const std::string& type_name = type_it->second;
                
                // Transfer collection based on its type
                transferCollectionToPodioFrame(*podio_frame, name, collection.get(), type_name);
            }
            
            // Clear our collections since they've been moved
            mutable_collections_.clear();
            collection_types_.clear();
            
            return podio_frame;
        }
        
        /**
         * @brief Move a collection from this frame to another MutableFrame
         */
        template<typename T>
        void moveCollectionTo(const std::string& name, MutableFrame& dest_frame) {
            auto it = mutable_collections_.find(name);
            if (it == mutable_collections_.end()) {
                throw std::runtime_error("Collection '" + name + "' not found in frame");
            }
            
            // Move the collection
            dest_frame.mutable_collections_[name] = std::move(it->second);
            dest_frame.collection_types_[name] = collection_types_[name];
            
            // Remove from this frame
            mutable_collections_.erase(it);
            collection_types_.erase(name);
        }
        
    private:
        // Store collections as void pointers with type information
        std::unordered_map<std::string, std::unique_ptr<void, void(*)(void*)>> mutable_collections_;
        std::unordered_map<std::string, std::string> collection_types_;
        
        /**
         * @brief Helper to transfer a collection to a podio::Frame
         */
        void transferCollectionToPodioFrame(podio::Frame& frame, const std::string& name, 
                                          void* collection_ptr, const std::string& type_name) const;
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
     * @brief Read an entry and create a mutable frame with mutable collections
     * @param category Tree name to read from
     * @param entry Entry index to read
     * @return Unique pointer to mutable frame containing mutable collections
     */
    std::unique_ptr<MutableFrame> readMutableEntry(const std::string& category, size_t entry);
    
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
     * @brief Helper to create appropriate collection type based on branch name/type
     */
    std::unique_ptr<void, void(*)(void*)> createCollectionFromBranch(const std::string& branch_name, 
                                                                      TBranch* branch, 
                                                                      size_t entry);
    
    /**
     * @brief Get the type name for a collection based on branch name
     */
    std::string getCollectionTypeName(const std::string& branch_name);
    
    // ROOT file management
    std::vector<std::unique_ptr<TFile>> root_files_;
    std::unordered_map<std::string, TTree*> trees_;
    std::unordered_map<std::string, size_t> total_entries_;
    
    // Current file tracking for chain-like behavior
    size_t current_file_index_ = 0;
    std::unordered_map<std::string, size_t> entries_per_file_;
};
