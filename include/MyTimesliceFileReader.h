#pragma once

#include <JANA/JEventSource.h>
#include <JANA/Components/JPodioOutput.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>

#include <podio/ROOTReader.h>
#include <iostream>
#include <random>
#include <numeric>

struct MyTimesliceFileReader : public JEventSource {

    std::string m_tag{""};
    std::string m_filename;
    podio::ROOTReader m_reader;
    size_t m_event_counter = 0;
    size_t m_total_timeslices = 0;
    bool m_loop_forever = false; // Don't loop for merger - process each file once
    std::vector<size_t> timeslice_indices;   

    MyTimesliceFileReader(const std::string& filename) : m_filename(filename) {
        SetTypeName(NAME_OF_THIS);
        SetResourceName(filename);
        SetCallbackStyle(CallbackStyle::ExpertMode);
        m_reader.openFile(m_filename);
        m_total_timeslices = m_reader.getEntries("timeslices");
        timeslice_indices.resize(m_total_timeslices);
    }

    void SetTag(std::string tag) { m_tag = std::move(tag); }
    const std::string& GetTag() const { return m_tag; }

    void Open() override { /* Already opened in constructor */ }
    void Close() override { 
        // m_reader.closeFile(); 
    }

    Result Emit(JEvent& event) override {

        
        const auto& all_event_collection_names = event.GetAllCollectionNames();
        //Print all collection names
        for (const auto& name : all_event_collection_names) {
            std::cout << "Event collection: " << name << std::endl;
        }

        if(m_event_counter >= m_total_timeslices) {
            return Result::FailureFinished;
        }

        auto frame_data = m_reader.readNextEntry("timeslices");
        auto frame      = std::make_unique<podio::Frame>(std::move(frame_data));



        //Print all frame collections
        for (const auto& name : frame->getAvailableCollections()) {
            std::cout << "Frame collection: " << name << std::endl;
        }

        // First, read all available collections from the timeslice frame into event
        for (const auto& coll_name : frame->getAvailableCollections()) {
            const podio::CollectionBase* coll = frame->get(coll_name);
            const auto& coll_type = coll->getValueTypeName();

            //Create new unique collection name by adding the filetag
            std::string unique_coll_name = m_tag + "_" + coll_name;

            std::cout << "Reading collection: " << coll_name << " of type: " << coll_type << std::endl;
            std::cout << "Inserting as: " << unique_coll_name << std::endl;

            if(coll_type == "edm4hep::MCParticle") {
                event.InsertCollectionAlreadyInFrame<edm4hep::MCParticle>(coll, unique_coll_name);
                if(event.GetCollection<edm4hep::MCParticle>(unique_coll_name)) {
                    std::cout << "Successfully inserted collection: " << unique_coll_name << std::endl;
                }
                std::cout << "HI" << std::endl;
                if (std::find(all_event_collection_names.begin(), all_event_collection_names.end(), coll_name) == all_event_collection_names.end()) {
                    std::cout << "Creating empty collection for: " << coll_name << std::endl;
                    auto new_coll = edm4hep::MCParticleCollection();
                    new_coll.setSubsetCollection();

                    event.InsertCollection<edm4hep::MCParticle>(std::move(new_coll), coll_name);
                    std::cout << "Successfully inserted collection: " << coll_name << std::endl;
                }
                std::cout << "Merging collections: " << coll_name << " into " << unique_coll_name << std::endl;
                auto source_collection = event.GetCollection<edm4hep::MCParticle>(coll_name);
                auto merge_collection = const_cast<edm4hep::MCParticleCollection*>(event.GetCollection<edm4hep::MCParticle>(unique_coll_name));
                for(const auto& obj : *source_collection) {
                    merge_collection->push_back(obj);
                }

            }
            else if(coll_type == "edm4hep::EventHeader") {
                event.InsertCollectionAlreadyInFrame<edm4hep::EventHeader>(coll, unique_coll_name);
            }
            else if (coll_type == "edm4hep::SimTrackerHit") {
                event.InsertCollectionAlreadyInFrame<edm4hep::SimTrackerHit>(coll, unique_coll_name);
            }
            else if (coll_type == "edm4hep::SimCalorimeterHit") {
                // Check for corresponding contribution collection
                std::string contribution_name = coll_name + "Contributions";
                std::string unique_contribution_name = m_tag + "_" + contribution_name;
                const podio::CollectionBase* contribution_coll = frame->get(contribution_name);
                if (contribution_coll) {
                    event.InsertCollectionAlreadyInFrame<edm4hep::CaloHitContribution>(contribution_coll, unique_contribution_name);
                    event.InsertCollectionAlreadyInFrame<edm4hep::SimCalorimeterHit>(coll, unique_coll_name);
                }
            }
            else if (coll_type == "edm4hep::CaloHitContribution") {
                // Skip - handled with SimCalorimeterHit
                continue;
            }
            else {
                std::cerr << "Warning: Unhandled collection type '" << coll_type << "' for collection '" << coll_name << "'" << std::endl;
            }
        }

        // NOW: Add merged collections directly to the frame before inserting
        // addMergedCollectionsToFrame(event);

        //Print the name of all collections in the event
        for (const auto& coll_name : event.GetAllCollectionNames()) {
            std::cout << "Event contains collection: " << coll_name << std::endl;
        }

        event.Insert(frame.release());
        m_event_counter++;
        return Result::Success;
    }

private:
    // void addMergedCollectionsToFrame(JEvent& event) {
    //     // Create merged collections from ts_* collections and add them directly to the frame
    //     // This preserves all associations since we're working on the same frame
        
    //     // Maps to store merged collections by suffix
    //     // std::map<std::string, edm4hep::MCParticleCollection> mc_particle_collection_map;
    //     // std::map<std::string, edm4hep::EventHeaderCollection> event_header_collection_map;
    //     // std::map<std::string, edm4hep::SimTrackerHitCollection> sim_tracker_hit_collection_map;
    //     // std::map<std::string, edm4hep::SimCalorimeterHitCollection> sim_calorimeter_hit_collection_map;
    //     // std::map<std::string, edm4hep::CaloHitContributionCollection> calo_hit_contribution_collection_map;
        
    //     // Get all collection names from JANA event and process collections with tag prefix
    //     std::string tag_prefix = m_tag + "_";
    //     for (const auto& coll_name : event.GetAllCollectionNames()) {
            
    //         // Only process collections that have the tag prefix
    //         if (coll_name.substr(0, tag_prefix.length()) != tag_prefix) continue;
            
    //         // Extract the original collection name (remove tag prefix)
    //         std::string original_name = coll_name.substr(tag_prefix.length());
            
    //         std::cout << "Merging " << coll_name << " -> " << original_name << std::endl;
            
    //         try {
    //             auto base_ptr = event.GetCollectionBase(coll_name);
    //             const auto& coll_type = base_ptr->getValueTypeName();
                
    //             // Check if the output collection for this original name exists, if not create it
    //             if (coll_type == "edm4hep::MCParticle") {
    //                 if( !event.GetCollection<edm4hep::MCParticle>(coll_name)) {
    //                     auto new_collection = edm4hep::MCParticleCollection();
    //                     new_collection.setSubsetCollection();
    //                     event.InsertCollection<edm4hep::MCParticle>(new_collection, original_name);
    //                 }
    //                 auto in_coll = event.GetCollection<edm4hep::MCParticle>(coll_name);
    //                 auto& out_coll = event.GetCollection<edm4hep::MCParticle>(original_name);
    //                 for (const auto& obj : *in_coll) {
    //                     out_coll->push_back(obj); // Preserves object references
    //                 }
    //             } else if (coll_type == "edm4hep::EventHeader") {
    //                 if (!event.GetCollection<edm4hep::EventHeader>(original_name)) {
    //                     auto new_collection = edm4hep::EventHeaderCollection();
    //                     new_collection.setSubsetCollection();
    //                     event.InsertCollection<edm4hep::EventHeader>(new_collection, original_name);
    //                 }
    //                 auto in_coll = event.GetCollection<edm4hep::EventHeader>(coll_name);
    //                 auto& out_coll = event.GetCollection<edm4hep::EventHeader>(original_name);
    //                 for (const auto& obj : *in_coll) {
    //                     out_coll->push_back(obj);
    //                 }
    //             } else if (coll_type == "edm4hep::SimTrackerHit") {
    //                 if (!event.GetCollection<edm4hep::SimTrackerHit>(original_name)) {
    //                     auto new_collection = edm4hep::SimTrackerHitCollection();
    //                     new_collection.setSubsetCollection();
    //                     event.InsertCollection<edm4hep::SimTrackerHit>(new_collection, original_name);
    //                 }
    //                 auto in_coll = event.GetCollection<edm4hep::SimTrackerHit>(coll_name);
    //                 auto& out_coll = event.GetCollection<edm4hep::SimTrackerHit>(original_name);
    //                 for (const auto& obj : *in_coll) {
    //                     out_coll->push_back(obj);
    //                 }
    //             } else if (coll_type == "edm4hep::SimCalorimeterHit") {
    //                 if (!event.GetCollection<edm4hep::SimCalorimeterHit>(original_name)) {
    //                     auto new_collection = edm4hep::SimCalorimeterHitCollection();
    //                     new_collection.setSubsetCollection();
    //                     event.InsertCollection<edm4hep::SimCalorimeterHit>(new_collection, original_name);
    //                 }
    //                 auto in_coll = event.GetCollection<edm4hep::SimCalorimeterHit>(coll_name);
    //                 auto& out_coll = event.GetCollection<edm4hep::SimCalorimeterHit>(original_name);
    //                 for (const auto& obj : *in_coll) {
    //                     out_coll->push_back(obj);
    //                 }
    //             } else if (coll_type == "edm4hep::CaloHitContribution") {
    //                 if (!event.GetCollection<edm4hep::CaloHitContribution>(original_name)) {
    //                     auto new_collection = edm4hep::CaloHitContributionCollection();
    //                     new_collection.setSubsetCollection();
    //                     event.InsertCollection<edm4hep::CaloHitContribution>(new_collection, original_name);
    //                 }
    //                 auto in_coll = event.GetCollection<edm4hep::CaloHitContribution>(coll_name);
    //                 auto& out_coll = event.GetCollection<edm4hep::CaloHitContribution>(original_name);
    //                 for (const auto& obj : *in_coll) {
    //                     out_coll->push_back(obj);
    //                 }
    //             } else {
    //                 std::cout << "Skipping unsupported collection type: " << coll_type << std::endl;
    //             }
                
    //         } catch (const std::exception& e) {
    //             std::cout << "Collection " << coll_name << " not found in event, skipping: " << e.what() << std::endl;
    //         }
    //     }
        
    //     // Add all merged collections directly to the frame
    //     // for (auto& [original_name, coll] : mc_particle_collection_map) {
    //     //     std::cout << "Adding merged MCParticles collection to frame: " << original_name << std::endl;
    //     //     frame.put(std::move(coll), original_name);
    //     // }
    //     // for (auto& [original_name, coll] : event_header_collection_map) {
    //     //     std::cout << "Adding merged EventHeader collection to frame: " << original_name << std::endl;
    //     //     frame.put(std::move(coll), original_name);
    //     // }
    //     // for (auto& [original_name, coll] : sim_tracker_hit_collection_map) {
    //     //     std::cout << "Adding merged SimTrackerHit collection to frame: " << original_name << std::endl;
    //     //     frame.put(std::move(coll), original_name);
    //     // }
    //     // for (auto& [original_name, coll] : sim_calorimeter_hit_collection_map) {
    //     //     std::cout << "Adding merged SimCalorimeterHit collection to frame: " << original_name << std::endl;
    //     //     frame.put(std::move(coll), original_name);
    //     // }
    //     // for (auto& [original_name, coll] : calo_hit_contribution_collection_map) {
    //     //     std::cout << "Adding merged CaloHitContribution collection to frame: " << original_name << std::endl;
    //     //     frame.put(std::move(coll), original_name);
    //     // }
    // }
};