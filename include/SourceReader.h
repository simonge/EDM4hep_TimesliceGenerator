#pragma once

#include "StandaloneMergerConfig.h"
#include <podio/ROOTReader.h>
#include <vector>
#include <string>

class SourceReader {
public:
    // Constructor
    SourceReader() = default;
    explicit SourceReader(const SourceConfig& config);
    
    // Initialization methods
    void initialize(const SourceConfig& config);
    bool openFiles();
    void validateCollections(const std::vector<std::string>& required_collections);
    
    // File reading methods
    size_t getTotalEntries() const { return total_entries_; }
    size_t getCurrentEntryIndex() const { return current_entry_index_; }
    size_t getEntriesNeeded() const { return entries_needed_; }
    void setEntriesNeeded(size_t entries) { entries_needed_ = entries; }
    void advanceEntry() { current_entry_index_++; }
    
    // Collection access methods
    const std::vector<std::string>& getCollectionNamesToRead() const { return collection_names_to_read_; }
    const std::vector<std::string>& getCollectionTypesToRead() const { return collection_types_to_read_; }
    void addCollectionToRead(const std::string& name, const std::string& type);
    
    // Configuration access
    const SourceConfig& getConfig() const { return *config_; }
    
    // Reader access
    podio::ROOTReader& getReader() { return reader_; }
    const podio::ROOTReader& getReader() const { return reader_; }
    
    // Validation methods
    bool isInitialized() const { return config_ != nullptr && total_entries_ > 0; }
    bool hasMoreEntries() const { return current_entry_index_ < total_entries_; }
    bool canReadRequiredEntries() const { 
        return (current_entry_index_ + entries_needed_) <= total_entries_; 
    }

private:
    podio::ROOTReader reader_;
    size_t total_entries_{0};
    size_t current_entry_index_{0};
    size_t entries_needed_{1};
    std::vector<std::string> collection_names_to_read_;
    std::vector<std::string> collection_types_to_read_;
    const SourceConfig* config_{nullptr};
};