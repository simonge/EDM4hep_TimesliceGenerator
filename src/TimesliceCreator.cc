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

    // Add default parameters for all of the configs

    MyTimesliceBuilderConfig config;
    app->SetDefaultParameter("timeslice:duration", config.time_slice_duration, "Default timeslice duration");
    app->SetDefaultParameter("timeslice:bunch_crossing_period", config.bunch_crossing_period, "Default bunch crossing period");

    config.static_number_of_hits = true;
    config.mean_hit_frequency = 0.1f; // Hz
    config.use_bunch_crossing = true;
    app->Add(new MyTimesliceBuilder(config));

    // Event processor that writes and timeslices to file
    app->Add(new MyTimesliceFileWriter());

}
} // "C"


