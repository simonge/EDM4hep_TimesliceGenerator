#pragma once

#include <string>
#include <map>
#include <functional>
#include <vector>

/**
 * Registry for branch type patterns and their categories.
 * Eliminates hardcoded type checks by using a lookup system.
 * New data types can be added by simply registering them here.
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
     * Get the category for a branch type string
     */
    static BranchCategory getCategoryForType(const std::string& type_string);
    
    /**
     * Get the category for a branch based on its name pattern
     */
    static BranchCategory getCategoryForName(const std::string& branch_name);
    
    /**
     * Check if a branch name matches a specific pattern category
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
