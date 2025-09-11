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
                                                      const TFile* root_file) {
    // Try to read collection type from podio metadata first
    if (root_file) {
        // Look for podio metadata tree (commonly named "podio_metadata" or "metadata")
        auto* metadata_tree = root_file->Get<TTree>("podio_metadata");
        if (!metadata_tree) {
            metadata_tree = root_file->Get<TTree>("metadata");
        }
        
        if (metadata_tree) {
            // Try to read collection type info from metadata
            // This would require understanding podio's metadata format
            // For now, we'll fall back to heuristics
        }
    }
    
    // Enhanced heuristic-based collection type detection
    // Based on common EIC detector naming conventions
    
    // MCParticle collections
    if (branch_name == "MCParticles" || 
        branch_name.find("MCParticle") != std::string::npos ||
        branch_name.find("_particle") != std::string::npos) {
        return "MCParticleCollection";
    }
    
    // EventHeader collections  
    if (branch_name == "EventHeader" || branch_name.find("EventHeader") != std::string::npos) {
        return "EventHeaderCollection";
    }
    
    // Tracker hit collections
    if (branch_name.find("TrackerHit") != std::string::npos ||
        branch_name.find("SiTrackerHits") != std::string::npos ||
        branch_name.find("TrackerBarrel") != std::string::npos ||
        branch_name.find("TrackerEndcap") != std::string::npos ||
        branch_name.find("VertexBarrel") != std::string::npos ||
        branch_name.find("VertexEndcap") != std::string::npos) {
        return "SimTrackerHitCollection";
    }
    
    // Calorimeter hit collections (including various EIC calorimeters)
    if (branch_name.find("CalorimeterHit") != std::string::npos ||
        branch_name.find("EcalHits") != std::string::npos ||
        branch_name.find("HcalHits") != std::string::npos ||
        branch_name.find("LFHCALHits") != std::string::npos ||
        branch_name.find("HCalHits") != std::string::npos ||
        branch_name.find("EMCalHits") != std::string::npos ||
        branch_name.find("LumiDirectPCALHits") != std::string::npos ||
        branch_name.find("LumiSpecCALHits") != std::string::npos ||
        branch_name.find("HcalFarForwardZDCHits") != std::string::npos ||
        branch_name.find("HcalEndcapPInsertHits") != std::string::npos ||
        branch_name.find("EcalFarForwardZDCHits") != std::string::npos ||
        branch_name.find("EcalEndcapPInsertHits") != std::string::npos ||
        branch_name.find("EcalBarrel") != std::string::npos ||
        branch_name.find("EcalEndcap") != std::string::npos ||
        (branch_name.find("Hits") != std::string::npos && 
         (branch_name.find("Cal") != std::string::npos || 
          branch_name.find("HCAL") != std::string::npos || 
          branch_name.find("ECAL") != std::string::npos ||
          branch_name.find("ZDC") != std::string::npos))) {
        return "SimCalorimeterHitCollection";
    }
    
    // Calorimeter hit contribution collections
    if (branch_name.find("CaloHitContribution") != std::string::npos ||
        branch_name.find("_contributions") != std::string::npos ||
        branch_name.find("Contributions") != std::string::npos) {
        return "CaloHitContributionCollection";
    }
    
    // Unknown type - log warning and default
    std::cout << "Warning: Unknown collection type for branch '" << branch_name 
              << "', using heuristic-based fallback" << std::endl;
    
    // Make an educated guess based on naming patterns
    if (branch_name.find("Hit") != std::string::npos) {
        return "SimCalorimeterHitCollection"; // Most hits are calorimeter hits in EIC
    }
    
    return "MCParticleCollection"; // Final fallback
}

MutableRootReader::MutableFrame::CollectionVariant 
MutableRootReader::createCollectionFromBranch(const std::string& branch_name, 
                                               TBranch* branch, 
                                               size_t entry) {
    // Determine collection type using improved detection
    std::string collection_type = determineCollectionType(branch_name, 
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

