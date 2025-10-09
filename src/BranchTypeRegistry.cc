#include "BranchTypeRegistry.h"
#include "StandaloneTimesliceMerger.h"
#include <algorithm>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <podio/ObjectID.h>

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

bool BranchTypeRegistry::isContributionParticleRef(const std::string& branch_name) {
    return branch_name.find("Contributions_particle") != std::string::npos;
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

BranchTypeRegistry::CollectionHandler BranchTypeRegistry::getHandlerForCategory(BranchCategory category) {
    static std::map<BranchCategory, CollectionHandler> handlers;
    
    // Initialize handlers on first call
    if (handlers.empty()) {
        // Tracker hits handler
        handlers[BranchCategory::TRACKER_HIT] = [](std::any& collection_data, MergedCollections& merged, const std::string& collection_name) {
            auto* hits = std::any_cast<std::vector<edm4hep::SimTrackerHitData>>(&collection_data);
            if (hits) {
                merged.tracker_hits[collection_name].insert(
                    merged.tracker_hits[collection_name].end(),
                    std::make_move_iterator(hits->begin()),
                    std::make_move_iterator(hits->end()));
            }
        };
        
        // Calorimeter hits handler
        handlers[BranchCategory::CALORIMETER_HIT] = [](std::any& collection_data, MergedCollections& merged, const std::string& collection_name) {
            auto* hits = std::any_cast<std::vector<edm4hep::SimCalorimeterHitData>>(&collection_data);
            if (hits) {
                merged.calo_hits[collection_name].insert(
                    merged.calo_hits[collection_name].end(),
                    std::make_move_iterator(hits->begin()),
                    std::make_move_iterator(hits->end()));
            }
        };
        
        // Contribution handler
        handlers[BranchCategory::CONTRIBUTION] = [](std::any& collection_data, MergedCollections& merged, const std::string& collection_name) {
            auto* contribs = std::any_cast<std::vector<edm4hep::CaloHitContributionData>>(&collection_data);
            if (contribs) {
                std::string base_name = collection_name;
                if (base_name.length() > 13 && base_name.substr(base_name.length() - 13) == "Contributions") {
                    base_name = base_name.substr(0, base_name.length() - 13);
                }
                merged.calo_contributions[base_name].insert(
                    merged.calo_contributions[base_name].end(),
                    std::make_move_iterator(contribs->begin()),
                    std::make_move_iterator(contribs->end()));
            }
        };
        
        // ObjectID reference handler
        handlers[BranchCategory::OBJECTID_REF] = [](std::any& collection_data, MergedCollections& merged, const std::string& collection_name) {
            auto* refs = std::any_cast<std::vector<podio::ObjectID>>(&collection_data);
            if (refs) {
                // Use branch name pattern to determine the specific type of reference
                if (isParticleRef(collection_name)) {
                    // Particle reference branches
                    if (isContributionParticleRef(collection_name)) {
                        std::string base_name = collection_name.substr(1);
                        base_name = base_name.substr(0, base_name.find("Contributions_particle"));
                        merged.calo_contrib_particle_refs[base_name].insert(
                            merged.calo_contrib_particle_refs[base_name].end(),
                            std::make_move_iterator(refs->begin()),
                            std::make_move_iterator(refs->end()));
                    } else {
                        std::string base_name = collection_name.substr(1, collection_name.find("_particle") - 1);
                        merged.tracker_hit_particle_refs[base_name].insert(
                            merged.tracker_hit_particle_refs[base_name].end(),
                            std::make_move_iterator(refs->begin()),
                            std::make_move_iterator(refs->end()));
                    }
                }
                else if (isContributionRef(collection_name)) {
                    // Contribution reference branches
                    std::string base_name = collection_name.substr(1, collection_name.find("_contributions") - 1);
                    merged.calo_hit_contributions_refs[base_name].insert(
                        merged.calo_hit_contributions_refs[base_name].end(),
                        std::make_move_iterator(refs->begin()),
                        std::make_move_iterator(refs->end()));
                }
            }
        };
        
        // GP key handler
        handlers[BranchCategory::GP_KEY] = [](std::any& collection_data, MergedCollections& merged, const std::string& collection_name) {
            auto* gp_keys = std::any_cast<std::vector<std::string>>(&collection_data);
            if (gp_keys) {
                merged.gp_key_branches[collection_name].insert(
                    merged.gp_key_branches[collection_name].end(),
                    std::make_move_iterator(gp_keys->begin()),
                    std::make_move_iterator(gp_keys->end()));
            }
        };
    }
    
    auto it = handlers.find(category);
    if (it != handlers.end()) {
        return it->second;
    }
    return nullptr;
}
