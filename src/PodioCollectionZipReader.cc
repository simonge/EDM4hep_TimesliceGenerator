#include "PodioCollectionZipReader.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>

#ifdef PODIO_AVAILABLE

// PodioMutableCollectionReader implementation
PodioMutableCollectionReader::PodioMutableCollectionReader(const std::vector<std::string>& input_files)
    : reader_(std::make_shared<podio::ROOTReader>()) {
    reader_->openFiles(input_files);
}

size_t PodioMutableCollectionReader::getEntries(const std::string& category) const {
    return reader_->getEntries(category);
}

std::vector<std::string> PodioMutableCollectionReader::getAvailableCategories() const {
    return reader_->getAvailableCategories();
}

std::unique_ptr<podio::Frame> PodioMutableCollectionReader::readMutableEntry(const std::string& category, size_t entry) {
    // Read the const frame from podio::ROOTReader
    auto const_frame_data = reader_->readEntry(category, entry);
    auto const_frame = podio::Frame(std::move(const_frame_data));
    
    // Create a new mutable frame by copying all collections
    return createMutableFrame(const_frame);
}

std::unique_ptr<podio::Frame> PodioMutableCollectionReader::createMutableFrame(const podio::Frame& const_frame) {
    auto mutable_frame = std::make_unique<podio::Frame>();
    
    // Get all available collections in the const frame
    auto collection_names = const_frame.getAvailableCollections();
    
    for (const auto& name : collection_names) {
        try {
            const auto* collection = const_frame.get(name);
            if (!collection) continue;
            
            std::string type_name = collection->getValueTypeName();
            
            // Create mutable copies based on collection type
            if (type_name == "edm4hep::MCParticle") {
                const auto& source = const_frame.get<edm4hep::MCParticleCollection>(name);
                auto mutable_copy = cloneMCParticleCollection(source);
                mutable_frame->put(std::move(*mutable_copy), name);
            } else if (type_name == "edm4hep::SimTrackerHit") {
                const auto& source = const_frame.get<edm4hep::SimTrackerHitCollection>(name);
                auto mutable_copy = cloneSimTrackerHitCollection(source);
                mutable_frame->put(std::move(*mutable_copy), name);
            } else if (type_name == "edm4hep::SimCalorimeterHit") {
                const auto& source = const_frame.get<edm4hep::SimCalorimeterHitCollection>(name);
                auto mutable_copy = cloneSimCalorimeterHitCollection(source);
                mutable_frame->put(std::move(*mutable_copy), name);
            } else if (type_name == "edm4hep::CaloHitContribution") {
                const auto& source = const_frame.get<edm4hep::CaloHitContributionCollection>(name);
                auto mutable_copy = cloneCaloHitContributionCollection(source);
                mutable_frame->put(std::move(*mutable_copy), name);
            } else if (type_name == "edm4hep::EventHeader") {
                const auto& source = const_frame.get<edm4hep::EventHeaderCollection>(name);
                auto mutable_copy = cloneEventHeaderCollection(source);
                mutable_frame->put(std::move(*mutable_copy), name);
            }
            // Add more collection types as needed
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to clone collection '" << name << "': " << e.what() << std::endl;
        }
    }
    
    return mutable_frame;
}

// Collection cloning methods - create true mutable copies
std::unique_ptr<edm4hep::MCParticleCollection> PodioMutableCollectionReader::cloneMCParticleCollection(const edm4hep::MCParticleCollection& source) {
    auto clone = std::make_unique<edm4hep::MCParticleCollection>();
    
    // Clone each particle in the collection
    for (const auto& particle : source) {
        auto mutable_particle = edm4hep::MutableMCParticle();
        mutable_particle.setPDG(particle.getPDG());
        mutable_particle.setGeneratorStatus(particle.getGeneratorStatus());
        mutable_particle.setSimulatorStatus(particle.getSimulatorStatus());
        mutable_particle.setCharge(particle.getCharge());
        mutable_particle.setTime(particle.getTime());
        mutable_particle.setMass(particle.getMass());
        mutable_particle.setVertex(particle.getVertex());
        mutable_particle.setEndpoint(particle.getEndpoint());
        mutable_particle.setMomentum(particle.getMomentum());
        mutable_particle.setMomentumAtEndpoint(particle.getMomentumAtEndpoint());
        mutable_particle.setSpin(particle.getSpin());
        mutable_particle.setColorFlow(particle.getColorFlow());
        
        clone->push_back(mutable_particle);
    }
    
    return clone;
}

std::unique_ptr<edm4hep::SimTrackerHitCollection> PodioMutableCollectionReader::cloneSimTrackerHitCollection(const edm4hep::SimTrackerHitCollection& source) {
    auto clone = std::make_unique<edm4hep::SimTrackerHitCollection>();
    
    // Clone each hit in the collection
    for (const auto& hit : source) {
        auto mutable_hit = edm4hep::MutableSimTrackerHit();
        mutable_hit.setCellID(hit.getCellID());
        mutable_hit.setEDep(hit.getEDep());
        mutable_hit.setTime(hit.getTime());
        mutable_hit.setPathLength(hit.getPathLength());
        mutable_hit.setQuality(hit.getQuality());
        mutable_hit.setPosition(hit.getPosition());
        mutable_hit.setMomentum(hit.getMomentum());
        
        clone->push_back(mutable_hit);
    }
    
    return clone;
}

std::unique_ptr<edm4hep::SimCalorimeterHitCollection> PodioMutableCollectionReader::cloneSimCalorimeterHitCollection(const edm4hep::SimCalorimeterHitCollection& source) {
    auto clone = std::make_unique<edm4hep::SimCalorimeterHitCollection>();
    
    // Clone each hit in the collection
    for (const auto& hit : source) {
        auto mutable_hit = edm4hep::MutableSimCalorimeterHit();
        mutable_hit.setCellID(hit.getCellID());
        mutable_hit.setEnergy(hit.getEnergy());
        mutable_hit.setPosition(hit.getPosition());
        
        // Clone contributions
        for (const auto& contrib : hit.getContributions()) {
            auto mutable_contrib = edm4hep::MutableCaloHitContribution();
            mutable_contrib.setPDG(contrib.getPDG());
            mutable_contrib.setEnergy(contrib.getEnergy());
            mutable_contrib.setTime(contrib.getTime());
            mutable_contrib.setStepPosition(contrib.getStepPosition());
            mutable_hit.addToContributions(mutable_contrib);
        }
        
        clone->push_back(mutable_hit);
    }
    
    return clone;
}

std::unique_ptr<edm4hep::CaloHitContributionCollection> PodioMutableCollectionReader::cloneCaloHitContributionCollection(const edm4hep::CaloHitContributionCollection& source) {
    auto clone = std::make_unique<edm4hep::CaloHitContributionCollection>();
    
    // Clone each contribution in the collection
    for (const auto& contrib : source) {
        auto mutable_contrib = edm4hep::MutableCaloHitContribution();
        mutable_contrib.setPDG(contrib.getPDG());
        mutable_contrib.setEnergy(contrib.getEnergy());
        mutable_contrib.setTime(contrib.getTime());
        mutable_contrib.setStepPosition(contrib.getStepPosition());
        
        clone->push_back(mutable_contrib);
    }
    
    return clone;
}

std::unique_ptr<edm4hep::EventHeaderCollection> PodioMutableCollectionReader::cloneEventHeaderCollection(const edm4hep::EventHeaderCollection& source) {
    auto clone = std::make_unique<edm4hep::EventHeaderCollection>();
    
    // Clone each header in the collection
    for (const auto& header : source) {
        auto mutable_header = edm4hep::MutableEventHeader();
        mutable_header.setEventNumber(header.getEventNumber());
        mutable_header.setRunNumber(header.getRunNumber());
        mutable_header.setTimeStamp(header.getTimeStamp());
        mutable_header.setWeight(header.getWeight());
        
        clone->push_back(mutable_header);
    }
    
    return clone;
}

PodioMutableCollectionReader::ZippedCollections PodioMutableCollectionReader::zipCollections(
    podio::Frame& frame, const std::vector<std::string>& collection_names) {
    
    ZippedCollections zipped;
    zipped.names = collection_names;
    zipped.min_size = SIZE_MAX;
    
    for (const auto& name : collection_names) {
        try {
            // Since our frame contains truly mutable collections, we can access them directly
            auto* collection = frame.get(name);
            if (collection) {
                zipped.collections.push_back(collection);
                zipped.min_size = std::min(zipped.min_size, collection->size());
            } else {
                throw std::runtime_error("Collection not found: " + name);
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to access collection '" + name + "': " + e.what());
        }
    }
    
    if (zipped.min_size == SIZE_MAX) {
        zipped.min_size = 0;
    }
    
    return zipped;
}

void PodioMutableCollectionReader::addTimeOffsetVectorized(edm4hep::MCParticleCollection& particles, float time_offset) {
    // Process all particles in the collection efficiently
    for (auto& particle : particles) {
        auto mutable_particle = edm4hep::MutableMCParticle(particle);
        mutable_particle.setTime(particle.getTime() + time_offset);
    }
}

void PodioMutableCollectionReader::addTimeOffsetVectorized(edm4hep::SimTrackerHitCollection& hits, float time_offset) {
    // Process all hits in the collection efficiently
    for (auto& hit : hits) {
        auto mutable_hit = edm4hep::MutableSimTrackerHit(hit);
        mutable_hit.setTime(hit.getTime() + time_offset);
    }
}

void PodioMutableCollectionReader::addTimeOffsetVectorized(edm4hep::SimCalorimeterHitCollection& hits, float time_offset) {
    // Process all calorimeter hits in the collection efficiently
    for (auto& hit : hits) {
        auto mutable_hit = edm4hep::MutableSimCalorimeterHit(hit);
        // For calorimeter hits, we need to update contributions as well
        auto contributions = hit.getContributions();
        for (auto& contrib : contributions) {
            auto mutable_contrib = edm4hep::MutableCaloHitContribution(contrib);
            mutable_contrib.setTime(contrib.getTime() + time_offset);
        }
    }
}

void PodioMutableCollectionReader::addTimeOffsetVectorized(edm4hep::CaloHitContributionCollection& contributions, float time_offset) {
    // Process all contributions in the collection efficiently
    for (auto& contrib : contributions) {
        auto mutable_contrib = edm4hep::MutableCaloHitContribution(contrib);
        mutable_contrib.setTime(contrib.getTime() + time_offset);
    }
}

void PodioMutableCollectionReader::addTimeOffsetToFrame(podio::Frame& frame, float time_offset, 
                                                   const std::vector<std::string>& collection_names) {
    std::vector<std::string> collections_to_process;
    
    if (collection_names.empty()) {
        // Process all known time-bearing collection types
        auto available_collections = frame.getAvailableCollections();
        for (const auto& name : available_collections) {
            const auto* collection = frame.get(name);
            if (collection) {
                std::string type_name = collection->getValueTypeName();
                // Check if this is a time-bearing collection type
                if (getTypeProcessors().count(type_name) > 0) {
                    collections_to_process.push_back(name);
                }
            }
        }
    } else {
        collections_to_process = collection_names;
    }
    
    // Apply time offset to each collection using the processor map
    const auto& processors = getTypeProcessors();
    for (const auto& name : collections_to_process) {
        try {
            const auto* collection = frame.get(name);
            if (!collection) continue;
            
            std::string type_name = collection->getValueTypeName();
            auto it = processors.find(type_name);
            if (it != processors.end()) {
                it->second(frame, name, time_offset);
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to apply time offset to collection '" << name << "': " << e.what() << std::endl;
        }
    }
}

const std::unordered_map<std::string, std::function<void(podio::Frame&, const std::string&, float)>>& 
PodioMutableCollectionReader::getTypeProcessors() {
    static const std::unordered_map<std::string, std::function<void(podio::Frame&, const std::string&, float)>> processors = {
        {"edm4hep::MCParticle", [](podio::Frame& frame, const std::string& name, float time_offset) {
            auto* particles = PodioMutableCollectionReader::getMutableCollectionPtr<edm4hep::MCParticleCollection>(frame, name);
            if (particles) addTimeOffsetVectorized(*particles, time_offset);
        }},
        {"edm4hep::SimTrackerHit", [](podio::Frame& frame, const std::string& name, float time_offset) {
            auto* hits = PodioMutableCollectionReader::getMutableCollectionPtr<edm4hep::SimTrackerHitCollection>(frame, name);
            if (hits) addTimeOffsetVectorized(*hits, time_offset);
        }},
        {"edm4hep::SimCalorimeterHit", [](podio::Frame& frame, const std::string& name, float time_offset) {
            auto* hits = PodioMutableCollectionReader::getMutableCollectionPtr<edm4hep::SimCalorimeterHitCollection>(frame, name);
            if (hits) addTimeOffsetVectorized(*hits, time_offset);
        }},
        {"edm4hep::CaloHitContribution", [](podio::Frame& frame, const std::string& name, float time_offset) {
            auto* contributions = PodioMutableCollectionReader::getMutableCollectionPtr<edm4hep::CaloHitContributionCollection>(frame, name);
            if (contributions) addTimeOffsetVectorized(*contributions, time_offset);
        }}
    };
    return processors;
}

std::vector<std::string> PodioMutableCollectionReader::getCollectionNamesByType(const podio::Frame& frame, const std::string& type_name) {
    std::vector<std::string> matching_collections;
    auto available_collections = frame.getAvailableCollections();
    
    for (const auto& name : available_collections) {
        const auto* collection = frame.get(name);
        if (collection && collection->getValueTypeName() == type_name) {
            matching_collections.push_back(name);
        }
    }
    
    return matching_collections;
}

// Template specializations for createMutableCopy
template<>
std::unique_ptr<edm4hep::MCParticleCollection> PodioMutableCollectionReader::createMutableCopy(const podio::Frame& frame, const std::string& collection_name) {
    const auto& source = frame.get<edm4hep::MCParticleCollection>(collection_name);
    return cloneMCParticleCollection(source);
}

template<>
std::unique_ptr<edm4hep::SimTrackerHitCollection> PodioMutableCollectionReader::createMutableCopy(const podio::Frame& frame, const std::string& collection_name) {
    const auto& source = frame.get<edm4hep::SimTrackerHitCollection>(collection_name);
    return cloneSimTrackerHitCollection(source);
}

template<>
std::unique_ptr<edm4hep::SimCalorimeterHitCollection> PodioMutableCollectionReader::createMutableCopy(const podio::Frame& frame, const std::string& collection_name) {
    const auto& source = frame.get<edm4hep::SimCalorimeterHitCollection>(collection_name);
    return cloneSimCalorimeterHitCollection(source);
}

template<>
std::unique_ptr<edm4hep::CaloHitContributionCollection> PodioMutableCollectionReader::createMutableCopy(const podio::Frame& frame, const std::string& collection_name) {
    const auto& source = frame.get<edm4hep::CaloHitContributionCollection>(collection_name);
    return cloneCaloHitContributionCollection(source);
}

template<>
std::unique_ptr<edm4hep::EventHeaderCollection> PodioMutableCollectionReader::createMutableCopy(const podio::Frame& frame, const std::string& collection_name) {
    const auto& source = frame.get<edm4hep::EventHeaderCollection>(collection_name);
    return cloneEventHeaderCollection(source);
}

#endif // PODIO_AVAILABLE