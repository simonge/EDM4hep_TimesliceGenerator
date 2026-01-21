#include "HepMC3DataSource.h"
#include <iostream>
#include <stdexcept>

HepMC3DataSource::HepMC3DataSource(const SourceConfig& config, size_t source_index) {
    config_ = &config;
    source_index_ = source_index;
    total_entries_ = 0;
    current_entry_index_ = 0;
    entries_needed_ = 0;
    openInputFiles();
}

HepMC3DataSource::~HepMC3DataSource() {
    cleanup();
}

void HepMC3DataSource::initialize(const std::vector<std::string>& tracker_collections,
                                  const std::vector<std::string>& calo_collections,
                                  const std::vector<std::string>& gp_collections) {
    // HepMC3 doesn't use collection names like EDM4hep
    // Just verify the reader is ready
    if (!reader_) {
        throw std::runtime_error("HepMC3 reader not initialized");
    }
}

void HepMC3DataSource::openInputFiles() {
    if (config_->input_files.empty()) {
        throw std::runtime_error("No input files specified for source: " + config_->name);
    }
    
    // For now, support only single file (could be extended to support multiple files)
    if (config_->input_files.size() > 1) {
        std::cout << "Warning: HepMC3DataSource currently supports only the first input file. "
                  << "Using: " << config_->input_files[0] << std::endl;
    }
    
    const std::string& input_file = config_->input_files[0];
    
    // Validate file extension
    if (input_file.find(".hepmc3.tree.root") == std::string::npos) {
        throw std::runtime_error(
            "HepMC3DataSource only supports .hepmc3.tree.root format. Got: " + input_file
        );
    }
    
    std::cout << "Opening HepMC3 file: " << input_file << std::endl;
    
    // Use HepMC3 factory to create reader
    reader_ = HepMC3::deduce_reader(input_file);
    if (!reader_) {
        throw std::runtime_error("Failed to open HepMC3 file: " + input_file);
    }
    
    // Count total entries by reading through the file
    // Note: HepMC3 doesn't provide a direct way to get event count
    // We need to read through the file once
    std::cout << "Counting events in HepMC3 file..." << std::endl;
    total_entries_ = 0;
    HepMC3::GenEvent temp_event;
    while (!reader_->failed()) {
        if (!reader_->read_event(temp_event)) {
            break;
        }
        total_entries_++;
    }
    
    std::cout << "Found " << total_entries_ << " events in HepMC3 file" << std::endl;
    
    // Close and reopen to reset position
    reader_->close();
    reader_ = HepMC3::deduce_reader(input_file);
    if (!reader_) {
        throw std::runtime_error("Failed to reopen HepMC3 file: " + input_file);
    }
    
    current_entry_index_ = 0;
}

bool HepMC3DataSource::hasMoreEntries() const {
    return current_entry_index_ + entries_needed_ <= total_entries_;
}

bool HepMC3DataSource::loadNextEvent() {
    if (current_entry_index_ >= total_entries_) {
        return false;
    }
    
    if (reader_->failed()) {
        return false;
    }
    
    // Read the next event
    if (!reader_->read_event(current_event_)) {
        return false;
    }
    
    current_entry_index_++;
    return true;
}

void HepMC3DataSource::loadEvent(size_t event_index) {
    // HepMC3 readers are sequential, we can't jump to arbitrary positions
    // This would require reopening and skipping events
    // For now, just ensure we're reading sequentially
    if (event_index != current_entry_index_) {
        std::cerr << "Warning: HepMC3DataSource only supports sequential reading. "
                  << "Requested event " << event_index << " but at " << current_entry_index_ << std::endl;
    }
    loadNextEvent();
}

void HepMC3DataSource::cleanup() {
    if (reader_) {
        reader_->close();
        reader_.reset();
    }
}

void HepMC3DataSource::printStatus() const {
    std::cout << "HepMC3DataSource Status:" << std::endl;
    std::cout << "  Name: " << config_->name << std::endl;
    std::cout << "  Source Index: " << source_index_ << std::endl;
    std::cout << "  Total Entries: " << total_entries_ << std::endl;
    std::cout << "  Current Entry: " << current_entry_index_ << std::endl;
    std::cout << "  Entries Needed: " << entries_needed_ << std::endl;
    std::cout << "  Current Time Offset: " << current_time_offset_ << std::endl;
}

DataSource::VertexPosition HepMC3DataSource::getBeamVertexPosition() const {
    VertexPosition pos{0.0f, 0.0f, 0.0f};
    
    // Get the first vertex in the event (production vertex)
    const auto& vertices = current_event_.vertices();
    if (!vertices.empty() && vertices[0]) {
        const auto& vertex_pos = vertices[0]->position();
        pos.x = static_cast<float>(vertex_pos.x());
        pos.y = static_cast<float>(vertex_pos.y());
        pos.z = static_cast<float>(vertex_pos.z());
    }
    
    return pos;
}
