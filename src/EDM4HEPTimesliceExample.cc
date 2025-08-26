// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.


#include "MyFileReaderGeneratorEDM4HEP.h"
#include "MyFileWriterEDM4HEP.h"
#include "MyTimesliceBuilderEDM4HEP.h"
#include "MyTimesliceBuilderConfig.h"
#include "SimTrackerHitCollector_factory.h"

#include <JANA/Components/JOmniFactoryGeneratorT.h>

#include <JANA/JApplication.h>


extern "C"{
void InitPlugin(JApplication *app) {

    InitJANAPlugin(app);

    // Event source generator instantiates a FileReader for each filename passed to jana.
    // The event source it produces is configured to either produce Timeslices or Events.
    // Either way, these files contain just hits
    app->Add(new MyFileReaderGeneratorEDM4HEP());

    MyTimesliceBuilderConfig config;
    config.tag = "det1";
    config.parent_level = JEventLevel::PhysicsEvent;
    // Unfolder that takes timeslices and splits them into physics events.
    app->Add(new MyTimesliceBuilderEDM4HEP(config));

    MyTimesliceBuilderConfig config2;
    config2.tag = "det2";
    config2.parent_level = JEventLevel::Subevent;
    app->Add(new MyTimesliceBuilderEDM4HEP(config2));

    // Collection Collector for the output...
     app->Add(new JOmniFactoryGeneratorT<SimTrackerHitCollector_factory>(
      {.tag                   = "ts_hits",
       .variadic_input_names  = {{"det1ts_hits"}},
       .output_names = {"ts_hits"},
      }
    ));



    // Event processor that writes and timeslices to file
    app->Add(new MyFileWriterEDM4HEP());

}
} // "C"


