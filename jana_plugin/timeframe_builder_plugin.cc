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
    // These match the standalone tool configuration options
    
    // Global timeframe configuration
    app->SetDefaultParameter("tfb:timeframe_duration", 2000.0f,
        "Duration of each timeframe in nanoseconds");
    app->SetDefaultParameter("tfb:bunch_crossing_period", 10.0f,
        "Bunch crossing period in nanoseconds");
    app->SetDefaultParameter("tfb:max_timeframes", 100,
        "Maximum number of timeframes to process");
    app->SetDefaultParameter("tfb:random_seed", 0u,
        "Random seed for event merging (0 = use random_device)");
    app->SetDefaultParameter("tfb:introduce_offsets", true,
        "Introduce random time offsets for events");
    app->SetDefaultParameter("tfb:merge_particles", false,
        "Merge particles (advanced feature)");
    
    // Output configuration
    app->SetDefaultParameter("tfb:output_file", std::string(""),
        "Output file for merged timeframes (empty = no output file)");
    
    // Multi-source support - comma-separated list of source names
    app->SetDefaultParameter("tfb:source_names", std::string(""),
        "Comma-separated list of source names (empty = use input file as single source)");
    
    // Default source configuration (for backward compatibility when no sources specified)
    app->SetDefaultParameter("tfb:static_events", false,
        "Use static number of events per timeframe (default source)");
    app->SetDefaultParameter("tfb:events_per_frame", 1,
        "Static events per timeframe (default source)");
    app->SetDefaultParameter("tfb:event_frequency", 1.0f,
        "Mean event frequency in events/ns (default source)");
    app->SetDefaultParameter("tfb:use_bunch_crossing", false,
        "Enable bunch crossing discretization (default source)");
    app->SetDefaultParameter("tfb:attach_to_beam", false,
        "Enable beam attachment (default source)");
    app->SetDefaultParameter("tfb:beam_angle", 0.0f,
        "Beam angle in radians (default source)");
    app->SetDefaultParameter("tfb:beam_speed", 299.792458f,
        "Beam speed in mm/ns (default source)");
    app->SetDefaultParameter("tfb:beam_spread", 0.0f,
        "Gaussian beam time spread in ns (default source)");
    app->SetDefaultParameter("tfb:status_offset", 0,
        "Generator status offset (default source)");
    app->SetDefaultParameter("tfb:already_merged", false,
        "Input is already merged timeframes (default source)");
    app->SetDefaultParameter("tfb:tree_name", std::string("events"),
        "TTree name in input file (default source)");
    app->SetDefaultParameter("tfb:repeat_on_eof", false,
        "Repeat source when EOF reached (default source)");
    
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
    std::cout << "    - JEventSourceTimeframeBuilderHepMC3 (NOT AVAILABLE)\n";
#endif
    std::cout << "\n";
    std::cout << "  Configuration parameters match standalone tool\n";
    std::cout << "  Use -Ptfb:parameter=value to configure\n";
    std::cout << "  Run 'jana -Pprint-default-parameters' to see all options\n";
    std::cout << "==========================================================\n";
    std::cout << "\n";
}
}
