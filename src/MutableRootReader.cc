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
        auto root_file = std::make_unique<TFile>(file_path.c_str(), "READ");
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

MutableRootReader::MutableFrame::CollectionVariant 
MutableRootReader::createCollectionFromBranch(const std::string& branch_name, 
                                               TBranch* branch, 
                                               size_t entry) {
    // Map branch names to collection types
    // This is a simplified mapping - real implementation would read podio metadata
    
    if (branch_name == "MCParticles" || branch_name.find("MCParticle") != std::string::npos) {
        auto collection = createMutableCollection<edm4hep::MCParticleCollection>(branch, entry);
        return std::unique_ptr<edm4hep::MCParticleCollection>(collection.release());
    }
    else if (branch_name == "EventHeader" || branch_name.find("EventHeader") != std::string::npos) {
        auto collection = createMutableCollection<edm4hep::EventHeaderCollection>(branch, entry);
        return std::unique_ptr<edm4hep::EventHeaderCollection>(collection.release());
    }
    else if (branch_name.find("TrackerHit") != std::string::npos) {
        auto collection = createMutableCollection<edm4hep::SimTrackerHitCollection>(branch, entry);
        return std::unique_ptr<edm4hep::SimTrackerHitCollection>(collection.release());
    }
    else if (branch_name.find("CalorimeterHit") != std::string::npos) {
        auto collection = createMutableCollection<edm4hep::SimCalorimeterHitCollection>(branch, entry);
        return std::unique_ptr<edm4hep::SimCalorimeterHitCollection>(collection.release());
    }
    else if (branch_name.find("CaloHitContribution") != std::string::npos) {
        auto collection = createMutableCollection<edm4hep::CaloHitContributionCollection>(branch, entry);
        return std::unique_ptr<edm4hep::CaloHitContributionCollection>(collection.release());
    }
    
    // Default to MCParticleCollection if unknown type
    std::cout << "Warning: Unknown collection type for branch '" << branch_name 
              << "', defaulting to MCParticleCollection" << std::endl;
    auto collection = createMutableCollection<edm4hep::MCParticleCollection>(branch, entry);
    return std::unique_ptr<edm4hep::MCParticleCollection>(collection.release());
}

