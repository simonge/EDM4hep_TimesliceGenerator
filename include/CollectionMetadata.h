#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

/**
 * Completely generic collection processing system using type erasure.
 * No type-specific methods - everything driven by metadata.
 * 
 * The metadata describes what fields to update and how, with function pointers
 * for type-erased generic processing.
 */

/**
 * Describes a single field that needs updating.
 * Can be time, generatorStatus, or any index offset field.
 */
struct FieldUpdateDescriptor {
    std::string field_name;  // e.g., "time", "generatorStatus", "parents", "daughters", "contributions"
    
    enum class UpdateType {
        FLOAT_OFFSET,      // Add a float offset (e.g., time offset)
        INT_OFFSET,        // Add an int offset (e.g., generator status offset)
        SIZE_T_OFFSET      // Add a size_t offset (e.g., index offsets for begin/end pairs)
    };
    
    UpdateType update_type;
    
    // For SIZE_T_OFFSET types with begin/end pairs
    bool is_range_field = false;  // true if this is a begin/end pair
    
    FieldUpdateDescriptor(const std::string& name, UpdateType type, bool is_range = false)
        : field_name(name), update_type(type), is_range_field(is_range) {}
};

/**
 * Complete metadata for processing a collection type.
 * Uses type erasure with void* and function pointers.
 */
struct CollectionMetadata {
    std::string collection_name;
    std::string type_name;  // For debugging
    
    // Fields that need updating
    std::vector<FieldUpdateDescriptor> fields;
    
    // Type-erased update function
    // Parameters: (void* collection_data, field_name, offset_value, is_range_field)
    // The function knows how to cast void* back to the correct type and apply updates
    std::function<void(void*, const std::string&, float, bool)> apply_float_offset;
    std::function<void(void*, const std::string&, int, bool)> apply_int_offset;
    std::function<void(void*, const std::string&, size_t, bool)> apply_size_t_offset;
    
    CollectionMetadata() = default;
    
    CollectionMetadata(const std::string& name, const std::string& type)
        : collection_name(name), type_name(type) {}
    
    /**
     * Add a field that needs updating
     */
    void addField(const std::string& field_name, FieldUpdateDescriptor::UpdateType type, bool is_range = false) {
        fields.emplace_back(field_name, type, is_range);
    }
    
    /**
     * Check if this collection has a specific field
     */
    bool hasField(const std::string& field_name) const {
        for (const auto& field : fields) {
            if (field.field_name == field_name) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * Get field descriptor
     */
    const FieldUpdateDescriptor* getField(const std::string& field_name) const {
        for (const auto& field : fields) {
            if (field.field_name == field_name) {
                return &field;
            }
        }
        return nullptr;
    }
};

/**
 * Registry of collection metadata - maps collection names to their processing metadata
 */
class CollectionMetadataRegistry {
public:
    /**
     * Register metadata for a collection type
     */
    static void registerMetadata(const std::string& collection_name, CollectionMetadata&& metadata) {
        getRegistry()[collection_name] = std::move(metadata);
    }
    
    /**
     * Get metadata for a collection
     */
    static const CollectionMetadata* getMetadata(const std::string& collection_name) {
        auto& registry = getRegistry();
        auto it = registry.find(collection_name);
        if (it != registry.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    /**
     * Check if a collection is registered
     */
    static bool isRegistered(const std::string& collection_name) {
        return getRegistry().find(collection_name) != getRegistry().end();
    }
    
    /**
     * Get all registered collection names
     */
    static std::vector<std::string> getRegisteredCollections() {
        std::vector<std::string> names;
        for (const auto& [name, _] : getRegistry()) {
            names.push_back(name);
        }
        return names;
    }
    
private:
    static std::map<std::string, CollectionMetadata>& getRegistry() {
        static std::map<std::string, CollectionMetadata> registry;
        return registry;
    }
};
