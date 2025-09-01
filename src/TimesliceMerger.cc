// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.


#include "MyFileReaderGeneratorEDM4HEP.h"
#include "MyFileWriterEDM4HEP.h"

#include <JANA/Components/JOmniFactoryGeneratorT.h>

#include <JANA/JApplication.h>


extern "C"{
void InitPlugin(JApplication *app) {

    InitJANAPlugin(app);
    size_t default_nevents = 100;
    app->SetDefaultParameter("writer:nevents", default_nevents, "Default number of events to write");

    bool default_write_event_frame = false;
    app->SetDefaultParameter("writer:write_event_frame", default_write_event_frame, "Write parent event frame");
    
    // Event source generator that reads timeslice files output from TimesliceCreator
    // This will read each timeslice file and make the collections available at timeslice level
    app->Add(new MyFileReaderGeneratorEDM4HEP());

    // Event processor that reads timeslices from multiple files and writes merged output
    // The merging happens in the file writer which combines collections from all sources
    app->Add(new MyFileWriterEDM4HEP());

}
} // "C"


