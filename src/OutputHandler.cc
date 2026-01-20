#include "OutputHandler.h"
#include "EDM4hepOutputHandler.h"
#include <stdexcept>

std::unique_ptr<OutputHandler> OutputHandler::create(const std::string& filename) {
    // Get file extension
    size_t last_dot = filename.find_last_of(".");
    std::string extension = (last_dot != std::string::npos) ? filename.substr(last_dot + 1) : "";
    
    // Create appropriate handler based on extension
    if (extension == "root") {
        return std::make_unique<EDM4hepOutputHandler>();
    }
    
    // Unsupported format
    throw std::runtime_error(
        "Unsupported output format: " + filename + "\n"
        "Currently supported formats:\n"
        "  - *.root (EDM4hep format)\n"
        "Planned formats:\n"
        "  - *.hepmc3 (HepMC3 format)"
    );
}
