#include "CollectionProcessor.h"
#include <iostream>

// Type-erased updater functions for MCParticleData
namespace {
    void applyFloatOffsetMCParticle(void* data, const std::string& field_name, float offset, bool is_range) {
        auto& particles = *static_cast<std::vector<edm4hep::MCParticleData>*>(data);
        
        if (field_name == "time") {
            for (auto& p : particles) {
                p.time += offset;
            }
        }
        // Add more float fields here if needed
    }
    
    void applyIntOffsetMCParticle(void* data, const std::string& field_name, int offset, bool is_range) {
        auto& particles = *static_cast<std::vector<edm4hep::MCParticleData>*>(data);
        
        if (field_name == "generatorStatus") {
            for (auto& p : particles) {
                p.generatorStatus += offset;
            }
        }
        // Add more int fields here if needed
    }
    
    void applySizeTOffsetMCParticle(void* data, const std::string& field_name, size_t offset, bool is_range) {
        auto& particles = *static_cast<std::vector<edm4hep::MCParticleData>*>(data);
        
        if (!is_range) return;
        
        // Use pointer-to-member for generic access
        const auto& accessors = IndexOffsetHelper::getMCParticleFieldAccessors();
        for (const auto& accessor : accessors) {
            if (accessor.field_name == field_name) {
                for (auto& p : particles) {
                    p.*(accessor.begin_member) += offset;
                    p.*(accessor.end_member) += offset;
                }
                break;
            }
        }
    }
    
    // Type-erased updater functions for SimTrackerHitData
    void applyFloatOffsetTrackerHit(void* data, const std::string& field_name, float offset, bool is_range) {
        auto& hits = *static_cast<std::vector<edm4hep::SimTrackerHitData>*>(data);
        
        if (field_name == "time") {
            for (auto& h : hits) {
                h.time += offset;
            }
        }
    }
    
    void applyIntOffsetTrackerHit(void* data, const std::string& field_name, int offset, bool is_range) {
        // SimTrackerHitData doesn't have int fields that need offsetting
    }
    
    void applySizeTOffsetTrackerHit(void* data, const std::string& field_name, size_t offset, bool is_range) {
        // SimTrackerHitData doesn't have size_t range fields
    }
    
    // Type-erased updater functions for SimCalorimeterHitData
    void applyFloatOffsetCaloHit(void* data, const std::string& field_name, float offset, bool is_range) {
        // SimCalorimeterHitData doesn't have float fields that need offsetting
    }
    
    void applyIntOffsetCaloHit(void* data, const std::string& field_name, int offset, bool is_range) {
        // SimCalorimeterHitData doesn't have int fields that need offsetting
    }
    
    void applySizeTOffsetCaloHit(void* data, const std::string& field_name, size_t offset, bool is_range) {
        auto& hits = *static_cast<std::vector<edm4hep::SimCalorimeterHitData>*>(data);
        
        if (!is_range) return;
        
        // Use pointer-to-member for generic access
        const auto& accessors = IndexOffsetHelper::getCaloHitFieldAccessors();
        for (const auto& accessor : accessors) {
            if (accessor.field_name == field_name) {
                for (auto& h : hits) {
                    h.*(accessor.begin_member) += offset;
                    h.*(accessor.end_member) += offset;
                }
                break;
            }
        }
    }
    
    // Type-erased updater functions for CaloHitContributionData
    void applyFloatOffsetCaloContrib(void* data, const std::string& field_name, float offset, bool is_range) {
        auto& contribs = *static_cast<std::vector<edm4hep::CaloHitContributionData>*>(data);
        
        if (field_name == "time") {
            for (auto& c : contribs) {
                c.time += offset;
            }
        }
    }
    
    void applyIntOffsetCaloContrib(void* data, const std::string& field_name, int offset, bool is_range) {
        // CaloHitContributionData doesn't have int fields that need offsetting
    }
    
    void applySizeTOffsetCaloContrib(void* data, const std::string& field_name, size_t offset, bool is_range) {
        // CaloHitContributionData doesn't have size_t range fields
    }
}

void CollectionProcessor::initializeRegistry() {
    // Register MCParticles
    {
        CollectionMetadata meta("MCParticles", "edm4hep::MCParticleData");
        meta.addField("time", FieldUpdateDescriptor::UpdateType::FLOAT_OFFSET, false);
        meta.addField("generatorStatus", FieldUpdateDescriptor::UpdateType::INT_OFFSET, false);
        // Index offset fields will be added dynamically from OneToMany relations
        // but we need to set up the updater functions
        meta.apply_float_offset = applyFloatOffsetMCParticle;
        meta.apply_int_offset = applyIntOffsetMCParticle;
        meta.apply_size_t_offset = applySizeTOffsetMCParticle;
        
        CollectionMetadataRegistry::registerMetadata("MCParticles", std::move(meta));
    }
    
    // Register SimTrackerHit collections (we'll handle dynamic names in processing)
    {
        CollectionMetadata meta("SimTrackerHit", "edm4hep::SimTrackerHitData");
        meta.addField("time", FieldUpdateDescriptor::UpdateType::FLOAT_OFFSET, false);
        meta.apply_float_offset = applyFloatOffsetTrackerHit;
        meta.apply_int_offset = applyIntOffsetTrackerHit;
        meta.apply_size_t_offset = applySizeTOffsetTrackerHit;
        
        CollectionMetadataRegistry::registerMetadata("SimTrackerHit", std::move(meta));
    }
    
    // Register SimCalorimeterHit collections
    {
        CollectionMetadata meta("SimCalorimeterHit", "edm4hep::SimCalorimeterHitData");
        // Index offset fields will be added dynamically
        meta.apply_float_offset = applyFloatOffsetCaloHit;
        meta.apply_int_offset = applyIntOffsetCaloHit;
        meta.apply_size_t_offset = applySizeTOffsetCaloHit;
        
        CollectionMetadataRegistry::registerMetadata("SimCalorimeterHit", std::move(meta));
    }
    
    // Register CaloHitContribution collections
    {
        CollectionMetadata meta("CaloHitContribution", "edm4hep::CaloHitContributionData");
        meta.addField("time", FieldUpdateDescriptor::UpdateType::FLOAT_OFFSET, false);
        meta.apply_float_offset = applyFloatOffsetCaloContrib;
        meta.apply_int_offset = applyIntOffsetCaloContrib;
        meta.apply_size_t_offset = applySizeTOffsetCaloContrib;
        
        CollectionMetadataRegistry::registerMetadata("CaloHitContribution", std::move(meta));
    }
}

void CollectionProcessor::processCollection(
    void* collection_data,
    const std::string& collection_name,
    const std::map<std::string, float>& float_offsets,
    const std::map<std::string, int>& int_offsets,
    const std::map<std::string, size_t>& size_t_offsets,
    bool already_merged)
{
    // Get metadata - try exact match first, then type-based match
    const CollectionMetadata* meta = CollectionMetadataRegistry::getMetadata(collection_name);
    
    if (!meta) {
        // Try to match by type for dynamic collection names
        // e.g., "ECalBarrelCollection" -> "SimCalorimeterHit"
        // We'll need to detect this from the collection name or have it passed in
        // For now, we'll skip if not found
        std::cerr << "Warning: No metadata found for collection: " << collection_name << std::endl;
        return;
    }
    
    // Apply float offsets
    for (const auto& [field_name, offset] : float_offsets) {
        if (already_merged) continue;  // Skip if already merged
        
        const auto* field = meta->getField(field_name);
        if (field && field->update_type == FieldUpdateDescriptor::UpdateType::FLOAT_OFFSET) {
            if (meta->apply_float_offset) {
                meta->apply_float_offset(collection_data, field_name, offset, field->is_range_field);
            }
        }
    }
    
    // Apply int offsets
    for (const auto& [field_name, offset] : int_offsets) {
        if (already_merged) continue;  // Skip if already merged
        
        const auto* field = meta->getField(field_name);
        if (field && field->update_type == FieldUpdateDescriptor::UpdateType::INT_OFFSET) {
            if (meta->apply_int_offset) {
                meta->apply_int_offset(collection_data, field_name, offset, field->is_range_field);
            }
        }
    }
    
    // Apply size_t offsets (index offsets)
    for (const auto& [field_name, offset] : size_t_offsets) {
        // Index offsets always applied (even for already_merged, since indices change)
        
        const auto* field = meta->getField(field_name);
        if (field && field->update_type == FieldUpdateDescriptor::UpdateType::SIZE_T_OFFSET) {
            if (meta->apply_size_t_offset) {
                meta->apply_size_t_offset(collection_data, field_name, offset, field->is_range_field);
            }
        } else {
            // Field not in static metadata - might be dynamically discovered
            // Try to apply it anyway if the updater supports it
            if (meta->apply_size_t_offset) {
                meta->apply_size_t_offset(collection_data, field_name, offset, true);
            }
        }
    }
}
