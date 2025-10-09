#include "CollectionMergeHandler.h"
#include "StandaloneTimesliceMerger.h"
#include <iostream>

void CollectionMergeRegistry::initializeRegistry() {
    
    // ========== MCParticles ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "MCParticles";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            float_offsets["time"] = time_offset;
            int_offsets["generatorStatus"] = gen_status_offset;
            
            // Index offsets from OneToMany relations
            auto it = one_to_many_relations.find("MCParticles");
            if (it != one_to_many_relations.end()) {
                for (const auto& field_name : it->second) {
                    size_t_offsets[field_name] = collection_offsets.at("MCParticles");
                }
            }
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* particles = std::any_cast<std::vector<edm4hep::MCParticleData>>(&collection_data);
            
            if (should_process) {
                CollectionProcessor::processCollection(
                    particles, "MCParticles",
                    float_offsets, int_offsets, size_t_offsets,
                    already_merged);
            }
            
            // Move to merged collection
            merged_collections.mcparticles.insert(merged_collections.mcparticles.end(),
                                                  std::make_move_iterator(particles->begin()),
                                                  std::make_move_iterator(particles->end()));
        };
        
        registerHandler("MCParticles", std::move(handler));
    }
    
    // ========== ObjectID (for MCParticle references) ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "MCParticleObjectID";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            // No offset maps needed, processed directly
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* refs = std::any_cast<std::vector<podio::ObjectID>>(&collection_data);
            
            if (should_process) {
                // Find the appropriate offset from collection_offsets map
                // For MCParticle references, we use MCParticles offset
                size_t offset = 0;
                for (const auto& [key, val] : size_t_offsets) {
                    offset = val;  // Use the first offset (MCParticles)
                    break;
                }
                CollectionProcessor::processObjectIDReferences(*refs, offset);
            }
            
            if (collection_name == "_MCParticles_parents") {
                merged_collections.mcparticle_parents_refs.insert(
                    merged_collections.mcparticle_parents_refs.end(),
                    std::make_move_iterator(refs->begin()),
                    std::make_move_iterator(refs->end()));
            } else if (collection_name == "_MCParticles_daughters") {
                merged_collections.mcparticle_children_refs.insert(
                    merged_collections.mcparticle_children_refs.end(),
                    std::make_move_iterator(refs->begin()),
                    std::make_move_iterator(refs->end()));
            }
        };
        
        registerHandler("MCParticleObjectID", std::move(handler));
    }
    
    // ========== SimTrackerHit ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "SimTrackerHit";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            float_offsets["time"] = time_offset;
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* hits = std::any_cast<std::vector<edm4hep::SimTrackerHitData>>(&collection_data);
            
            if (should_process) {
                CollectionProcessor::processCollection(
                    hits, "SimTrackerHit",
                    float_offsets, int_offsets, size_t_offsets,
                    already_merged);
            }
            
            merged_collections.tracker_hits[collection_name].insert(
                merged_collections.tracker_hits[collection_name].end(),
                std::make_move_iterator(hits->begin()),
                std::make_move_iterator(hits->end()));
        };
        
        registerHandler("SimTrackerHit", std::move(handler));
    }
    
    // ========== TrackerHitParticleRef ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "TrackerHitParticleRef";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            // No offset maps needed
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* refs = std::any_cast<std::vector<podio::ObjectID>>(&collection_data);
            
            if (should_process) {
                size_t offset = 0;
                for (const auto& [key, val] : size_t_offsets) {
                    offset = val;
                    break;
                }
                CollectionProcessor::processObjectIDReferences(*refs, offset);
            }
            
            // Extract tracker collection name
            std::string base_name = collection_name.substr(1, collection_name.find("_particle") - 1);
            
            merged_collections.tracker_hit_particle_refs[base_name].insert(
                merged_collections.tracker_hit_particle_refs[base_name].end(),
                std::make_move_iterator(refs->begin()),
                std::make_move_iterator(refs->end()));
        };
        
        registerHandler("TrackerHitParticleRef", std::move(handler));
    }
    
    // ========== SimCalorimeterHit ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "SimCalorimeterHit";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            // Index offsets from OneToMany relations
            auto it = one_to_many_relations.find(collection_name);
            if (it != one_to_many_relations.end()) {
                for (const auto& field_name : it->second) {
                    auto contrib_it = collection_offsets.find(collection_name + "Contributions");
                    if (contrib_it != collection_offsets.end()) {
                        size_t_offsets[field_name] = contrib_it->second;
                    }
                }
            }
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* hits = std::any_cast<std::vector<edm4hep::SimCalorimeterHitData>>(&collection_data);
            
            if (should_process) {
                CollectionProcessor::processCollection(
                    hits, "SimCalorimeterHit",
                    float_offsets, int_offsets, size_t_offsets,
                    already_merged);
            }
            
            merged_collections.calo_hits[collection_name].insert(
                merged_collections.calo_hits[collection_name].end(),
                std::make_move_iterator(hits->begin()),
                std::make_move_iterator(hits->end()));
        };
        
        registerHandler("SimCalorimeterHit", std::move(handler));
    }
    
    // ========== CaloHitContributionRef ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "CaloHitContributionRef";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            // No offset maps needed
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* refs = std::any_cast<std::vector<podio::ObjectID>>(&collection_data);
            
            std::string base_name = collection_name.substr(1, collection_name.find("_contributions") - 1);
            
            if (should_process) {
                size_t offset = 0;
                for (const auto& [key, val] : size_t_offsets) {
                    offset = val;
                    break;
                }
                CollectionProcessor::processObjectIDReferences(*refs, offset);
            }
            
            merged_collections.calo_hit_contributions_refs[base_name].insert(
                merged_collections.calo_hit_contributions_refs[base_name].end(),
                std::make_move_iterator(refs->begin()),
                std::make_move_iterator(refs->end()));
        };
        
        registerHandler("CaloHitContributionRef", std::move(handler));
    }
    
    // ========== CaloHitContribution ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "CaloHitContribution";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            float_offsets["time"] = time_offset;
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* contribs = std::any_cast<std::vector<edm4hep::CaloHitContributionData>>(&collection_data);
            
            if (should_process) {
                CollectionProcessor::processCollection(
                    contribs, "CaloHitContribution",
                    float_offsets, int_offsets, size_t_offsets,
                    already_merged);
            }
            
            // Extract base calo name
            std::string base_name = collection_name;
            if (base_name.length() > 13 && base_name.substr(base_name.length() - 13) == "Contributions") {
                base_name = base_name.substr(0, base_name.length() - 13);
            }
            
            merged_collections.calo_contributions[base_name].insert(
                merged_collections.calo_contributions[base_name].end(),
                std::make_move_iterator(contribs->begin()),
                std::make_move_iterator(contribs->end()));
        };
        
        registerHandler("CaloHitContribution", std::move(handler));
    }
    
    // ========== CaloContribParticleRef ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "CaloContribParticleRef";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            // No offset maps needed
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* refs = std::any_cast<std::vector<podio::ObjectID>>(&collection_data);
            
            if (should_process) {
                size_t offset = 0;
                for (const auto& [key, val] : size_t_offsets) {
                    offset = val;
                    break;
                }
                CollectionProcessor::processObjectIDReferences(*refs, offset);
            }
            
            // Extract base calo collection name
            std::string base_name = collection_name.substr(1);  // Remove leading _
            base_name = base_name.substr(0, base_name.find("Contributions_particle"));
            
            merged_collections.calo_contrib_particle_refs[base_name].insert(
                merged_collections.calo_contrib_particle_refs[base_name].end(),
                std::make_move_iterator(refs->begin()),
                std::make_move_iterator(refs->end()));
        };
        
        registerHandler("CaloContribParticleRef", std::move(handler));
    }
    
    // ========== GP Key Branches ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "GPKeys";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            // No processing needed
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* gp_keys = std::any_cast<std::vector<std::string>>(&collection_data);
            
            merged_collections.gp_key_branches[collection_name].insert(
                merged_collections.gp_key_branches[collection_name].end(),
                std::make_move_iterator(gp_keys->begin()),
                std::make_move_iterator(gp_keys->end()));
        };
        
        registerHandler("GPKeys", std::move(handler));
    }
    
    // ========== GP Int Values ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "GPIntValues";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            // No processing needed
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* gp_values = std::any_cast<std::vector<std::vector<int>>>(&collection_data);
            merged_collections.gp_int_values.insert(
                merged_collections.gp_int_values.end(),
                std::make_move_iterator(gp_values->begin()),
                std::make_move_iterator(gp_values->end()));
        };
        
        registerHandler("GPIntValues", std::move(handler));
    }
    
    // ========== GP Float Values ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "GPFloatValues";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            // No processing needed
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* gp_values = std::any_cast<std::vector<std::vector<float>>>(&collection_data);
            merged_collections.gp_float_values.insert(
                merged_collections.gp_float_values.end(),
                std::make_move_iterator(gp_values->begin()),
                std::make_move_iterator(gp_values->end()));
        };
        
        registerHandler("GPFloatValues", std::move(handler));
    }
    
    // ========== GP Double Values ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "GPDoubleValues";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            // No processing needed
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* gp_values = std::any_cast<std::vector<std::vector<double>>>(&collection_data);
            merged_collections.gp_double_values.insert(
                merged_collections.gp_double_values.end(),
                std::make_move_iterator(gp_values->begin()),
                std::make_move_iterator(gp_values->end()));
        };
        
        registerHandler("GPDoubleValues", std::move(handler));
    }
    
    // ========== GP String Values ==========
    {
        CollectionMergeHandler handler;
        handler.collection_type = "GPStringValues";
        
        handler.build_offset_maps = [](
            float time_offset,
            int gen_status_offset,
            const std::map<std::string, size_t>& collection_offsets,
            const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
            const std::string& collection_name,
            std::map<std::string, float>& float_offsets,
            std::map<std::string, int>& int_offsets,
            std::map<std::string, size_t>& size_t_offsets
        ) {
            // No processing needed
        };
        
        handler.process_and_merge = [](
            std::any& collection_data,
            const std::string& collection_name,
            bool should_process,
            const std::map<std::string, float>& float_offsets,
            const std::map<std::string, int>& int_offsets,
            const std::map<std::string, size_t>& size_t_offsets,
            bool already_merged,
            MergedCollections& merged_collections
        ) {
            auto* gp_values = std::any_cast<std::vector<std::vector<std::string>>>(&collection_data);
            merged_collections.gp_string_values.insert(
                merged_collections.gp_string_values.end(),
                std::make_move_iterator(gp_values->begin()),
                std::make_move_iterator(gp_values->end()));
        };
        
        registerHandler("GPStringValues", std::move(handler));
    }
}
