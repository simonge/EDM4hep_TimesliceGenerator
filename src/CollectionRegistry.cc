#include "CollectionRegistry.h"
#include "StandaloneTimesliceMerger.h"
#include <iostream>

std::map<std::string, CollectionDescriptor>& CollectionRegistry::getRegistry() {
    static std::map<std::string, CollectionDescriptor> registry;
    return registry;
}

void CollectionRegistry::registerDescriptor(const std::string& collection_name, CollectionDescriptor&& desc) {
    getRegistry()[collection_name] = std::move(desc);
}

const CollectionDescriptor* CollectionRegistry::getDescriptor(const std::string& collection_name) {
    auto& registry = getRegistry();
    auto it = registry.find(collection_name);
    if (it != registry.end()) {
        return &it->second;
    }
    return nullptr;
}

void CollectionRegistry::initialize(MergedCollections& merged) {
    auto& registry = getRegistry();
    registry.clear();
    
    // MCParticles
    {
        CollectionDescriptor desc;
        desc.type_name = "MCParticles";
        desc.merge_function = [](std::any& data, MergedCollections& m, const std::string&) {
            auto* coll = std::any_cast<std::vector<edm4hep::MCParticleData>>(&data);
            if (coll) {
                m.mcparticles.insert(m.mcparticles.end(),
                                    std::make_move_iterator(coll->begin()),
                                    std::make_move_iterator(coll->end()));
            }
        };
        desc.get_size_function = [](const MergedCollections& m, const std::string&) {
            return m.mcparticles.size();
        };
        registerDescriptor("MCParticles", std::move(desc));
    }
    
    // _MCParticles_parents
    {
        CollectionDescriptor desc;
        desc.type_name = "ObjectID";
        desc.merge_function = [](std::any& data, MergedCollections& m, const std::string&) {
            auto* coll = std::any_cast<std::vector<podio::ObjectID>>(&data);
            if (coll) {
                m.mcparticle_parents_refs.insert(m.mcparticle_parents_refs.end(),
                                                std::make_move_iterator(coll->begin()),
                                                std::make_move_iterator(coll->end()));
            }
        };
        desc.get_size_function = [](const MergedCollections& m, const std::string&) {
            return m.mcparticle_parents_refs.size();
        };
        registerDescriptor("_MCParticles_parents", std::move(desc));
    }
    
    // _MCParticles_daughters
    {
        CollectionDescriptor desc;
        desc.type_name = "ObjectID";
        desc.merge_function = [](std::any& data, MergedCollections& m, const std::string&) {
            auto* coll = std::any_cast<std::vector<podio::ObjectID>>(&data);
            if (coll) {
                m.mcparticle_children_refs.insert(m.mcparticle_children_refs.end(),
                                                  std::make_move_iterator(coll->begin()),
                                                  std::make_move_iterator(coll->end()));
            }
        };
        desc.get_size_function = [](const MergedCollections& m, const std::string&) {
            return m.mcparticle_children_refs.size();
        };
        registerDescriptor("_MCParticles_daughters", std::move(desc));
    }
    
    // GP collections
    {
        CollectionDescriptor desc;
        desc.type_name = "GPIntValues";
        desc.merge_function = [](std::any& data, MergedCollections& m, const std::string&) {
            auto* coll = std::any_cast<std::vector<std::vector<int>>>(&data);
            if (coll) {
                m.gp_int_values.insert(m.gp_int_values.end(),
                                      std::make_move_iterator(coll->begin()),
                                      std::make_move_iterator(coll->end()));
            }
        };
        desc.get_size_function = [](const MergedCollections& m, const std::string&) {
            return m.gp_int_values.size();
        };
        registerDescriptor("GPIntValues", std::move(desc));
    }
    
    {
        CollectionDescriptor desc;
        desc.type_name = "GPFloatValues";
        desc.merge_function = [](std::any& data, MergedCollections& m, const std::string&) {
            auto* coll = std::any_cast<std::vector<std::vector<float>>>(&data);
            if (coll) {
                m.gp_float_values.insert(m.gp_float_values.end(),
                                        std::make_move_iterator(coll->begin()),
                                        std::make_move_iterator(coll->end()));
            }
        };
        desc.get_size_function = [](const MergedCollections& m, const std::string&) {
            return m.gp_float_values.size();
        };
        registerDescriptor("GPFloatValues", std::move(desc));
    }
    
    {
        CollectionDescriptor desc;
        desc.type_name = "GPDoubleValues";
        desc.merge_function = [](std::any& data, MergedCollections& m, const std::string&) {
            auto* coll = std::any_cast<std::vector<std::vector<double>>>(&data);
            if (coll) {
                m.gp_double_values.insert(m.gp_double_values.end(),
                                         std::make_move_iterator(coll->begin()),
                                         std::make_move_iterator(coll->end()));
            }
        };
        desc.get_size_function = [](const MergedCollections& m, const std::string&) {
            return m.gp_double_values.size();
        };
        registerDescriptor("GPDoubleValues", std::move(desc));
    }
    
    {
        CollectionDescriptor desc;
        desc.type_name = "GPStringValues";
        desc.merge_function = [](std::any& data, MergedCollections& m, const std::string&) {
            auto* coll = std::any_cast<std::vector<std::vector<std::string>>>(&data);
            if (coll) {
                m.gp_string_values.insert(m.gp_string_values.end(),
                                         std::make_move_iterator(coll->begin()),
                                         std::make_move_iterator(coll->end()));
            }
        };
        desc.get_size_function = [](const MergedCollections& m, const std::string&) {
            return m.gp_string_values.size();
        };
        registerDescriptor("GPStringValues", std::move(desc));
    }
    
    // Note: Dynamic collections (tracker hits, calo hits, etc.) are registered
    // dynamically when discovered, as their names are not known at compile time
}
