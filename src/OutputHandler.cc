#include "OutputHandler.h"
#include "EDM4hepOutputHandler.h"
#include <stdexcept>

std::unique_ptr<OutputHandler> OutputHandler::create(const std::string& filename) {
    // Check if filename includes .edm4hep.root
    if (filename.find(".edm4hep.root") != std::string::npos) {
        return std::make_unique<EDM4hepOutputHandler>();
    }
    
    // Unsupported format
    throw std::runtime_error(
        "Unsupported output format: " + filename + "\n"
        "Currently supported formats:\n"
        "  - Files containing '.edm4hep.root' (e.g., output.edm4hep.root)\n"
        "Planned formats:\n"
        "  - *.hepmc3 (HepMC3 format)"
    );
}
