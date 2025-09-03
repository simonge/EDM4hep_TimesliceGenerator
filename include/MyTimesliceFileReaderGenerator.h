#include <JANA/JEventSourceGenerator.h>
#include "MyTimesliceFileReader.h"
#include <filesystem>

class MyTimesliceFileReaderGenerator : public JEventSourceGenerator {

    JEventSource* MakeJEventSource(std::string resource_name) override {

        auto source = new MyTimesliceFileReader(resource_name);

        // Get the file basename to use as a tag 
        std::filesystem::path p(resource_name);
        // Get the filename without extension
        std::string tag = p.stem().string();
        source->SetTag(tag);


        std::cout << "MyTimesliceFileReaderGenerator: Created JEventSource for timeslice file " << resource_name << " with tag " << tag << std::endl;
        return source;
    }

    double CheckOpenable(std::string resource_name) override {
        // Check if this is a ROOT file that likely contains timeslices
        if (resource_name.find(".root") != std::string::npos) {
            // Give it a higher priority than regular event files for timeslice files
            if (resource_name.find("timeslice") != std::string::npos || 
                resource_name.find("ts_") != std::string::npos) {
                return 0.1;
            }
            return 0.02;
        }
        else {
            return 0;
        }
    }
};