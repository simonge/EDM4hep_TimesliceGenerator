#pragma once

#include <string>
#include <map>
#include <vector>
#include <any>
#include <functional>
#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <edm4hep/EventHeaderData.h>
#include <podio/ObjectID.h>

// Forward declaration
struct MergedCollections;

/**
 * Descriptor for how to handle a collection.
 * Maps collection names to their types and destination containers.
 */
struct CollectionDescriptor {
    std::string type_name;  // e.g., "MCParticles", "ObjectID", "SimTrackerHit"
    
    // Function to cast from std::any and merge into destination
    std::function<void(std::any&, MergedCollections&, const std::string&)> merge_function;
    
    // Function to get the current size of the destination (for offset calculations)
    std::function<size_t(const MergedCollections&, const std::string&)> get_size_function;
};

/**
 * Registry mapping collection names to their descriptors.
 * Eliminates hardcoded checks like "if (collection_name == "GPIntValues")".
 */
class CollectionRegistry {
public:
    static void initialize(MergedCollections& merged);
    static const CollectionDescriptor* getDescriptor(const std::string& collection_name);
    static void registerDescriptor(const std::string& collection_name, CollectionDescriptor&& desc);
    
private:
    static std::map<std::string, CollectionDescriptor>& getRegistry();
};
