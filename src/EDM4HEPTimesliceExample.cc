// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.


#include "MyFileReaderGeneratorEDM4HEP.h"
#include "MyFileWriterEDM4HEP.h"
#include "MyTimesliceBuilderEDM4HEP.h"

#include <JANA/Components/JOmniFactoryGeneratorT.h>

#include <JANA/JApplication.h>


extern "C"{
void InitPlugin(JApplication *app) {

    InitJANAPlugin(app);

    // Event source generator instantiates a FileReader for each filename passed to jana.
    // The event source it produces is configured to either produce Timeslices or Events.
    // Either way, these files contain just hits
    app->Add(new MyFileReaderGeneratorEDM4HEP());


    // Unfolder that takes timeslices and splits them into physics events.
    app->Add(new MyTimesliceBuilderEDM4HEP());

     


    // Collection Collector for the output...


    // Event processor that writes and timeslices to file
    app->Add(new MyFileWriterEDM4HEP());

}
} // "C"


