#include "MutableRootReader.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>

#ifdef PODIO_AVAILABLE

MutableRootReader::MutableRootReader(const std::vector<std::string>& input_files)
    : reader_(std::make_shared<podio::ROOTReader>()) {
    
    // TODO: This temporarily uses podio::ROOTReader
    // Replace with direct ROOT file access once podio format is understood
    reader_->openFiles(input_files);
    
    std::cout << "MutableRootReader: Temporarily using podio::ROOTReader wrapper" << std::endl;
    std::cout << "TODO: Implement direct ROOT file reading to eliminate const restrictions" << std::endl;
}

size_t MutableRootReader::getEntries(const std::string& category) const {
    return reader_->getEntries(category);
}

std::vector<std::string> MutableRootReader::getAvailableCategories() const {
    return reader_->getAvailableCategories();
}

std::unique_ptr<MutableRootReader::MutableFrame> MutableRootReader::readMutableEntry(const std::string& category, size_t entry) {
    // TODO: Replace with direct ROOT reading
    auto frame_data = reader_->readEntry(category, entry);
    auto podio_frame = std::make_unique<podio::Frame>(std::move(frame_data));
    
    return std::make_unique<MutableFrame>(std::move(podio_frame));
}

#endif // PODIO_AVAILABLE