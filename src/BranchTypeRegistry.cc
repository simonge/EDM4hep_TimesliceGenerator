#include "BranchTypeRegistry.h"
#include <algorithm>

const std::vector<BranchTypeRegistry::TypeMapping>& BranchTypeRegistry::getTypeMappings() {
    static const std::vector<TypeMapping> mappings = {
        {"vector<edm4hep::SimTrackerHitData>", BranchCategory::TRACKER_HIT},
        {"vector<edm4hep::SimCalorimeterHitData>", BranchCategory::CALORIMETER_HIT},
        {"vector<edm4hep::CaloHitContributionData>", BranchCategory::CONTRIBUTION},
        {"vector<edm4hep::MCParticleData>", BranchCategory::MC_PARTICLE},
        {"vector<edm4hep::EventHeaderData>", BranchCategory::EVENT_HEADER},
        {"vector<podio::ObjectID>", BranchCategory::OBJECTID_REF},
        {"vector<vector<int>>", BranchCategory::GP_VALUE},
        {"vector<vector<float>>", BranchCategory::GP_VALUE},
        {"vector<vector<double>>", BranchCategory::GP_VALUE},
        {"vector<vector<string>>", BranchCategory::GP_VALUE},
        {"vector<string>", BranchCategory::GP_KEY}
    };
    return mappings;
}

const std::vector<BranchTypeRegistry::NameMapping>& BranchTypeRegistry::getNameMappings() {
    static const std::vector<NameMapping> mappings = {
        {
            [](const std::string& name) { 
                return name.find("GPIntKeys") == 0 || name.find("GPFloatKeys") == 0 || 
                       name.find("GPDoubleKeys") == 0 || name.find("GPStringKeys") == 0;
            },
            BranchCategory::GP_KEY
        },
        {
            [](const std::string& name) { 
                return name == "GPIntValues" || name == "GPFloatValues" || 
                       name == "GPDoubleValues" || name == "GPStringValues";
            },
            BranchCategory::GP_VALUE
        },
        {
            [](const std::string& name) { 
                return name.find("_") == 0 && name.find("_particle") != std::string::npos;
            },
            BranchCategory::OBJECTID_REF
        },
        {
            [](const std::string& name) { 
                return name.find("_") == 0 && (name.find("_parents") != std::string::npos || 
                                                name.find("_daughters") != std::string::npos ||
                                                name.find("_contributions") != std::string::npos);
            },
            BranchCategory::OBJECTID_REF
        }
    };
    return mappings;
}

const std::vector<std::string>& BranchTypeRegistry::getGPPatterns() {
    static const std::vector<std::string> patterns = {
        "GPIntKeys", "GPFloatKeys", "GPStringKeys", "GPDoubleKeys",
        "GPIntValues", "GPFloatValues", "GPStringValues", "GPDoubleValues"
    };
    return patterns;
}

BranchTypeRegistry::BranchCategory BranchTypeRegistry::getCategoryForType(const std::string& type_string) {
    for (const auto& mapping : getTypeMappings()) {
        if (type_string.find(mapping.type_pattern) != std::string::npos) {
            return mapping.category;
        }
    }
    return BranchCategory::UNKNOWN;
}

BranchTypeRegistry::BranchCategory BranchTypeRegistry::getCategoryForName(const std::string& branch_name) {
    for (const auto& mapping : getNameMappings()) {
        if (mapping.matcher(branch_name)) {
            return mapping.category;
        }
    }
    return BranchCategory::UNKNOWN;
}

bool BranchTypeRegistry::isGPBranch(const std::string& branch_name) {
    for (const auto& pattern : getGPPatterns()) {
        if (branch_name.find(pattern) == 0) {
            return true;
        }
    }
    return false;
}

bool BranchTypeRegistry::isObjectIDRef(const std::string& branch_name) {
    return branch_name.find("_") == 0;
}

bool BranchTypeRegistry::isParticleRef(const std::string& branch_name) {
    return branch_name.find("_") == 0 && branch_name.find("_particle") != std::string::npos;
}

bool BranchTypeRegistry::isContributionRef(const std::string& branch_name) {
    return branch_name.find("_") == 0 && branch_name.find("_contributions") != std::string::npos;
}

std::vector<std::string> BranchTypeRegistry::getTypePatternsForCategory(BranchCategory category) {
    std::vector<std::string> patterns;
    for (const auto& mapping : getTypeMappings()) {
        if (mapping.category == category) {
            patterns.push_back(mapping.type_pattern);
        }
    }
    return patterns;
}

std::vector<std::string> BranchTypeRegistry::getGPNamePatterns() {
    return getGPPatterns();
}
