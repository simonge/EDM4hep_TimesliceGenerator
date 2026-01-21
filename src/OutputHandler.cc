#include "OutputHandler.h"
#include "EDM4hepOutputHandler.h"
#ifdef HAVE_HEPMC3
#include "HepMC3OutputHandler.h"
#endif
#include <stdexcept>

std::unique_ptr<OutputHandler> OutputHandler::create(const std::string& filename) {
#ifdef HAVE_HEPMC3
    // Check if filename includes .hepmc3.tree.root
    if (filename.find(".hepmc3.tree.root") != std::string::npos) {
        return std::make_unique<HepMC3OutputHandler>();
    }
#endif
    
    // Check if filename includes .edm4hep.root
    if (filename.find(".edm4hep.root") != std::string::npos) {
        return std::make_unique<EDM4hepOutputHandler>();
    }
    
    // Unsupported format
    std::string error_msg = "Unsupported output format: " + filename + "\n"
        "Currently supported formats:\n"
        "  - Files containing '.edm4hep.root' (e.g., output.edm4hep.root)\n";
#ifdef HAVE_HEPMC3
    error_msg += "  - Files containing '.hepmc3.tree.root' (e.g., output.hepmc3.tree.root)";
#else
    error_msg += "\nHepMC3 support not available (HepMC3 library not found during build)";
#endif
    throw std::runtime_error(error_msg);
}
