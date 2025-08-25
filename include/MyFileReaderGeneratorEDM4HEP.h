
#include <JANA/JEventSourceGenerator.h>
#include "MyFileReaderEDM4HEP.h"


class MyFileReaderGeneratorEDM4HEP : public JEventSourceGenerator {

    JEventSource* MakeJEventSource(std::string resource_name) override {

    auto source = new MyFileReaderEDM4HEP;
        source->SetResourceName(resource_name);
        source->SetLevel(JEventLevel::PhysicsEvent);
        return source;
    }

    double CheckOpenable(std::string resource_name) override {
        // In theory, we should check whether PODIO can open the file and 
        // whether it contains an 'events' or 'timeslices' tree. If not, return 0.
        if (resource_name.find(".root") != std::string::npos) {
            return 0.01;
        }
        else {
            return 0;
        }
    }
};


