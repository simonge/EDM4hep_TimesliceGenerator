#include "MutableRootReader.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <typeinfo>
#include <podio/Frame.h>

#include <TKey.h>
#include <TList.h>
#include <TObjArray.h>

MutableRootReader::MutableRootReader(const std::vector<std::string>& input_files) {
    std::cout << "MutableRootReader: Initializing direct ROOT file access" << std::endl;
    
    // Open ROOT files directly
    for (const auto& file_path : input_files) {
        std::unique_ptr<TFile> root_file;
        
        root_file.reset(TFile::Open(file_path.c_str(), "READ"));

        if (!root_file || root_file->IsZombie()) {
            throw std::runtime_error("Failed to open ROOT file: " + file_path);
        }
        
        // Get list of trees (categories) in this file
        TList* keys = root_file->GetListOfKeys();
        for (int i = 0; i < keys->GetEntries(); ++i) {
            TKey* key = static_cast<TKey*>(keys->At(i));
            if (std::string(key->GetClassName()) == "TTree") {
                std::string tree_name = key->GetName();
                TTree* tree = static_cast<TTree*>(root_file->Get(tree_name.c_str()));
                if (tree) {
                    // Store tree pointer (only from first file for now - TODO: handle chaining)
                    if (trees_.find(tree_name) == trees_.end()) {
                        trees_[tree_name] = tree;
                        total_entries_[tree_name] = tree->GetEntries();
                        std::cout << "Found tree '" << tree_name << "' with " 
                                  << total_entries_[tree_name] << " entries" << std::endl;
                    }
                }
            }
        }
        
        root_files_.push_back(std::move(root_file));
    }
    
    if (trees_.empty()) {
        throw std::runtime_error("No trees found in input files");
    }
}

MutableRootReader::~MutableRootReader() {
    // ROOT files will be automatically closed when unique_ptrs are destroyed
}

size_t MutableRootReader::getEntries(const std::string& category) const {
    auto it = total_entries_.find(category);
    if (it == total_entries_.end()) {
        throw std::runtime_error("Category '" + category + "' not found");
    }
    return it->second;
}

std::vector<std::string> MutableRootReader::getAvailableCategories() const {
    std::vector<std::string> categories;
    for (const auto& [tree_name, _] : trees_) {
        categories.push_back(tree_name);
    }
    return categories;
}

std::unique_ptr<MutableRootReader::MutableFrame> 
MutableRootReader::readMutableEntry(const std::string& category, size_t entry) {
    auto tree_it = trees_.find(category);
    if (tree_it == trees_.end()) {
        throw std::runtime_error("Category '" + category + "' not found");
    }
    
    TTree* tree = tree_it->second;
    if (entry >= static_cast<size_t>(tree->GetEntries())) {
        throw std::runtime_error("Entry " + std::to_string(entry) + " out of range for category '" + category + "'");
    }
    
    auto frame = std::make_unique<MutableFrame>();
    
    // Get entry from tree
    tree->GetEntry(entry);
    
    // Iterate through branches and create mutable collections
    TObjArray* branches = tree->GetListOfBranches();
    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = static_cast<TBranch*>(branches->At(i));
        std::string branch_name = branch->GetName();
        
        // Create mutable collection based on branch type/name
        auto collection_variant = createCollectionFromBranch(branch_name, branch, entry);
        frame->putMutableVariant(std::move(collection_variant), branch_name);
    }
    
    return frame;
}

std::string MutableRootReader::determineCollectionType(const std::string& branch_name, 
                                                      const TBranch* branch,
                                                      const TFile* root_file) {
    // First, try to get type information from the branch itself
    if (branch) {
        // Get the class name of the branch data
        const char* class_name = branch->GetClassName();
        if (class_name && strlen(class_name) > 0) {
            std::string type_name(class_name);
            
            std::cout << "Branch '" << branch_name << "' has ROOT type: " << type_name << std::endl;
            
            // Map ROOT type names to our collection types based on actual ROOT class names
            // These patterns match how podio typically stores collections in ROOT files
            if (type_name.find("MCParticleData") != std::string::npos ||
                type_name.find("MCParticle") != std::string::npos ||
                type_name.find("edm4hep::MCParticle") != std::string::npos) {
                return "MCParticleCollection";
            }
            else if (type_name.find("EventHeaderData") != std::string::npos ||
                     type_name.find("EventHeader") != std::string::npos ||
                     type_name.find("edm4hep::EventHeader") != std::string::npos) {
                return "EventHeaderCollection";
            }
            else if (type_name.find("SimTrackerHitData") != std::string::npos ||
                     type_name.find("SimTrackerHit") != std::string::npos ||
                     type_name.find("edm4hep::SimTrackerHit") != std::string::npos) {
                return "SimTrackerHitCollection";
            }
            else if (type_name.find("SimCalorimeterHitData") != std::string::npos ||
                     type_name.find("SimCalorimeterHit") != std::string::npos ||
                     type_name.find("edm4hep::SimCalorimeterHit") != std::string::npos) {
                return "SimCalorimeterHitCollection";
            }
            else if (type_name.find("CaloHitContributionData") != std::string::npos ||
                     type_name.find("CaloHitContribution") != std::string::npos ||
                     type_name.find("edm4hep::CaloHitContribution") != std::string::npos) {
                return "CaloHitContributionCollection";
            }
            
            // Also check for vector types (common in ROOT storage)
            if (type_name.find("vector") != std::string::npos) {
                if (type_name.find("MCParticle") != std::string::npos) {
                    return "MCParticleCollection";
                }
                else if (type_name.find("EventHeader") != std::string::npos) {
                    return "EventHeaderCollection";
                }
                else if (type_name.find("SimTrackerHit") != std::string::npos) {
                    return "SimTrackerHitCollection";
                }
                else if (type_name.find("SimCalorimeterHit") != std::string::npos) {
                    return "SimCalorimeterHitCollection";
                }
                else if (type_name.find("CaloHitContribution") != std::string::npos) {
                    return "CaloHitContributionCollection";
                }
            }
        }
        
        // If we have a branch but no recognizable class name, try to get more information
        TClass* branch_class = branch->GetCurrentClass();
        if (branch_class) {
            std::string class_name = branch_class->GetName();
            std::cout << "Branch '" << branch_name << "' has TClass: " << class_name << std::endl;
            
            // Similar mapping for TClass names
            if (class_name.find("MCParticle") != std::string::npos) {
                return "MCParticleCollection";
            }
            else if (class_name.find("EventHeader") != std::string::npos) {
                return "EventHeaderCollection";
            }
            else if (class_name.find("SimTrackerHit") != std::string::npos) {
                return "SimTrackerHitCollection";
            }
            else if (class_name.find("SimCalorimeterHit") != std::string::npos) {
                return "SimCalorimeterHitCollection";
            }
            else if (class_name.find("CaloHitContribution") != std::string::npos) {
                return "CaloHitContributionCollection";
            }
        }
    }
    
    // Try to read collection type from podio metadata 
    if (root_file) {
        // Look for podio metadata tree (commonly named "podio_metadata" or "metadata")
        auto* metadata_tree = root_file->Get<TTree>("podio_metadata");
        if (!metadata_tree) {
            metadata_tree = root_file->Get<TTree>("metadata");
        }
        
        if (metadata_tree) {
            // podio typically stores collection type information in metadata
            // This would involve reading the metadata entries to find collection type mappings
            std::cout << "Found metadata tree, but detailed parsing not implemented yet" << std::endl;
        }
    }
    
    // If we reach here, we couldn't determine the type from ROOT type system
    std::cout << "Warning: Could not determine collection type from ROOT type information for branch '" 
              << branch_name << "', using fallback" << std::endl;
    
    return "MCParticleCollection"; // Final fallback
}

MutableRootReader::MutableFrame::CollectionVariant 
MutableRootReader::createCollectionFromBranch(const std::string& branch_name, 
                                               TBranch* branch, 
                                               size_t entry) {
    // Determine collection type using improved detection based on ROOT type information
    std::string collection_type = determineCollectionType(branch_name, branch,
        root_files_.empty() ? nullptr : root_files_[0].get());
    
    std::cout << "Creating mutable collection of type " << collection_type 
              << " for branch " << branch_name << std::endl;
    
    if (collection_type == "MCParticleCollection") {
        auto collection = createMutableCollection<edm4hep::MCParticleCollection>(branch, entry);
        return std::unique_ptr<edm4hep::MCParticleCollection>(collection.release());
    }
    else if (collection_type == "EventHeaderCollection") {
        auto collection = createMutableCollection<edm4hep::EventHeaderCollection>(branch, entry);
        return std::unique_ptr<edm4hep::EventHeaderCollection>(collection.release());
    }
    else if (collection_type == "SimTrackerHitCollection") {
        auto collection = createMutableCollection<edm4hep::SimTrackerHitCollection>(branch, entry);
        return std::unique_ptr<edm4hep::SimTrackerHitCollection>(collection.release());
    }
    else if (collection_type == "SimCalorimeterHitCollection") {
        auto collection = createMutableCollection<edm4hep::SimCalorimeterHitCollection>(branch, entry);
        return std::unique_ptr<edm4hep::SimCalorimeterHitCollection>(collection.release());
    }
    else if (collection_type == "CaloHitContributionCollection") {
        auto collection = createMutableCollection<edm4hep::CaloHitContributionCollection>(branch, entry);
        return std::unique_ptr<edm4hep::CaloHitContributionCollection>(collection.release());
    }
    
    // Should not reach here due to fallback in determineCollectionType
    std::cout << "Error: Failed to create collection for type " << collection_type << std::endl;
    auto collection = createMutableCollection<edm4hep::MCParticleCollection>(branch, entry);
    return std::unique_ptr<edm4hep::MCParticleCollection>(collection.release());
}

