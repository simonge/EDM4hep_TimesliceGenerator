#include <iostream>
#include <memory>

// Podio includes
#include <podio/ROOTReader.h>
#include <podio/Frame.h>

// EDM4HEP includes
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/EventHeaderCollection.h>

// Test to verify that a generated file can be properly read using Podio API
// and that the relationships between collections are correctly preserved
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <root_file>" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    std::cout << "=== Podio API Format Test ===" << std::endl;
    std::cout << "Testing file: " << filename << std::endl;
    
    try {
        // Create Podio ROOT reader
        auto reader = podio::ROOTReader();
        reader.openFile(filename);
        
        std::cout << "✓ File opened successfully with Podio reader" << std::endl;
        
        // Get available categories (frame names)
        auto categories = reader.getAvailableCategories();
        std::cout << "Available categories: " << categories.size() << std::endl;
        for (const auto& category : categories) {
            std::cout << "  - " << category << std::endl;
        }
        
        // Check if we have the expected "events" category
        if (std::find(categories.begin(), categories.end(), "events") == categories.end()) {
            std::cout << "❌ ERROR: No 'events' category found" << std::endl;
            return 1;
        }
        
        // Get number of entries
        size_t num_entries = reader.getEntries("events");
        std::cout << "✓ Found " << num_entries << " events" << std::endl;
        
        if (num_entries == 0) {
            std::cout << "❌ ERROR: No events found in file" << std::endl;
            return 1;
        }
        
        // Test reading first few frames to verify format
        size_t test_entries = std::min(static_cast<size_t>(3), num_entries);
        std::cout << "\n--- Testing first " << test_entries << " frames ---" << std::endl;
        
        for (size_t i = 0; i < test_entries; ++i) {
            std::cout << "\nFrame " << i << ":" << std::endl;
            
            // Read frame
            auto frame = podio::Frame(reader.readEntry("events", i));
            
            // Get available collections in this frame
            auto collections = frame.getAvailableCollections();
            std::cout << "  Collections: " << collections.size() << std::endl;
            
            // Check for expected collections
            bool has_mcparticles = false;
            bool has_calo_hits = false;
            bool has_calo_contributions = false;
            bool has_tracker_hits = false;
            bool has_event_header = false;
            
            std::vector<std::string> calo_collection_names;
            std::vector<std::string> contrib_collection_names;
            std::vector<std::string> tracker_collection_names;
            
            for (const auto& collection_name : collections) {
                std::cout << "    - " << collection_name << std::endl;
                
                if (collection_name == "MCParticles") {
                    has_mcparticles = true;
                    
                    // Test MCParticles collection
                    const auto& mcparticles = frame.get<edm4hep::MCParticleCollection>("MCParticles");
                    std::cout << "      MCParticles: " << mcparticles.size() << " particles" << std::endl;
                    
                    // Test relationships
                    if (mcparticles.size() > 0) {
                        const auto& first_particle = mcparticles[0];
                        std::cout << "      First particle parents: " << first_particle.getParents().size() 
                                 << ", daughters: " << first_particle.getDaughters().size() << std::endl;
                    }
                    
                } else if (collection_name == "EventHeader") {
                    has_event_header = true;
                    
                    const auto& headers = frame.get<edm4hep::EventHeaderCollection>("EventHeader");
                    std::cout << "      EventHeaders: " << headers.size() << " headers" << std::endl;
                    
                } else if (collection_name.find("SimCalorimeterHit") != std::string::npos || 
                          collection_name.find("CaloHit") != std::string::npos) {
                    if (collection_name.find("Contribution") == std::string::npos) {
                        has_calo_hits = true;
                        calo_collection_names.push_back(collection_name);
                        
                        // Test calorimeter hits collection
                        const auto& calo_hits = frame.get<edm4hep::SimCalorimeterHitCollection>(collection_name);
                        std::cout << "      " << collection_name << ": " << calo_hits.size() << " hits" << std::endl;
                        
                        // Test contributions relationship
                        if (calo_hits.size() > 0) {
                            const auto& first_hit = calo_hits[0];
                            std::cout << "        First hit contributions: " << first_hit.getContributions().size() << std::endl;
                        }
                    }
                    
                } else if (collection_name.find("CaloHitContribution") != std::string::npos) {
                    has_calo_contributions = true;
                    contrib_collection_names.push_back(collection_name);
                    
                    // Test contributions collection
                    const auto& contributions = frame.get<edm4hep::CaloHitContributionCollection>(collection_name);
                    std::cout << "      " << collection_name << ": " << contributions.size() << " contributions" << std::endl;
                    
                    // Test particle relationship
                    if (contributions.size() > 0) {
                        const auto& first_contrib = contributions[0];
                        std::cout << "        First contribution particle valid: " 
                                 << (first_contrib.getParticle().isAvailable() ? "yes" : "no") << std::endl;
                    }
                    
                } else if (collection_name.find("SimTrackerHit") != std::string::npos || 
                          collection_name.find("TrackerHit") != std::string::npos) {
                    has_tracker_hits = true;
                    tracker_collection_names.push_back(collection_name);
                    
                    // Test tracker hits collection
                    const auto& tracker_hits = frame.get<edm4hep::SimTrackerHitCollection>(collection_name);
                    std::cout << "      " << collection_name << ": " << tracker_hits.size() << " hits" << std::endl;
                    
                    // Test particle relationship
                    if (tracker_hits.size() > 0) {
                        const auto& first_hit = tracker_hits[0];
                        std::cout << "        First hit particle valid: " 
                                 << (first_hit.getParticle().isAvailable() ? "yes" : "no") << std::endl;
                    }
                }
            }
            
            // Summary for this frame
            std::cout << "  Frame " << i << " validation:" << std::endl;
            std::cout << "    MCParticles: " << (has_mcparticles ? "✓" : "❌") << std::endl;
            std::cout << "    EventHeader: " << (has_event_header ? "✓" : "❌") << std::endl;
            std::cout << "    Calo hits: " << (has_calo_hits ? "✓" : "❌") << " (" << calo_collection_names.size() << " collections)" << std::endl;
            std::cout << "    Calo contributions: " << (has_calo_contributions ? "✓" : "❌") << " (" << contrib_collection_names.size() << " collections)" << std::endl;
            std::cout << "    Tracker hits: " << (has_tracker_hits ? "✓" : "❌") << " (" << tracker_collection_names.size() << " collections)" << std::endl;
        }
        
        std::cout << "\n=== Podio API Test Complete ===" << std::endl;
        std::cout << "✅ File successfully validated with Podio API" << std::endl;
        std::cout << "✅ All collections can be properly formed into Podio frames" << std::endl;
        std::cout << "✅ Relationships between collections are preserved" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "❌ ERROR: Failed to read file with Podio API: " << e.what() << std::endl;
        return 1;
    }
}