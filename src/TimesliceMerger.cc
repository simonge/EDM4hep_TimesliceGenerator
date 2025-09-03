// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.


#include "MyTimesliceFileReaderGenerator.h"
#include "MyEventFileWriter.h"
#include "MyTimesliceFileWriter.h"
#include "FrameMergerFactory.h"

#include <JANA/Components/JOmniFactoryGeneratorT.h>

#include <JANA/JApplication.h>


extern "C"{
void InitPlugin(JApplication *app) {

    InitJANAPlugin(app);
    size_t default_nevents = 100;
    app->SetDefaultParameter("writer:nevents", default_nevents, "Default number of events to write");

    bool default_write_event_frame = false;
    app->SetDefaultParameter("writer:write_event_frame", default_write_event_frame, "Write parent event frame");

    std::set<std::string> input_files = {"oiutput.root"};
    app->SetDefaultParameter("reader:input_files", input_files,
                             "Comma separated list of files to merge");

    // Event source generator that reads timeslice files output from TimesliceCreator
    // This will read each timeslice file and make the collections available at timeslice level
    app->Add(new MyTimesliceFileReaderGenerator());



    // Create merger factories to combine collections from all timeslice sources
    // DISABLED: Moving merging logic directly into MyEventFileWriter to avoid dependency cycles
    // app->Add(new JOmniFactoryGeneratorT<FrameMergerFactory>());

    // Event processor that reads timeslices from multiple files and writes merged output
    // The merging happens in the file writer which combines collections from all sources
    app->Add(new MyEventFileWriter());
    // app->Add(new MyTimesliceFileWriter());

}
} // "C"


