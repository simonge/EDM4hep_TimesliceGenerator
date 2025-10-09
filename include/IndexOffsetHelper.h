#pragma once

#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>
#include <TFile.h>
#include <TTree.h>
#include <TObjArray.h>
#include <TBranch.h>

/**
 * Helper class to apply index offsets to EDM4hep data structures.
 * This eliminates the need to hardcode field names for each collection type.
 * 
 * The helper uses a generic approach with pointer-to-member to apply offsets
 * without hardcoded field name checks. It provides:
 * - Runtime discovery of OneToMany relations from file structure
 * - Generic offset application using member pointer maps
 * - Single function to handle all collection types
 * - No compile-time knowledge of field names required
 * 
 * Usage example:
 *   // Discover relations from file
 *   auto relations = extractAllOneToManyRelations(file_path);
 *   
 *   // Apply offsets generically
 *   applyOffsetsGeneric(particles, offset, relations["MCParticles"]);
 */
class IndexOffsetHelper {
public:
    /**
     * Generic field accessor using pointer-to-member for begin/end pairs.
     * Allows accessing struct members without hardcoded field names.
     */
    template<typename T>
    struct FieldAccessor {
        unsigned int T::*begin_member;
        unsigned int T::*end_member;
        std::string field_name;
        
        FieldAccessor(unsigned int T::*begin, unsigned int T::*end, const std::string& name)
            : begin_member(begin), end_member(end), field_name(name) {}
    };
    
    /**
     * Registry of field accessors for MCParticleData.
     * Maps field names to member pointers.
     */
    static const std::vector<FieldAccessor<edm4hep::MCParticleData>>& getMCParticleFieldAccessors() {
        static std::vector<FieldAccessor<edm4hep::MCParticleData>> accessors = {
            {&edm4hep::MCParticleData::parents_begin, &edm4hep::MCParticleData::parents_end, "parents"},
            {&edm4hep::MCParticleData::daughters_begin, &edm4hep::MCParticleData::daughters_end, "daughters"}
        };
        return accessors;
    }
    
    /**
     * Registry of field accessors for SimCalorimeterHitData.
     * Maps field names to member pointers.
     */
    static const std::vector<FieldAccessor<edm4hep::SimCalorimeterHitData>>& getCaloHitFieldAccessors() {
        static std::vector<FieldAccessor<edm4hep::SimCalorimeterHitData>> accessors = {
            {&edm4hep::SimCalorimeterHitData::contributions_begin, &edm4hep::SimCalorimeterHitData::contributions_end, "contributions"}
        };
        return accessors;
    }
    /**
     * Metadata about which fields need offsets for a given collection type.
     * Each entry is a field name prefix (e.g., "parents" for parents_begin/parents_end).
     */
    struct OffsetFieldMetadata {
        std::string collection_type;
        std::vector<std::string> offset_field_prefixes;
        
        // Human-readable description of what this collection type is
        std::string description;
    };
    
    /**
     * Generic offset application using field accessors.
     * This function works with any data type that has registered field accessors.
     * No hardcoded field name checks required.
     * 
     * @param data Vector of data objects
     * @param offset The offset to add to index fields
     * @param field_names Vector of field names that need offsets (from runtime discovery)
     * @param accessors Registry of field accessors for this data type
     */
    template<typename T>
    static void applyOffsetsGeneric(std::vector<T>& data,
                                    size_t offset,
                                    const std::vector<std::string>& field_names,
                                    const std::vector<FieldAccessor<T>>& accessors) {
        // For each data object
        for (auto& item : data) {
            // For each discovered field name
            for (const auto& field_name : field_names) {
                // Find the matching accessor by field name
                for (const auto& accessor : accessors) {
                    if (accessor.field_name == field_name) {
                        // Apply offset using pointer-to-member
                        item.*(accessor.begin_member) += offset;
                        item.*(accessor.end_member) += offset;
                        break;
                    }
                }
            }
        }
    }
    
    /**
     * Apply index offsets to MCParticle data using discovered field names.
     * This version uses the generic offset application with field accessors.
     * 
     * @param particles Vector of MCParticle data objects
     * @param offset The offset to add to index fields
     * @param field_names Vector of field name prefixes that need offsets
     */
    static void applyMCParticleOffsets(std::vector<edm4hep::MCParticleData>& particles, 
                                       size_t offset,
                                       const std::vector<std::string>& field_names) {
        applyOffsetsGeneric(particles, offset, field_names, getMCParticleFieldAccessors());
    }
    
    /**
     * Apply index offsets to MCParticle data.
     * Applies offsets to: parents_begin, parents_end, daughters_begin, daughters_end
     * 
     * These fields are indices into the ObjectID vectors for parent and daughter particles.
     * When merging events, particle indices need to be adjusted to account for particles
     * from previous events.
     * 
     * @param particles Vector of MCParticle data objects
     * @param offset The offset to add to index fields
     */
    static void applyMCParticleOffsets(std::vector<edm4hep::MCParticleData>& particles, size_t offset) {
        // Use the version with explicit field names for backward compatibility
        std::vector<std::string> default_fields = {"parents", "daughters"};
        applyMCParticleOffsets(particles, offset, default_fields);
    }
    
    /**
     * Apply index offsets to SimCalorimeterHit data using discovered field names.
     * This version uses the generic offset application with field accessors.
     * 
     * @param hits Vector of SimCalorimeterHit data objects
     * @param offset The offset to add to index fields
     * @param field_names Vector of field name prefixes that need offsets
     */
    static void applyCaloHitOffsets(std::vector<edm4hep::SimCalorimeterHitData>& hits, 
                                    size_t offset,
                                    const std::vector<std::string>& field_names) {
        applyOffsetsGeneric(hits, offset, field_names, getCaloHitFieldAccessors());
    }
    
    /**
     * Apply index offsets to SimCalorimeterHit data.
     * Applies offsets to: contributions_begin, contributions_end
     * 
     * These fields are indices into the CaloHitContribution vector.
     * When merging events, contribution indices need to be adjusted to account for
     * contributions from previous events.
     * 
     * @param hits Vector of SimCalorimeterHit data objects
     * @param offset The offset to add to index fields
     */
    static void applyCaloHitOffsets(std::vector<edm4hep::SimCalorimeterHitData>& hits, size_t offset) {
        // Use the version with explicit field names for backward compatibility
        std::vector<std::string> default_fields = {"contributions"};
        applyCaloHitOffsets(hits, offset, default_fields);
    }
    
    /**
     * Get metadata about which fields need offsets for MCParticle collections.
     * This can be used to understand the structure without hardcoding in multiple places.
     * 
     * @return Metadata describing the offset requirements for MCParticle collections
     */
    static OffsetFieldMetadata getMCParticleOffsetMetadata() {
        return {
            "MCParticles", 
            {"parents", "daughters"},
            "MC truth particles with parent-child relationships"
        };
    }
    
    /**
     * Get metadata about which fields need offsets for SimCalorimeterHit collections.
     * This can be used to understand the structure without hardcoding in multiple places.
     * 
     * @return Metadata describing the offset requirements for SimCalorimeterHit collections
     */
    static OffsetFieldMetadata getCaloHitOffsetMetadata() {
        return {
            "SimCalorimeterHit", 
            {"contributions"},
            "Simulated calorimeter hits with energy contributions"
        };
    }
    
    /**
     * Get all registered offset metadata.
     * This provides a complete map of all collection types and their offset requirements.
     * 
     * @return Vector of all offset metadata for all supported collection types
     */
    static std::vector<OffsetFieldMetadata> getAllOffsetMetadata() {
        return {
            getMCParticleOffsetMetadata(),
            getCaloHitOffsetMetadata()
        };
    }
    
    /**
     * Infer which fields need offsets for a collection based on its ObjectID branch names.
     * 
     * Given a collection name and a list of ObjectID branch names, this function
     * extracts the field names that need offsets.
     * 
     * Example: For "MCParticles" with branches ["_MCParticles_parents", "_MCParticles_daughters"]
     *          Returns: ["parents", "daughters"]
     * 
     * This allows the system to automatically determine offset requirements by analyzing
     * the ROOT branch structure rather than hardcoding the information.
     * 
     * @param collection_name The name of the collection (e.g., "MCParticles")
     * @param objectid_branch_names List of ObjectID branch names
     * @return Vector of field name prefixes that need offsets
     */
    static std::vector<std::string> inferOffsetFieldsFromBranches(
        const std::string& collection_name,
        const std::vector<std::string>& objectid_branch_names) 
    {
        std::vector<std::string> field_prefixes;
        std::string prefix = "_" + collection_name + "_";
        
        for (const auto& branch_name : objectid_branch_names) {
            // Check if this branch belongs to the collection
            if (branch_name.find(prefix) == 0) {
                // Extract the field name after the prefix
                std::string field_name = branch_name.substr(prefix.length());
                field_prefixes.push_back(field_name);
            }
        }
        
        return field_prefixes;
    }
    
    /**
     * Create an OffsetFieldMetadata by inferring from ObjectID branches.
     * 
     * This enables automatic discovery of offset requirements from the file structure,
     * reducing the need for hardcoded configuration.
     * 
     * @param collection_name The name of the collection
     * @param objectid_branch_names List of all ObjectID branch names
     * @return Metadata describing the offset requirements for this collection
     */
    static OffsetFieldMetadata createMetadataFromBranches(
        const std::string& collection_name,
        const std::vector<std::string>& objectid_branch_names)
    {
        OffsetFieldMetadata metadata;
        metadata.collection_type = collection_name;
        metadata.offset_field_prefixes = inferOffsetFieldsFromBranches(collection_name, objectid_branch_names);
        metadata.description = "Inferred from branch structure";
        return metadata;
    }
    
    /**
     * Extract OneToMany relation field names from a ROOT file by analyzing the branch structure.
     * This is a runtime approach that discovers which fields need offsets without compile-time knowledge.
     * 
     * The method looks at the ObjectID branches in the file to determine which fields
     * represent OneToMany relations (e.g., parents, daughters, contributions).
     * 
     * @param file_path Path to the ROOT file
     * @param collection_name Name of the collection to analyze (e.g., "MCParticles")
     * @return Vector of field name prefixes that need offsets (e.g., ["parents", "daughters"])
     */
    static std::vector<std::string> extractOneToManyFieldsFromFile(
        const std::string& file_path,
        const std::string& collection_name)
    {
        std::vector<std::string> field_names;
        
        try {
            TFile* file = TFile::Open(file_path.c_str(), "READ");
            if (!file || file->IsZombie()) {
                return field_names;
            }
            
            // Get the events tree to analyze branches
            TTree* events_tree = dynamic_cast<TTree*>(file->Get("events"));
            if (!events_tree) {
                file->Close();
                delete file;
                return field_names;
            }
            
            // Get all branches
            TObjArray* branches = events_tree->GetListOfBranches();
            if (!branches) {
                file->Close();
                delete file;
                return field_names;
            }
            
            // Look for ObjectID branches that belong to this collection
            // Pattern: _<CollectionName>_<fieldname>
            std::string prefix = "_" + collection_name + "_";
            
            for (int i = 0; i < branches->GetEntries(); ++i) {
                TBranch* branch = dynamic_cast<TBranch*>(branches->At(i));
                if (!branch) continue;
                
                std::string branch_name = branch->GetName();
                
                // Check if this is an ObjectID branch for our collection
                if (branch_name.find(prefix) == 0) {
                    // Check if it's a vector of ObjectID (indicating OneToMany relation)
                    std::string class_name = branch->GetClassName();
                    if (class_name.find("vector") != std::string::npos && 
                        class_name.find("ObjectID") != std::string::npos) {
                        // Extract the field name
                        std::string field_name = branch_name.substr(prefix.length());
                        field_names.push_back(field_name);
                    }
                }
            }
            
            file->Close();
            delete file;
            
        } catch (...) {
            // Silently fail - return empty vector
        }
        
        return field_names;
    }
    
    /**
     * Create a map of all collections in a file and their OneToMany relation fields.
     * This discovers all offset requirements from the file structure at runtime.
     * 
     * @param file_path Path to the ROOT file
     * @return Map from collection name to list of OneToMany field names
     */
    static std::map<std::string, std::vector<std::string>> extractAllOneToManyRelations(
        const std::string& file_path)
    {
        std::map<std::string, std::vector<std::string>> relations;
        
        try {
            TFile* file = TFile::Open(file_path.c_str(), "READ");
            if (!file || file->IsZombie()) {
                return relations;
            }
            
            TTree* events_tree = dynamic_cast<TTree*>(file->Get("events"));
            if (!events_tree) {
                file->Close();
                delete file;
                return relations;
            }
            
            TObjArray* branches = events_tree->GetListOfBranches();
            if (!branches) {
                file->Close();
                delete file;
                return relations;
            }
            
            // Find all ObjectID branches and group them by collection
            for (int i = 0; i < branches->GetEntries(); ++i) {
                TBranch* branch = dynamic_cast<TBranch*>(branches->At(i));
                if (!branch) continue;
                
                std::string branch_name = branch->GetName();
                
                // Check if this is an ObjectID branch (pattern: _<CollectionName>_<fieldname>)
                if (branch_name.size() > 0 && branch_name[0] == '_') {
                    std::string class_name = branch->GetClassName();
                    if (class_name.find("vector") != std::string::npos && 
                        class_name.find("ObjectID") != std::string::npos) {
                        
                        // Extract collection name and field name
                        size_t second_underscore = branch_name.find('_', 1);
                        if (second_underscore != std::string::npos) {
                            std::string collection_name = branch_name.substr(1, second_underscore - 1);
                            std::string field_name = branch_name.substr(second_underscore + 1);
                            
                            relations[collection_name].push_back(field_name);
                        }
                    }
                }
            }
            
            file->Close();
            delete file;
            
        } catch (...) {
            // Silently fail - return what we have
        }
        
        return relations;
    }
};
