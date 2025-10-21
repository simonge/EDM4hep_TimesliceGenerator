#include "BranchRelationshipMapper.h"
#include <iostream>
#include <algorithm>
#include <TBranch.h>
#include <TObjArray.h>
#include <TClass.h>

void BranchRelationshipMapper::discoverRelationships(TChain* chain) {
    if (!chain) {
        std::cerr << "Error: null TChain provided to discoverRelationships" << std::endl;
        return;
    }
    
    // Clear any existing mappings
    clear();
    
    // Get list of branches
    TObjArray* branches = chain->GetListOfBranches();
    if (!branches) {
        std::cerr << "Warning: No branches found in chain" << std::endl;
        return;
    }
    
    std::cout << "=== Discovering Branch Relationships ===" << std::endl;
    std::cout << "Total branches: " << branches->GetEntries() << std::endl;
    
    // First pass: identify all object data branches
    std::vector<std::string> object_branches;
    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = dynamic_cast<TBranch*>(branches->At(i));
        if (!branch) continue;
        
        std::string branch_name = branch->GetName();
        
        // Skip relationship branches (those starting with "_")
        if (isRelationshipBranch(branch_name)) {
            continue;
        }
        
        // Get the data type
        std::string data_type = getBranchDataType(branch);
        
        // Check if this is an EDM4hep data collection
        if (data_type.find("edm4hep::") != std::string::npos && 
            data_type.find("Data>") != std::string::npos) {
            // This is an object data branch
            object_branches.push_back(branch_name);
            collection_map_[branch_name] = CollectionRelationships(branch_name, data_type);
            
            std::cout << "  Found collection: " << branch_name << " (type: " << data_type << ")" << std::endl;
        }
    }
    
    // Second pass: identify and map relationship branches
    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = dynamic_cast<TBranch*>(branches->At(i));
        if (!branch) continue;
        
        std::string branch_name = branch->GetName();
        
        // Only process relationship branches
        if (!isRelationshipBranch(branch_name)) {
            continue;
        }
        
        // Get the data type to verify it's a relationship branch
        std::string data_type = getBranchDataType(branch);
        
        // Relationship branches should contain ObjectID or similar reference types
        if (data_type.find("ObjectID") == std::string::npos) {
            continue;
        }
        
        // Parse the branch name to extract collection and relation info
        std::string collection_name, relation_name;
        if (parseRelationshipBranch(branch_name, collection_name, relation_name)) {
            // Check if this collection exists in our map
            if (collection_map_.find(collection_name) != collection_map_.end()) {
                RelationshipInfo rel_info;
                rel_info.relation_branch_name = branch_name;
                rel_info.relation_name = relation_name;
                rel_info.target_collection = collection_name;  // Most relations point back to same or known collections
                
                // Determine if it's one-to-many based on data type
                rel_info.is_one_to_many = (data_type.find("vector<") != std::string::npos);
                
                // Add to collection's relationships
                collection_map_[collection_name].relationships.push_back(rel_info);
                
                std::cout << "  Found relationship: " << branch_name 
                         << " -> " << collection_name << "." << relation_name
                         << " (" << (rel_info.is_one_to_many ? "one-to-many" : "one-to-one") << ")" 
                         << std::endl;
            } else {
                std::cout << "  Warning: Relationship branch " << branch_name 
                         << " references unknown collection: " << collection_name << std::endl;
            }
        }
    }
    
    std::cout << "=== Discovery Complete ===" << std::endl;
    std::cout << "Discovered " << collection_map_.size() << " collections with relationships" << std::endl;
}

CollectionRelationships BranchRelationshipMapper::getCollectionRelationships(const std::string& collection_name) const {
    auto it = collection_map_.find(collection_name);
    if (it != collection_map_.end()) {
        return it->second;
    }
    return CollectionRelationships();  // Return empty if not found
}

std::vector<std::string> BranchRelationshipMapper::getAllCollectionNames() const {
    std::vector<std::string> names;
    names.reserve(collection_map_.size());
    for (const auto& [name, _] : collection_map_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> BranchRelationshipMapper::getCollectionsByType(const std::string& type_pattern) const {
    std::vector<std::string> names;
    for (const auto& [name, coll] : collection_map_) {
        if (coll.data_type.find(type_pattern) != std::string::npos) {
            names.push_back(name);
        }
    }
    return names;
}

bool BranchRelationshipMapper::hasRelationships(const std::string& collection_name) const {
    auto it = collection_map_.find(collection_name);
    return (it != collection_map_.end() && !it->second.relationships.empty());
}

std::vector<std::string> BranchRelationshipMapper::getRelationshipBranches(const std::string& collection_name) const {
    std::vector<std::string> branch_names;
    auto it = collection_map_.find(collection_name);
    if (it != collection_map_.end()) {
        for (const auto& rel : it->second.relationships) {
            branch_names.push_back(rel.relation_branch_name);
        }
    }
    return branch_names;
}

void BranchRelationshipMapper::printDiscoveredRelationships() const {
    std::cout << "\n=== Branch Relationship Map ===" << std::endl;
    for (const auto& [coll_name, coll_info] : collection_map_) {
        std::cout << "\nCollection: " << coll_name << std::endl;
        std::cout << "  Type: " << coll_info.data_type << std::endl;
        std::cout << "  Relationships (" << coll_info.relationships.size() << "):" << std::endl;
        for (const auto& rel : coll_info.relationships) {
            std::cout << "    - " << rel.relation_name 
                     << " (branch: " << rel.relation_branch_name 
                     << ", " << (rel.is_one_to_many ? "one-to-many" : "one-to-one") << ")"
                     << std::endl;
        }
    }
    std::cout << "================================\n" << std::endl;
}

void BranchRelationshipMapper::clear() {
    collection_map_.clear();
}

bool BranchRelationshipMapper::parseRelationshipBranch(const std::string& branch_name,
                                                        std::string& collection_name,
                                                        std::string& relation_name) const {
    // Relationship branches follow the pattern: _<CollectionName>_<relationName>
    // Examples: _MCParticles_parents, _VertexBarrelHits_particle
    
    if (branch_name.empty() || branch_name[0] != '_') {
        return false;
    }
    
    // Remove leading underscore
    std::string remaining = branch_name.substr(1);
    
    // Find the last underscore to separate collection from relation
    size_t last_underscore = remaining.rfind('_');
    if (last_underscore == std::string::npos) {
        return false;
    }
    
    collection_name = remaining.substr(0, last_underscore);
    relation_name = remaining.substr(last_underscore + 1);
    
    return !collection_name.empty() && !relation_name.empty();
}

bool BranchRelationshipMapper::isRelationshipBranch(const std::string& branch_name) const {
    // Relationship branches start with "_" and are not weight branches
    if (branch_name.empty() || branch_name[0] != '_') {
        return false;
    }
    
    // Exclude weight branches like "_EventHeader_weights"
    if (branch_name.find("_weights") != std::string::npos) {
        return false;
    }
    
    return true;
}

std::string BranchRelationshipMapper::getBranchDataType(TBranch* branch) const {
    if (!branch) {
        return "";
    }
    
    // Try to get the expected type
    TClass* expected_class = nullptr;
    EDataType expected_type;
    
    if (branch->GetExpectedType(expected_class, expected_type) == 0 && 
        expected_class && expected_class->GetName()) {
        return expected_class->GetName();
    }
    
    return "";
}
