#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <TChain.h>
#include <TTree.h>

/**
 * @brief Stores information about a relationship branch
 * 
 * This represents a branch that contains ObjectID references (e.g., _MCParticles_parents)
 * that link objects in one collection to objects in another collection.
 */
struct RelationshipInfo {
    std::string relation_branch_name;  // e.g., "_MCParticles_parents"
    std::string relation_name;         // e.g., "parents" (extracted from branch name)
    std::string target_collection;     // The collection being referenced (e.g., "MCParticles")
    bool is_one_to_many;              // true for vector relations, false for single references
    
    RelationshipInfo() : is_one_to_many(false) {}
};

/**
 * @brief Stores all relationship information for a single collection
 */
struct CollectionRelationships {
    std::string collection_name;       // e.g., "MCParticles", "VertexBarrelHits"
    std::string data_type;            // e.g., "vector<edm4hep::MCParticleData>"
    std::vector<RelationshipInfo> relationships;  // All relationships for this collection
    bool has_time_field;              // true if this collection type has a time field to update
    bool has_index_ranges;            // true if objects have begin/end index ranges (e.g., parents_begin/end)
    
    CollectionRelationships() : has_time_field(false), has_index_ranges(false) {}
    CollectionRelationships(const std::string& name, const std::string& type) 
        : collection_name(name), data_type(type), has_time_field(false), has_index_ranges(false) {}
    
    // Helper method to check if this is a specific type
    bool isType(const std::string& type_name) const {
        return data_type.find(type_name) != std::string::npos;
    }
};

/**
 * @brief Automatically discovers and manages branch relationships from ROOT trees
 * 
 * This class analyzes ROOT TTree branch structure to discover EDM4hep collection
 * relationships automatically, removing the need for hardcoded patterns. It identifies:
 * - Object data branches (e.g., "MCParticles", "VertexBarrelHits")
 * - Relationship branches (e.g., "_MCParticles_parents", "_VertexBarrelHits_particle")
 * - The connections between them based on podio naming conventions
 * 
 * Usage:
 *   BranchRelationshipMapper mapper;
 *   mapper.discoverRelationships(chain);
 *   auto relationships = mapper.getCollectionRelationships("MCParticles");
 */
class BranchRelationshipMapper {
public:
    BranchRelationshipMapper() = default;
    
    /**
     * @brief Discover all collection relationships from a ROOT TChain/TTree
     * 
     * Analyzes the branch structure to identify:
     * 1. Object data branches (branches containing EDM4hep data types)
     * 2. Relationship branches (branches starting with "_" containing ObjectID vectors)
     * 3. The mapping between them based on naming conventions
     * 
     * @param chain The ROOT TChain or TTree to analyze
     */
    void discoverRelationships(TChain* chain);
    
    /**
     * @brief Get relationship information for a specific collection
     * 
     * @param collection_name Name of the collection (e.g., "MCParticles")
     * @return CollectionRelationships structure with all relationships, or empty if not found
     */
    CollectionRelationships getCollectionRelationships(const std::string& collection_name) const;
    
    /**
     * @brief Get all discovered collection names
     * 
     * @return Vector of all collection names that were discovered
     */
    std::vector<std::string> getAllCollectionNames() const;
    
    /**
     * @brief Get collection names by data type pattern
     * 
     * @param type_pattern Pattern to match in the data type (e.g., "SimTrackerHit")
     * @return Vector of collection names matching the pattern
     */
    std::vector<std::string> getCollectionsByType(const std::string& type_pattern) const;
    
    /**
     * @brief Get related contribution collection name for a calo collection
     * 
     * For a collection like "EcalBarrelHits", returns "EcalBarrelHitsContributions" if it exists
     * 
     * @param calo_collection_name Name of the calorimeter hit collection
     * @return Name of the contribution collection, or empty string if not found
     */
    std::string getContributionCollection(const std::string& calo_collection_name) const;
    
    /**
     * @brief Get all discovered Global Parameter (GP) branch names
     * 
     * Returns names of GP key branches like "GPIntKeys", "GPFloatKeys", etc.
     * 
     * @return Vector of GP branch names
     */
    std::vector<std::string> getGPBranches() const;
    
    /**
     * @brief Check if a collection has relationships
     * 
     * @param collection_name Name of the collection
     * @return true if the collection has at least one relationship
     */
    bool hasRelationships(const std::string& collection_name) const;
    
    /**
     * @brief Get all relationship branch names for a collection
     * 
     * @param collection_name Name of the collection
     * @return Vector of relationship branch names (e.g., ["_MCParticles_parents", "_MCParticles_daughters"])
     */
    std::vector<std::string> getRelationshipBranches(const std::string& collection_name) const;
    
    /**
     * @brief Print discovered relationships for debugging
     */
    void printDiscoveredRelationships() const;
    
    /**
     * @brief Clear all discovered relationships
     */
    void clear();

private:
    // Map from collection name to its relationships
    std::unordered_map<std::string, CollectionRelationships> collection_map_;
    
    // GP (Global Parameter) branch names discovered during analysis
    std::vector<std::string> gp_branches_;
    
    /**
     * @brief Parse a relationship branch name to extract information
     * 
     * Extracts the collection name and relation name from a branch like "_MCParticles_parents"
     * 
     * @param branch_name The full branch name
     * @param collection_name Output: extracted collection name
     * @param relation_name Output: extracted relation name
     * @return true if successfully parsed
     */
    bool parseRelationshipBranch(const std::string& branch_name, 
                                  std::string& collection_name,
                                  std::string& relation_name) const;
    
    /**
     * @brief Check if a branch name represents a relationship branch
     * 
     * Relationship branches typically start with "_" and contain ObjectID types
     * 
     * @param branch_name Branch name to check
     * @return true if this appears to be a relationship branch
     */
    bool isRelationshipBranch(const std::string& branch_name) const;
    
    /**
     * @brief Get the data type of a branch
     * 
     * @param branch ROOT TBranch pointer
     * @return String representation of the data type
     */
    std::string getBranchDataType(TBranch* branch) const;
};
