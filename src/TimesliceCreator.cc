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
    // Set defaults
    // app->SetDefaultParameter("timeslice:duration", config.time_slice_duration, "Default timeslice duration");
    // app->SetDefaultParameter("timeslice:static_number_of_events", config.static_number_of_events, "Use a static number of events per timeslice");
    // app->SetDefaultParameter("timeslice:static_events_per_timeslice", config.static_events_per_timeslice, "Static events per timeslice");
    // app->SetDefaultParameter("timeslice:mean_event_frequency", config.mean_event_frequency, "Mean event frequency");
    // app->SetDefaultParameter("timeslice:bunch_crossing_period", config.bunch_crossing_period, "Bunch crossing period");
    // app->SetDefaultParameter("timeslice:use_bunch_crossing", config.use_bunch_crossing, "Use bunch crossing");
    // app->SetDefaultParameter("timeslice:attach_to_beam", config.attach_to_beam, "Attach to beam");
    // app->SetDefaultParameter("timeslice:beam_speed", config.beam_speed, "Beam speed");
    // app->SetDefaultParameter("timeslice:beam_spread", config.beam_spread, "Beam spread");
    // app->SetDefaultParameter("timeslice:generator_status_offset", config.generator_status_offset, "Generator status offset");

    // Get values from user
    app->GetParameter("timeslice:duration", config.time_slice_duration);
    app->GetParameter("timeslice:bunch_crossing_period", config.bunch_crossing_period);
    app->GetParameter("timeslice:use_bunch_crossing", config.use_bunch_crossing);
    app->GetParameter("timeslice:static_number_of_events", config.static_number_of_events);
    app->GetParameter("timeslice:mean_event_frequency", config.mean_event_frequency);
    app->GetParameter("timeslice:static_events_per_timeslice", config.static_events_per_timeslice);
    app->GetParameter("timeslice:attach_to_beam", config.attach_to_beam);
    app->GetParameter("timeslice:beam_speed", config.beam_speed);
    app->GetParameter("timeslice:beam_spread", config.beam_spread);
    app->GetParameter("timeslice:generator_status_offset", config.generator_status_offset);

    //Check the values of all of the config
     std::cout << "time_slice_duration: " << config.time_slice_duration << std::endl;
    std::cout << "bunch_crossing_period: " << config.bunch_crossing_period << std::endl;
    std::cout << "use_bunch_crossing: " << config.use_bunch_crossing << std::endl;
    std::cout << "static_number_of_events: " << config.static_number_of_events << std::endl;
    std::cout << "mean_event_frequency: " << config.mean_event_frequency << std::endl;
    std::cout << "static_events_per_timeslice: " << config.static_events_per_timeslice << std::endl;
    std::cout << "attach_to_beam: " << config.attach_to_beam << std::endl;
    std::cout << "beam_speed: " << config.beam_speed << std::endl;
    std::cout << "beam_spread: " << config.beam_spread << std::endl;
    std::cout << "generator_status_offset: " << config.generator_status_offset << std::endl;

    app->Add(new MyTimesliceBuilder(config));

    // Event processor that writes and timeslices to file
    app->Add(new MyTimesliceFileWriter());

}
} // "C"


