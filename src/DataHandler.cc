#include "DataHandler.h"
#include "EDM4hepDataHandler.h"
#ifdef HAVE_HEPMC3
#include "HepMC3DataHandler.h"
#endif
#include <stdexcept>

void DataHandler::mergeEvents(std::vector<std::unique_ptr<DataSource>>& sources,
                              size_t timeslice_number,
                              float time_slice_duration,
                              float bunch_crossing_period,
                              std::mt19937& gen) {
    current_timeslice_number_ = timeslice_number;
    
    // Iterate over all sources
    for (auto& source : sources) {
        const auto& config = source->getConfig();
        size_t entries_needed = source->getEntriesNeeded();
        int events_consumed = 0;
        
        // Process each event from this source
        for (size_t entry = 0; entry < entries_needed; ++entry) {
            // Load and prepare the event
            source->loadEvent(source->getCurrentEntryIndex());
            source->UpdateTimeOffset(time_slice_duration, bunch_crossing_period, gen);
            
            // Call format-specific processing
            processEvent(*source);
            
            source->setCurrentEntryIndex(source->getCurrentEntryIndex() + 1);
            events_consumed++;
        }
        
        std::cout << "Processed " << events_consumed << " events from source " 
                  << config.name << std::endl;
    }
}

std::unique_ptr<DataHandler> DataHandler::create(const std::string& filename) {
    // Helper lambda to check file extension
    auto hasExtension = [](const std::string& filename, const std::string& ext) {
        if (filename.length() < ext.length()) return false;
        return filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0;
    };
    
#ifdef HAVE_HEPMC3
    // Check if filename ends with .hepmc3.tree.root (more specific first)
    if (hasExtension(filename, ".hepmc3.tree.root")) {
        return std::make_unique<HepMC3DataHandler>();
    }
#endif
    
    // Check if filename ends with .edm4hep.root
    if (hasExtension(filename, ".edm4hep.root")) {
        return std::make_unique<EDM4hepDataHandler>();
    }
    
    // Unsupported format
    std::string error_msg = "Unsupported data format: " + filename + "\n"
        "Currently supported formats:\n"
        "  - Files ending with '.edm4hep.root' (e.g., output.edm4hep.root)\n";
#ifdef HAVE_HEPMC3
    error_msg += "  - Files ending with '.hepmc3.tree.root' (e.g., output.hepmc3.tree.root)";
#else
    error_msg += "\nHepMC3 support not available (HepMC3 library not found during build)";
#endif
    throw std::runtime_error(error_msg);
}
