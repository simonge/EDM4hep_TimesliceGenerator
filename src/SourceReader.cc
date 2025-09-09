#include "SourceReader.h"
#include <podio/Frame.h>
#include <stdexcept>
#include <algorithm>
#include <iostream>

SourceReader::SourceReader(const SourceConfig& config) : config_(&config) {
    initialize(config);
}

void SourceReader::initialize(const SourceConfig& config) {
    config_ = &config;
    total_entries_ = 0;
    current_entry_index_ = 0;
    entries_needed_ = 1;
    collection_names_to_read_.clear();
    collection_types_to_read_.clear();
}

bool SourceReader::openFiles() {
    if (!config_ || !config_->hasInputFiles()) {
        return false;
    }
    
    try {
        reader_.openFiles(config_->getInputFiles());
        
        auto tree_names = reader_.getAvailableCategories();
        
        // Use treename from config, check it exists
        if (std::find(tree_names.begin(), tree_names.end(), config_->getTreeName()) == tree_names.end()) {
            throw std::runtime_error("ERROR: Tree name '" + config_->getTreeName() + "' not found in file for source " + config_->getName());
        }
        
        total_entries_ = reader_.getEntries(config_->getTreeName());
        return true;
        
    } catch (const std::exception& e) {
        throw std::runtime_error("ERROR: Could not open input files for source " + config_->getName() + ": " + e.what());
    }
}

void SourceReader::validateCollections(const std::vector<std::string>& required_collections) {
    if (!isInitialized()) {
        throw std::runtime_error("SourceReader not initialized");
    }
    
    // Read the first frame to get collection names and types
    auto frame_data = reader_.readEntry(config_->getTreeName(), 0);
    podio::Frame frame(std::move(frame_data));
    auto available_collections = frame.getAvailableCollections();
    
    // Check required collections are present in this source
    for (const auto& coll_name : required_collections) {
        if (std::find(available_collections.begin(), available_collections.end(), coll_name) == available_collections.end()) {
            throw std::runtime_error("ERROR: Collection '" + coll_name + "' not found in source " + config_->getName());
        } else {
            const auto* coll = frame.get(coll_name);
            addCollectionToRead(coll_name, std::string(coll->getValueTypeName()));
        }
    }
    
    // Validate already_merged source has SubEventHeaders collection and regular source has no SubEventHeaders collection
    bool has_sub_event_headers = std::find(available_collections.begin(), available_collections.end(), "SubEventHeaders") != available_collections.end();
    
    if (config_->isAlreadyMerged() && !has_sub_event_headers) {
        throw std::runtime_error("ERROR: Source " + config_->getName() + " is marked as already_merged but has no SubEventHeaders collection.");
    } else if (!config_->isAlreadyMerged() && has_sub_event_headers) {
        throw std::runtime_error("ERROR: Source " + config_->getName() + " is marked as not already_merged but has SubEventHeaders collection.");
    } else if (has_sub_event_headers) {
        addCollectionToRead("SubEventHeaders", "edm4hep::EventHeader");
    }
}

void SourceReader::addCollectionToRead(const std::string& name, const std::string& type) {
    collection_names_to_read_.push_back(name);
    collection_types_to_read_.push_back(type);
}