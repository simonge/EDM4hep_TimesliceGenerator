
#include <JANA/JEventSourceGenerator.h>
#include "MyEventFileReader.h"
#include <filesystem>

class MyEventFileReaderGenerator : public JEventSourceGenerator {

    JEventSource* MakeJEventSource(std::string resource_name) override {

        auto source = new MyEventFileReader(resource_name);
        // source->SetResourceName(resource_name);

        // Get the file basename to use as a tag 
        // std::filesystem::path p(resource_name);
        // // Get the filename without extension
        std::string tag = "";//p.stem().string(); // "myfile"
        source->SetTag(tag);

        source->SetLevel(JEventLevel::PhysicsEvent);
        
        // Configure collections to include in timeframes
        // Example configuration - this would typically come from command-line parameters or config files
        std::vector<std::string> sim_tracker_hit_collections = {"SimTrackerHits"};  // Add more as needed
        std::vector<std::string> reconstructed_particle_collections = {"ReconstructedParticles"};  // Add more as needed
        
        source->SetSimTrackerHitCollections(sim_tracker_hit_collections);
        source->SetReconstructedParticleCollections(reconstructed_particle_collections);

        // if(tag=="det1") source->SetLevel(JEventLevel::PhysicsEvent);
        // if(tag=="det2") source->SetLevel(JEventLevel::Subevent);
        std::cout << "MyEventFileReaderGenerator: Created JEventSource for file " << resource_name << " with tag " << tag << std::endl;
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


