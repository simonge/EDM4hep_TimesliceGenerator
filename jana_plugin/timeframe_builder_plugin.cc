// Copyright 2024, EIC Collaboration
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// TimeframeBuilder JANA2 Plugin
//
// This plugin provides JEventSource implementations that use the TimeframeBuilder
// to merge multiple events from different sources into timeframes. The merged
// timeframes are then provided to JANA2 for further processing by factories
// and processors.
//
// Two event sources are provided:
// - JEventSourceTimeframeBuilderEDM4hep: For EDM4hep/PODIO format files
// - JEventSourceTimeframeBuilderHepMC3: For HepMC3 format files (if compiled with HepMC3)

#include <JANA/JApplication.h>
#include <JANA/JEventSourceGeneratorT.h>

#include "JEventSourceTimeframeBuilderEDM4hep.h"
#include "JEventSourceTimeframeBuilderHepMC3.h"

#ifdef HAVE_HEPMC3
#define HEPMC3_ENABLED true
#else
#define HEPMC3_ENABLED false
#endif

extern "C" {
void InitPlugin(JApplication* app) {
    InitJANAPlugin(app);
    
    // Register EDM4hep event source
    app->Add(new JEventSourceGeneratorT<JEventSourceTimeframeBuilderEDM4hep>());
    
    // Register HepMC3 event source if available
#ifdef HAVE_HEPMC3
    app->Add(new JEventSourceGeneratorT<JEventSourceTimeframeBuilderHepMC3>());
#endif
    
    // Register JANA parameters for timeframe builder configuration
    
    // Timeframe configuration
    app->SetDefaultParameter("tfb:timeframe_duration", 2000.0f,
        "Duration of each timeframe in nanoseconds");
    app->SetDefaultParameter("tfb:bunch_crossing_period", 10.0f,
        "Bunch crossing period in nanoseconds");
    app->SetDefaultParameter("tfb:max_timeframes", 100,
        "Maximum number of timeframes to process");
    app->SetDefaultParameter("tfb:random_seed", 0,
        "Random seed for event merging (0 = use random_device)");
    
    // Source configuration
    app->SetDefaultParameter("tfb:static_events", false,
        "Use static number of events per timeframe");
    app->SetDefaultParameter("tfb:events_per_frame", 1,
        "Static number of events per timeframe (if static_events is true)");
    app->SetDefaultParameter("tfb:event_frequency", 1.0f,
        "Mean event frequency in events/ns (if static_events is false)");
    
    // Bunch crossing and beam physics
    app->SetDefaultParameter("tfb:use_bunch_crossing", false,
        "Enable bunch crossing time discretization");
    app->SetDefaultParameter("tfb:attach_to_beam", false,
        "Enable beam attachment for particles");
    app->SetDefaultParameter("tfb:beam_speed", 299.792458f,
        "Beam speed in ns/mm (speed of light)");
    app->SetDefaultParameter("tfb:beam_spread", 0.0f,
        "Gaussian beam time spread (std deviation) in ns");
    
    // Generator configuration
    app->SetDefaultParameter("tfb:status_offset", 0,
        "Offset added to MCParticle generator status");
    
    // Output configuration (optional for JANA - might want to disable output file)
    app->SetDefaultParameter("tfb:output_file", std::string(""),
        "Output file for merged timeframes (empty = no output file)");
    
    // Print plugin information
    std::cout << "\n";
    std::cout << "==========================================================\n";
    std::cout << "  TimeframeBuilder JANA2 Plugin Loaded\n";
    std::cout << "==========================================================\n";
    std::cout << "  Available Event Sources:\n";
    std::cout << "    - JEventSourceTimeframeBuilderEDM4hep (.edm4hep.root)\n";
#ifdef HAVE_HEPMC3
    std::cout << "    - JEventSourceTimeframeBuilderHepMC3 (.hepmc3.tree.root)\n";
#else
    std::cout << "    - JEventSourceTimeframeBuilderHepMC3 (NOT AVAILABLE - HepMC3 not found)\n";
#endif
    std::cout << "\n";
    std::cout << "  Configuration Parameters (tfb:*):\n";
    std::cout << "    - timeframe_duration: Duration of each timeframe (ns)\n";
    std::cout << "    - bunch_crossing_period: Bunch crossing period (ns)\n";
    std::cout << "    - max_timeframes: Maximum number of timeframes to process\n";
    std::cout << "    - static_events: Use static number of events per timeframe\n";
    std::cout << "    - events_per_frame: Events per timeframe (if static)\n";
    std::cout << "    - event_frequency: Mean event frequency (events/ns)\n";
    std::cout << "    - use_bunch_crossing: Enable bunch crossing discretization\n";
    std::cout << "    - attach_to_beam: Enable beam attachment\n";
    std::cout << "    - output_file: Optional output file for merged timeframes\n";
    std::cout << "==========================================================\n";
    std::cout << "\n";
}
}
