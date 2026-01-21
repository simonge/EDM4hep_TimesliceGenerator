#include "OutputHandler.h"
#include "EDM4hepOutputHandler.h"
#ifdef HAVE_HEPMC3
#include "HepMC3OutputHandler.h"
#endif
#include <stdexcept>

std::unique_ptr<OutputHandler> OutputHandler::create(const std::string& filename) {
    // Helper lambda to check file extension
    auto hasExtension = [](const std::string& filename, const std::string& ext) {
        if (filename.length() < ext.length()) return false;
        return filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0;
    };
    
#ifdef HAVE_HEPMC3
    // Check if filename ends with .hepmc3.tree.root (more specific first)
    if (hasExtension(filename, ".hepmc3.tree.root")) {
        return std::make_unique<HepMC3OutputHandler>();
    }
#endif
    
    // Check if filename ends with .edm4hep.root
    if (hasExtension(filename, ".edm4hep.root")) {
        return std::make_unique<EDM4hepOutputHandler>();
    }
    
    // Unsupported format
    std::string error_msg = "Unsupported output format: " + filename + "\n"
        "Currently supported formats:\n"
        "  - Files ending with '.edm4hep.root' (e.g., output.edm4hep.root)\n";
#ifdef HAVE_HEPMC3
    error_msg += "  - Files ending with '.hepmc3.tree.root' (e.g., output.hepmc3.tree.root)";
#else
    error_msg += "\nHepMC3 support not available (HepMC3 library not found during build)";
#endif
    throw std::runtime_error(error_msg);
}
