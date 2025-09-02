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
    std::vector<std::string> m_collections_to_read {"ts_MCParticles", "ts_info"};    
    std::vector<std::string> m_sim_tracker_hit_collections;
    std::vector<std::string> m_sim_calorimeter_hit_collections;

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

        if(m_event_in_file_counter >= m_total_timeslices) {
            return Result::FailureFinished;
        }

        auto frame_data = m_reader.readNextEntry("timeslices", timeslice_indices);
        auto frame      = std::make_unique<podio::Frame>(std::move(frame_data));

        // Read all available collections from the timeslice frame
        for (const auto& coll_name : frame->getAvailableCollections()) {
            const podio::CollectionBase* coll = frame->get(coll_name);
            const auto& coll_type = coll->getValueTypeName();

            if(coll_type == "edm4hep::MCParticle") {
                event.InsertCollectionAlreadyInFrame<edm4hep::MCParticle>(coll, coll_name);
            }
            else if(coll_type == "edm4hep::EventHeader") {
                event.InsertCollectionAlreadyInFrame<edm4hep::EventHeader>(coll, coll_name);
            }
            else if (coll_type == "edm4hep::SimTrackerHit") {
                event.InsertCollectionAlreadyInFrame<edm4hep::SimTrackerHit>(coll, coll_name);
            }
            else if (coll_type == "edm4hep::SimCalorimeterHit") {
                // Check for corresponding contribution collection
                std::string contribution_name = coll_name + "Contributions";
                const podio::CollectionBase* contribution_coll = frame->get(contribution_name);
                if (contribution_coll) {
                    event.InsertCollectionAlreadyInFrame<edm4hep::CaloHitContribution>(contribution_coll, contribution_name);
                    event.InsertCollectionAlreadyInFrame<edm4hep::SimCalorimeterHit>(coll, coll_name);
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

        event.Insert(frame.release());
        m_event_counter++;
        return Result::Success;
    }
};