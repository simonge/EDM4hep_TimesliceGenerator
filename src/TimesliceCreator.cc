// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.


#include "MyEventFileReaderGenerator.h"
#include "MyTimesliceFileWriter.h"
#include "MyTimesliceBuilder.h"
#include "MyTimesliceBuilderConfig.h"

#include <JANA/Components/JOmniFactoryGeneratorT.h>

#include <JANA/JApplication.h>


extern "C"{
void InitPlugin(JApplication *app) {

    InitJANAPlugin(app);
    size_t default_nevents = 100;
    app->SetDefaultParameter("writer:nevents", default_nevents, "Default number of events to write");

    bool default_write_event_frame = false;
    app->SetDefaultParameter("writer:write_event_frame", default_write_event_frame, "Write parent event frame");
    // Event source generator instantiates a FileReader for each filename passed to jana.
    // The event source it produces is configured to either produce Timeslices or Events.
    // Either way, these files contain just hits
    app->Add(new MyEventFileReaderGenerator());

    // MyTimesliceBuilderConfig config;
    // config.tag = "det1";
    // config.parent_level = JEventLevel::PhysicsEvent;
    // config.time_slice_duration = 100.0f; // ns
    // config.mean_hit_frequency = 0.1f; // Hz
    // config.bunch_crossing_period = 10.0f; // ns
    // config.use_bunch_crossing = true;
    // // Unfolder that takes timeslices and splits them into physics events.
    // app->Add(new MyTimesliceBuilderEDM4HEP(config));

    MyTimesliceBuilderConfig config2;
    config2.tag = "det2";
    config2.parent_level = JEventLevel::PhysicsEvent;
    config2.time_slice_duration = 100.0f; // ns
    config2.mean_hit_frequency = 0.1f; // Hz
    config2.bunch_crossing_period = 10.0f; // ns
    config2.use_bunch_crossing = true;    
    app->Add(new MyTimesliceBuilder(config2));

    // Collection Collector for the output...
    //  app->Add(new JOmniFactoryGeneratorT<SimTrackerHitCollector_factory>(
    //   {.tag                   = "ts_hits",
    //     .level = JEventLevel::Timeslice,
    //    .variadic_input_names  = {{"det1ts_hits","det2ts_hits"}},
    //    .output_names = {"ts_hits"},
    //   }
    // ));



    // Event processor that writes and timeslices to file
    app->Add(new MyTimesliceFileWriter());

}
} // "C"


