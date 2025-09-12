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
        
        std::cout << "Opened ROOT file: " << file_path << std::endl;
        root_files_.push_back(std::move(root_file));
    }
    
    if (root_files_.empty()) {
        throw std::runtime_error("No ROOT files provided");
    }
}

MutableRootReader::~MutableRootReader() {
    // ROOT files will be automatically closed when unique_ptrs are destroyed
}

size_t MutableRootReader::getEntries(const std::string& tree_name) const {
    // Open the tree from the first file to check entries
    if (root_files_.empty()) {
        throw std::runtime_error("No ROOT files available");
    }
    
    TTree* tree = root_files_[0]->Get<TTree>(tree_name.c_str());
    if (!tree) {
        throw std::runtime_error("Tree '" + tree_name + "' not found");
    }
    
    return static_cast<size_t>(tree->GetEntries());
}

std::vector<std::string> MutableRootReader::getAvailableCategories() const {
    std::vector<std::string> categories;
    
    if (root_files_.empty()) {
        return categories;
    }
    
    // Get list of trees from first file
    TList* keys = root_files_[0]->GetListOfKeys();
    for (int i = 0; i < keys->GetEntries(); ++i) {
        TKey* key = static_cast<TKey*>(keys->At(i));
        if (std::string(key->GetClassName()) == "TTree") {
            categories.push_back(key->GetName());
        }
    }
    return categories;
}

std::unique_ptr<MutableRootReader::MutableFrame> 
MutableRootReader::readMutableEntry(const std::string& tree_name, size_t entry) {
    if (root_files_.empty()) {
        throw std::runtime_error("No ROOT files available");
    }
    
    // Get the tree from the first file (following podio's approach of focusing on specific trees)
    TTree* tree = root_files_[0]->Get<TTree>(tree_name.c_str());
    if (!tree) {
        throw std::runtime_error("Tree '" + tree_name + "' not found");
    }
    
    if (entry >= static_cast<size_t>(tree->GetEntries())) {
        throw std::runtime_error("Entry " + std::to_string(entry) + " out of range for tree '" + tree_name + "'");
    }
    
    auto frame = std::make_unique<MutableFrame>();
    
    std::cout << "Reading entry " << entry << " from tree '" << tree_name << "'" << std::endl;
    
    // Get entry from tree
    tree->GetEntry(entry);
    
    // Follow podio's approach: only read collection branches, not association/metadata branches
    TObjArray* branches = tree->GetListOfBranches();
    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = static_cast<TBranch*>(branches->At(i));
        std::string branch_name = branch->GetName();
        
        // Skip branches that are not actual data collections following podio patterns
        if (shouldSkipBranch(branch_name)) {
            std::cout << "Skipping non-collection branch: " << branch_name << std::endl;
            continue;
        }
        
        std::cout << "Processing collection branch: " << branch_name << std::endl;
        
        // Create mutable collection based on branch type
        auto collection_variant = createCollectionFromBranch(branch_name, branch, entry);
        frame->putMutableVariant(std::move(collection_variant), branch_name);
    }
    
    return frame;
}

bool MutableRootReader::shouldSkipBranch(const std::string& branch_name) const {
    // Skip association branches following podio patterns
    if (branch_name.length() > 0 && branch_name[0] == '_' && branch_name.find('_', 1) != std::string::npos) {
        // Association branches like _CollectionName_relationName
        return true;
    }
    
    // Skip generic parameter branches 
    if (branch_name == "GPIntKeys" || branch_name == "GPIntValues" ||
        branch_name == "GPFloatKeys" || branch_name == "GPFloatValues" ||
        branch_name == "GPDoubleKeys" || branch_name == "GPDoubleValues" ||
        branch_name == "GPStringKeys" || branch_name == "GPStringValues") {
        return true;
    }
    
    // Skip podio internal branches
    if (branch_name == "PARAMETERS") {
        return true;
    }
    
    // These are collection main data branches - we should read them
    return false;
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
            if (type_name.find("MCParticleData") != std::string::npos) {
                return "MCParticleCollection";
            }
            else if (type_name.find("EventHeaderData") != std::string::npos) {
                return "EventHeaderCollection";
            }
            else if (type_name.find("SimTrackerHitData") != std::string::npos) {
                return "SimTrackerHitCollection";
            }
            else if (type_name.find("SimCalorimeterHitData") != std::string::npos) {
                return "SimCalorimeterHitCollection";
            }
            else if (type_name.find("CaloHitContributionData") != std::string::npos) {
                return "CaloHitContributionCollection";
            }
            
            // Also check for vector types (common in ROOT storage)
            if (type_name.find("vector<") != std::string::npos) {
                if (type_name.find("MCParticleData") != std::string::npos) {
                    return "MCParticleCollection";
                }
                else if (type_name.find("EventHeaderData") != std::string::npos) {
                    return "EventHeaderCollection";
                }
                else if (type_name.find("SimTrackerHitData") != std::string::npos) {
                    return "SimTrackerHitCollection";
                }
                else if (type_name.find("SimCalorimeterHitData") != std::string::npos) {
                    return "SimCalorimeterHitCollection";
                }
                else if (type_name.find("CaloHitContributionData") != std::string::npos) {
                    return "CaloHitContributionCollection";
                }
            }
        }
    }
    
    // If we reach here, we couldn't determine the type from ROOT type system
    // This should be rare now that we're only processing actual collection branches
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

