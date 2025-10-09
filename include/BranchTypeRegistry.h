#pragma once

#include <string>
#include <map>
#include <functional>
#include <vector>
#include <any>

// Forward declarations
struct MergedCollections;

/**
 * Registry for branch type patterns and their categories.
 * Eliminates hardcoded type checks by using a lookup system.
 * New data types can be added by simply registering them here.
 * 
 * IMPORTANT: Type categorization should be based on the actual branch data type,
 * not the branch name. Branch names are only used for:
 * 1. Storing collections in maps (as keys)
 * 2. Extracting base names for relationship references (e.g., _particle, _contributions)
 */
class BranchTypeRegistry {
public:
    enum class BranchCategory {
        TRACKER_HIT,
        CALORIMETER_HIT,
        CONTRIBUTION,
        GP_KEY,
        GP_VALUE,
        OBJECTID_REF,
        EVENT_HEADER,
        MC_PARTICLE,
        UNKNOWN
    };
    
    /**
     * Get the category for a branch type string (PRIMARY METHOD)
     * Use this to determine the category of a collection based on its actual data type.
     */
    static BranchCategory getCategoryForType(const std::string& type_string);
    
    /**
     * Get the category for a branch based on its name pattern (RARELY NEEDED)
     * This should rarely be used - prefer getCategoryForType instead.
     * Only needed in cases where the type information is not available.
     */
    static BranchCategory getCategoryForName(const std::string& branch_name);
    
    /**
     * Check if a branch name matches a specific pattern category
     * These are used ONLY to distinguish between different ObjectID reference types
     * when we already know the type is OBJECTID_REF (from getCategoryForType).
     */
    static bool isGPBranch(const std::string& branch_name);
    static bool isObjectIDRef(const std::string& branch_name);
    static bool isParticleRef(const std::string& branch_name);
    static bool isContributionRef(const std::string& branch_name);
    static bool isContributionParticleRef(const std::string& branch_name);
    
    /**
     * Get all registered type patterns for a category
     */
    static std::vector<std::string> getTypePatternsForCategory(BranchCategory category);
    
    /**
     * Get all registered name patterns for GP branches
     */
    static std::vector<std::string> getGPNamePatterns();
    
    /**
     * Generic handler type for processing collections
     * Takes: (collection_data as std::any&, merged_collections, collection_name)
     */
    using CollectionHandler = std::function<void(std::any&, MergedCollections&, const std::string&)>;
    
    /**
     * Get the handler function for a specific category
     * Returns nullptr if no handler is registered for the category
     */
    static CollectionHandler getHandlerForCategory(BranchCategory category);
    
private:
    struct TypeMapping {
        std::string type_pattern;  // Pattern to match in type string
        BranchCategory category;
    };
    
    struct NameMapping {
        std::function<bool(const std::string&)> matcher;
        BranchCategory category;
    };
    
    static const std::vector<TypeMapping>& getTypeMappings();
    static const std::vector<NameMapping>& getNameMappings();
    static const std::vector<std::string>& getGPPatterns();
};
