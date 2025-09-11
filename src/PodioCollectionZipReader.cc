#include "PodioCollectionZipReader.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>

PodioCollectionZipReader::PodioCollectionZipReader(std::shared_ptr<podio::ROOTReader> reader)
    : reader_(std::move(reader)) {
}

PodioCollectionZipReader::PodioCollectionZipReader(const std::vector<std::string>& input_files)
    : reader_(std::make_shared<podio::ROOTReader>()) {
    reader_->openFiles(input_files);
}

size_t PodioCollectionZipReader::getEntries(const std::string& category) const {
    return reader_->getEntries(category);
}

std::vector<std::string> PodioCollectionZipReader::getAvailableCategories() const {
    return reader_->getAvailableCategories();
}

std::unique_ptr<podio::Frame> PodioCollectionZipReader::readEntry(const std::string& category, size_t entry) {
    auto frame_data = reader_->readEntry(category, entry);
    return std::make_unique<podio::Frame>(std::move(frame_data));
}

PodioCollectionZipReader::ZippedCollections PodioCollectionZipReader::zipCollections(
    podio::Frame& frame, const std::vector<std::string>& collection_names) {
    
    ZippedCollections zipped;
    zipped.names = collection_names;
    zipped.min_size = SIZE_MAX;
    
    for (const auto& name : collection_names) {
        try {
            // Get mutable access to the collection using safe const_cast
            auto* collection = const_cast<podio::CollectionBase*>(frame.get(name));
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

void PodioCollectionZipReader::addTimeOffsetVectorized(edm4hep::MCParticleCollection& particles, float time_offset) {
    // Process all particles in the collection efficiently
    for (auto& particle : particles) {
        auto mutable_particle = edm4hep::MutableMCParticle(particle);
        mutable_particle.setTime(particle.getTime() + time_offset);
    }
}

void PodioCollectionZipReader::addTimeOffsetVectorized(edm4hep::SimTrackerHitCollection& hits, float time_offset) {
    // Process all hits in the collection efficiently
    for (auto& hit : hits) {
        auto mutable_hit = edm4hep::MutableSimTrackerHit(hit);
        mutable_hit.setTime(hit.getTime() + time_offset);
    }
}

void PodioCollectionZipReader::addTimeOffsetVectorized(edm4hep::SimCalorimeterHitCollection& hits, float time_offset) {
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

void PodioCollectionZipReader::addTimeOffsetVectorized(edm4hep::CaloHitContributionCollection& contributions, float time_offset) {
    // Process all contributions in the collection efficiently
    for (auto& contrib : contributions) {
        auto mutable_contrib = edm4hep::MutableCaloHitContribution(contrib);
        mutable_contrib.setTime(contrib.getTime() + time_offset);
    }
}

void PodioCollectionZipReader::addTimeOffsetToFrame(podio::Frame& frame, float time_offset, 
                                                   const std::vector<std::string>& collection_names) {
    std::vector<std::string> collections_to_process;
    
    if (collection_names.empty()) {
        // Process all known time-bearing collection types
        auto available_collections = frame.getAvailableCollections();
        for (const auto& name : available_collections) {
            const auto* collection = frame.get(name);
            if (collection) {
                std::string type_name = collection->getValueTypeName();
                if (type_name == "edm4hep::MCParticle" || 
                    type_name == "edm4hep::SimTrackerHit" || 
                    type_name == "edm4hep::SimCalorimeterHit" || 
                    type_name == "edm4hep::CaloHitContribution") {
                    collections_to_process.push_back(name);
                }
            }
        }
    } else {
        collections_to_process = collection_names;
    }
    
    // Apply time offset to each collection based on its type
    for (const auto& name : collections_to_process) {
        try {
            const auto* collection = frame.get(name);
            if (!collection) continue;
            
            std::string type_name = collection->getValueTypeName();
            
            if (type_name == "edm4hep::MCParticle") {
                auto* particles = getMutableCollectionPtr<edm4hep::MCParticleCollection>(frame, name);
                if (particles) addTimeOffsetVectorized(*particles, time_offset);
            } else if (type_name == "edm4hep::SimTrackerHit") {
                auto* hits = getMutableCollectionPtr<edm4hep::SimTrackerHitCollection>(frame, name);
                if (hits) addTimeOffsetVectorized(*hits, time_offset);
            } else if (type_name == "edm4hep::SimCalorimeterHit") {
                auto* hits = getMutableCollectionPtr<edm4hep::SimCalorimeterHitCollection>(frame, name);
                if (hits) addTimeOffsetVectorized(*hits, time_offset);
            } else if (type_name == "edm4hep::CaloHitContribution") {
                auto* contributions = getMutableCollectionPtr<edm4hep::CaloHitContributionCollection>(frame, name);
                if (contributions) addTimeOffsetVectorized(*contributions, time_offset);
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to apply time offset to collection '" << name << "': " << e.what() << std::endl;
        }
    }
}

std::vector<std::string> PodioCollectionZipReader::getCollectionNamesByType(const podio::Frame& frame, const std::string& type_name) {
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